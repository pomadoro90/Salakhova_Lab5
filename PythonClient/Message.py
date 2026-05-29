import socket, struct
from dataclasses import dataclass

# Константы типов сообщений твоего C++ сервера
MT_CLOSE   = 0
MT_DATA    = 1
MT_START   = 2
MT_STOP    = 3
MT_QUIT    = 4
MT_INFO    = 5
MT_CONFIRM = 6

# Адресаты
MR_BROKER  = -2
MR_ALL     = -1

@dataclass
class MsgHeader:
    Type: int = 0
    Size: int = 0
    To:   int = 0
    From: int = 0

    def Send(self, s):
        # Используем '<iiii' (little-endian) для точного совпадения с C++ структурами
        s.send(struct.pack('<iiii', self.Type, self.Size, self.To, self.From))

    def Receive(self, s):
        try:
            header_data = s.recv(16)
            if not header_data or len(header_data) < 16:
                raise Exception("Socket closed")
            (self.Type, self.Size, self.To, self.From) = struct.unpack('<iiii', header_data)
        except:
            self.Size = 0
            self.Type = MT_CLOSE

class Message:
    ClientID = 0
    # Глобальный сокет, чтобы не переподключаться при каждом сообщении
    Connection = None

    def __init__(self, To=0, From=0, Type=MT_DATA, Data=""):
        self.Data = Data
        # Длина строки умножается на 2, так как кодировка utf-16 (аналог wchar_t)
        self.Header = MsgHeader(Type, len(Data) * 2, To, From)

    def Send(self, s):
        self.Header.Send(s)
        if self.Header.Size > 0:
            s.send(self.Data.encode('utf-16-le'))

    def Receive(self, s):
        self.Header.Receive(s)
        if self.Header.Size > 0:
            # Распаковываем сырые байты в строку
            raw_data = struct.unpack(f'<{self.Header.Size}s', s.recv(self.Header.Size))[0]
            self.Data = raw_data.decode('utf-16-le')

    @staticmethod
    def Connect(HOST='127.0.0.1', PORT=12345):
        Message.Connection = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        Message.Connection.connect((HOST, PORT))

    @staticmethod
    def SendMessage(To, Type=MT_DATA, Data=""):
        if Message.Connection is None:
            Message.Connect()
        m = Message(To, Message.ClientID, Type, Data)
        m.Send(Message.Connection)
        return m