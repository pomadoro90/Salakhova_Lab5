using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

namespace Salakhova_Sharp
{
    // Перечисления типов сообщений из файлов преподавателя
    public enum MessageTypes : int
    {
        MT_INIT = 0,
        MT_EXIT = 1,
        MT_GETDATA = 2,
        MT_DATA = 3,
        MT_NODATA = 4,
        MT_CONFIRM = 6 // В твоем сервере MT_CONFIRM равен 6
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct MessageHeader
    {
        [MarshalAs(UnmanagedType.I4)] public MessageTypes type;
        [MarshalAs(UnmanagedType.I4)] public int size;
        [MarshalAs(UnmanagedType.I4)] public int to;
        [MarshalAs(UnmanagedType.I4)] public int from;
    }

    public class SalakhovaNetworkClient
    {
        private Socket _socket;
        private bool _isConnected;
        private int _clientId = -1;
        private Thread _readerThread;

        private readonly object _writeLock = new object();
        private readonly object _inboxLock = new object();

        // Локальная очередь сообщений для пуллинга из UI
        private readonly Queue<(int fromId, MessageTypes type, string text)> _inbox =
            new Queue<(int, MessageTypes, string)>();

        public bool IsConnected => _isConnected;
        public int ClientId => _clientId;

        // Помощники преподавателя для работы с бинарными структурами
        private static byte[] StructureToBytes(object obj)
        {
            int size = Marshal.SizeOf(obj);
            byte[] buff = new byte[size];
            IntPtr ptr = Marshal.AllocHGlobal(size);
            Marshal.StructureToPtr(obj, ptr, true);
            Marshal.Copy(ptr, buff, 0, size);
            Marshal.FreeHGlobal(ptr);
            return buff;
        }

        private static T BytesToStructure<T>(byte[] buff) where T : struct
        {
            int size = Marshal.SizeOf(typeof(T));
            IntPtr ptr = Marshal.AllocHGlobal(size);
            Marshal.Copy(buff, 0, ptr, size);
            T obj = (T)Marshal.PtrToStructure(ptr, typeof(T));
            Marshal.FreeHGlobal(ptr);
            return obj;
        }

        public bool Connect(string host, int port, string clientName)
        {
            if (_isConnected) return true;

            try
            {
                _socket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
                _socket.Connect(new IPEndPoint(IPAddress.Parse(host), port));

                // 1. Отправляем MT_INIT на сервер (to = -2 (Broker), size = длина строки * 2)
                byte[] payload = Encoding.Unicode.GetBytes(clientName);
                MessageHeader header = new MessageHeader
                {
                    type = MessageTypes.MT_INIT,
                    size = payload.Length,
                    to = -2,
                    from = 0
                };

                _socket.Send(StructureToBytes(header));
                if (payload.Length > 0)
                {
                    _socket.Send(payload);
                }

                // В твоем сервере при подключении clientId выдается автоматически,
                // а список клиентов рассылается широковещательно (Broadcast) как MT_CONFIRM.
                _isConnected = true;

                // 2. Запускаем фоновый поток чтения сокета
                _readerThread = new Thread(ReaderLoop) { IsBackground = true };
                _readerThread.Start();

                return true;
            }
            catch
            {
                Disconnect();
                return false;
            }
        }

        public void Disconnect()
        {
            if (!_isConnected) return;

            try
            {
                // Отправляем MT_QUIT (в твоей системе это тип сообщения для выхода)
                // Или MT_EXIT из перечисления преподавателя. По коду твоего сервера обрабатывается MT_QUIT (4).
                Send(-2, (MessageTypes)4, "");
            }
            catch { }

            _isConnected = false;

            if (_socket != null)
            {
                try { _socket.Shutdown(SocketShutdown.Both); } catch { }
                try { _socket.Close(); } catch { }
                _socket = null;
            }

            _readerThread = null;
        }

        public void Send(int targetId, MessageTypes type, string text)
        {
            if (!_isConnected || _socket == null) return;

            try
            {
                byte[] payload = Encoding.Unicode.GetBytes(text ?? "");
                MessageHeader header = new MessageHeader
                {
                    type = type,
                    size = payload.Length,
                    to = targetId,
                    from = _clientId
                };

                lock (_writeLock)
                {
                    _socket.Send(StructureToBytes(header));
                    if (payload.Length > 0)
                    {
                        _socket.Send(payload);
                    }
                }
            }
            catch
            {
                _isConnected = false;
            }
        }

        // Метод извлечения данных из локальной очереди (внутренний пуллинг формы)
        public bool Poll(out int fromId, out MessageTypes type, out string text)
        {
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

        private void ReaderLoop()
        {
            int headerSize = Marshal.SizeOf(typeof(MessageHeader));
            byte[] headerBuffer = new byte[headerSize];

            while (_isConnected)
            {
                try
                {
                    // Читаем заголовок строго полностью
                    int readBytes = ReadExact(headerBuffer, headerSize);
                    if (readBytes == 0) break;

                    MessageHeader header = BytesToStructure<MessageHeader>(headerBuffer);

                    // Если сервер прислал нам подтверждение ID (например, при MT_INIT)
                    if (header.type == MessageTypes.MT_INIT)
                    {
                        _clientId = header.to; // Сервер записывает выданный ID в поле 'to'
                    }

                    // Если пришло уведомление о том, что сообщений на сервере больше нет
                    if (header.type == MessageTypes.MT_NODATA)
                    {
                        continue;
                    }

                    string text = "";
                    if (header.size > 0)
                    {
                        byte[] payloadBuffer = new byte[header.size];
                        ReadExact(payloadBuffer, header.size);
                        text = Encoding.Unicode.GetString(payloadBuffer);
                    }

                    // Добавляем сообщение во внутреннюю потокобезопасную очередь
                    lock (_inboxLock)
                    {
                        _inbox.Enqueue((header.from, header.type, text));
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

        private int ReadExact(byte[] buffer, int size)
        {
            int totalRead = 0;
            while (totalRead < size)
            {
                int read = _socket.Receive(buffer, totalRead, size - totalRead, SocketFlags.None);
                if (read == 0) return 0; // Соединение разорвано
                totalRead += read;
            }
            return totalRead;
        }
    }
}