using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;

namespace Salakhova_Sharp
{
    public enum MessageTypes : int
    {
        MT_INIT = 0,
        MT_EXIT = 1,
        MT_GETDATA = 2,
        MT_DATA = 3,
        MT_NODATA = 4,
        MT_INFO = 5,
        MT_CONFIRM = 6,
        MT_CLOSE = 7,
        MT_QUIT = 8
    }

    public static class MessageConstants
    {
        public const int ADDR_BROADCAST = -1;
        public const int ADDR_SERVER = -2;
        public const int HeaderSize = 16;
        public const int PingIntervalMs = 10000;
    }

    public class SalakhovaNetworkClient
    {
        private Socket _socket;
        private bool _isConnected;
        private int _clientId = -1;
        private Thread _readerThread;
        private Timer _pingTimer;

        private readonly object _writeLock = new object();
        private readonly object _inboxLock = new object();

        // Локальная очередь сообщений для пуллинга из UI
        private readonly Queue<(int fromId, MessageTypes type, string text)> _inbox =
            new Queue<(int, MessageTypes, string)>();

        public bool IsConnected => _isConnected;
        public int ClientId => _clientId;

        // ── Сериализация ────────────────────────────────────────────────

        private static byte[] PackHeader(int messageType, int size, int to, int from)
        {
            byte[] buf = new byte[MessageConstants.HeaderSize];
            Buffer.BlockCopy(BitConverter.GetBytes(messageType), 0, buf, 0, 4);
            Buffer.BlockCopy(BitConverter.GetBytes(size), 0, buf, 4, 4);
            Buffer.BlockCopy(BitConverter.GetBytes(to), 0, buf, 8, 4);
            Buffer.BlockCopy(BitConverter.GetBytes(from), 0, buf, 12, 4);
            return buf;
        }

        private static (int messageType, int size, int to, int from) UnpackHeader(byte[] buf)
        {
            return (
                BitConverter.ToInt32(buf, 0),
                BitConverter.ToInt32(buf, 4),
                BitConverter.ToInt32(buf, 8),
                BitConverter.ToInt32(buf, 12)
            );
        }

        // ── Connect ──────────────────────────────────────────────────────

        public bool Connect(string host, int port, string clientName)
        {
            if (_isConnected) return true;

            try
            {
                _socket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
                _socket.Connect(new IPEndPoint(IPAddress.Parse(host), port));

                // Сервер Салаховой автоматически создаёт сессию при подключении.
                // Отправляем MT_INIT, чтобы сервер знал имя клиента.
                byte[] payload = Encoding.Unicode.GetBytes(clientName ?? "");
                byte[] header = PackHeader(
                    (int)MessageTypes.MT_INIT,
                    payload.Length,
                    MessageConstants.ADDR_SERVER,
                    0
                );

                lock (_writeLock)
                {
                    _socket.Send(header);
                    if (payload.Length > 0)
                    {
                        _socket.Send(payload);
                    }
                }

                _isConnected = true;

                // Запускаем фоновый поток чтения
                _readerThread = new Thread(ReaderLoop) { IsBackground = true };
                _readerThread.Start();

                // Запускаем keep-alive пинг (MT_INFO каждые 10 сек)
                _pingTimer = new Timer(PingCallback, null,
                    MessageConstants.PingIntervalMs, MessageConstants.PingIntervalMs);

                // Ждём получения clientId через MT_CONFIRM (до 5 сек).
                // Сервер кладёт подтверждение в очередь сессии,
                // читаем его через GETDATA polling.
                _clientId = -1;
                DateTime deadline = DateTime.Now.AddSeconds(5);
                while (_clientId == -1 && DateTime.Now < deadline)
                {
                    // Шлём GETDATA чтобы сервер переслал сообщения из очереди
                    SendInternal(MessageConstants.ADDR_SERVER, MessageTypes.MT_GETDATA, "");

                    // Проверяем inbox
                    TryDrainClientId();

                    if (_clientId == -1)
                        Thread.Sleep(50);
                }

                if (_clientId == -1)
                {
                    // Не получили ID — всё равно продолжаем, ID не обязателен для работы
                    _clientId = 0;
                }

                return true;
            }
            catch
            {
                Disconnect();
                return false;
            }
        }

        /// <summary>
        /// Проверяет inbox на наличие MT_CONFIRM с clientId.
        /// Сервер шлёт MT_CONFIRM с текстом = строковое представление ID.
        /// </summary>
        private void TryDrainClientId()
        {
            lock (_inboxLock)
            {
                var keep = new Queue<(int, MessageTypes, string)>();
                while (_inbox.Count > 0)
                {
                    var msg = _inbox.Dequeue();
                    if (msg.type == MessageTypes.MT_CONFIRM && _clientId == -1)
                    {
                        // Первое MT_CONFIRM содержит наш ID как число
                        if (int.TryParse(msg.text, out int id) && id > 0)
                        {
                            _clientId = id;
                        }
                        else
                        {
                            keep.Enqueue(msg); // Это клиент-лист, не ID
                        }
                    }
                    else
                    {
                        keep.Enqueue(msg);
                    }
                }
                // Вернуть невостребованные сообщения обратно
                while (keep.Count > 0)
                {
                    _inbox.Enqueue(keep.Dequeue());
                }
            }
        }

        // ── Disconnect ──────────────────────────────────────────────────────

        public void Disconnect()
        {
            _isConnected = false;

            if (_pingTimer != null)
            {
                try { _pingTimer.Dispose(); } catch { }
                _pingTimer = null;
            }

            try
            {
                SendInternal(MessageConstants.ADDR_SERVER, MessageTypes.MT_QUIT, "");
            }
            catch { }

            if (_socket != null)
            {
                try { _socket.Shutdown(SocketShutdown.Both); } catch { }
                try { _socket.Close(); } catch { }
                _socket = null;
            }

            _readerThread = null;
        }

        // ── Send ──────────────────────────────────────────────────────────

        public void Send(int to, MessageTypes messageType, string text)
        {
            if (!_isConnected || _socket == null) return;
            try
            {
                SendInternal(to, messageType, text);
            }
            catch
            {
                _isConnected = false;
            }
        }

        private void SendInternal(int to, MessageTypes messageType, string text)
        {
            byte[] payload = Encoding.Unicode.GetBytes(text ?? "");
            byte[] header = PackHeader(
                (int)messageType,
                payload.Length,
                to,
                _clientId > 0 ? _clientId : 0
            );

            lock (_writeLock)
            {
                _socket.Send(header);
                if (payload.Length > 0)
                {
                    _socket.Send(payload);
                }
            }
        }

        // ── Poll ────────────────────────────────────────────────────────────

        /// <summary>
        /// Достаёт одно сообщение из inbox (неблокирующий).
        /// Автоматически шлёт MT_GETDATA для запроса новых данных.
        /// </summary>
        public bool Poll(out int fromId, out MessageTypes type, out string text)
        {
            // Отправляем GETDATA чтобы сервер переслал накопленные сообщения
            if (_isConnected)
            {
                try
                {
                    SendInternal(MessageConstants.ADDR_SERVER, MessageTypes.MT_GETDATA, "");
                }
                catch { }
            }

            lock (_inboxLock)
            {
                if (_inbox.Count > 0)
                {
                    var msg = _inbox.Dequeue();
                    fromId = msg.fromId;
                    type = msg.type;
                    text = msg.text;
                    return true;
                }
            }

            fromId = 0;
            type = MessageTypes.MT_NODATA;
            text = null;
            return false;
        }

        // ── Reader loop (background thread) ─────────────────────────────────

        private void ReaderLoop()
        {
            byte[] headerBuffer = new byte[MessageConstants.HeaderSize];

            while (_isConnected)
            {
                try
                {
                    int read = ReadExact(headerBuffer, MessageConstants.HeaderSize);
                    if (read == 0) break;

                    var (msgType, dataSize, msgTo, msgFrom) = UnpackHeader(headerBuffer);

                    // Получили ID — сохраним
                    if ((MessageTypes)msgType == MessageTypes.MT_CONFIRM && _clientId == -1)
                    {
                        // Будет обработан в TryDrainClientId
                    }

                    // Пропускаем пустые ответы (MT_NODATA)
                    if ((MessageTypes)msgType == MessageTypes.MT_NODATA)
                    {
                        continue;
                    }

                    // Сервер закрывает соединение
                    if ((MessageTypes)msgType == MessageTypes.MT_CLOSE)
                    {
                        _isConnected = false;
                        break;
                    }

                    // Читаем payload если есть
                    string text = "";
                    if (dataSize > 0)
                    {
                        byte[] payloadBuffer = new byte[dataSize];
                        ReadExact(payloadBuffer, dataSize);
                        text = Encoding.Unicode.GetString(payloadBuffer);
                    }

                    // Добавляем во входящую очередь
                    lock (_inboxLock)
                    {
                        _inbox.Enqueue((msgFrom, (MessageTypes)msgType, text));
                    }
                }
                catch
                {
                    _isConnected = false;
                    break;
                }
            }

            _isConnected = false;
        }

        // ── Keep-alive ping ─────────────────────────────────────────────────

        private void PingCallback(object state)
        {
            if (_isConnected)
            {
                try
                {
                    SendInternal(MessageConstants.ADDR_SERVER, MessageTypes.MT_INFO, "");
                }
                catch { }
            }
        }

        // ── Helper: read exact N bytes ──────────────────────────────────────

        private int ReadExact(byte[] buffer, int size)
        {
            int totalRead = 0;
            while (totalRead < size)
            {
                int read = _socket.Receive(buffer, totalRead, size - totalRead, SocketFlags.None);
                if (read == 0) return 0;
                totalRead += read;
            }
            return totalRead;
        }
    }
}