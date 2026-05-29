import socket
import struct
import threading
import collections
import time


class SalakhovaClient:
    """Python client for Salakhova message server (binary protocol).

    Protocol: 16-byte LE header [messageType, size, to, from]
    The server is a pull-based broker: messages are queued per session,
    and the client must send MT_GETDATA to receive them.
    """

    # Message types (must match C++ server enum)
    MT_INIT = 0
    MT_EXIT = 1
    MT_GETDATA = 2
    MT_DATA = 3
    MT_NODATA = 4
    MT_INFO = 5
    MT_CONFIRM = 6
    MT_CLOSE = 7
    MT_QUIT = 8

    # Special addresses
    ADDR_BROADCAST = -1
    ADDR_SERVER = -2

    HEADER_FMT = '<iiii'  # messageType, size, to, from
    HEADER_SIZE = 16

    def __init__(self):
        self.sock = None
        self.client_id = 0
        self.alive = False
        self.inbox = collections.deque()
        self.inbox_lock = threading.Lock()
        self.send_lock = threading.Lock()
        self.clients = {}  # id -> name

    def connect(self, host, port, name):
        """TCP connect, send MT_INIT, then poll MT_GETDATA until we receive client ID."""
        self.sock = socket.create_connection((host, port))
        self.alive = True

        # Send MT_INIT (server auto-creates session, but we inform it of our name)
        self._send_message(self.ADDR_SERVER, self.MT_INIT, name, from_id=0)

        # Start background reader thread
        threading.Thread(target=self._reader_loop, daemon=True).start()

        # Server queues MT_CONFIRM with our ID — we must GETDATA to receive it.
        # Poll for up to 5 seconds.
        self.client_id = -1
        deadline = time.time() + 5

        while self.client_id == -1 and self.alive and time.time() < deadline:
            try:
                self._send_message(self.ADDR_SERVER, self.MT_GETDATA)
            except Exception:
                break

            # Check inbox for MT_CONFIRM with numeric ID
            self._try_drain_client_id()

            if self.client_id == -1:
                time.sleep(0.05)

        if self.client_id == -1:
            self.client_id = 0  # Fallback

    def _try_drain_client_id(self):
        """Look for MT_CONFIRM with numeric client ID in inbox."""
        with self.inbox_lock:
            keep = collections.deque()
            while self.inbox:
                msg = self.inbox.popleft()
                from_id, msg_type, to, payload = msg
                if msg_type == self.MT_CONFIRM and self.client_id == -1:
                    try:
                        cid = int(payload)
                        if cid > 0:
                            self.client_id = cid
                            continue  # consumed — don't put back
                    except ValueError:
                        pass
                keep.append(msg)
            # Put remaining messages back
            self.inbox.clear()
            self.inbox.extend(keep)

    def _send_message(self, to, msg_type, text="", from_id=None):
        """Pack and send a message over the socket."""
        if from_id is None:
            from_id = self.client_id if self.client_id > 0 else 0
        payload = text.encode('utf-16-le') if text else b''
        header = struct.pack(self.HEADER_FMT, msg_type, len(payload), to, from_id)
        with self.send_lock:
            self.sock.sendall(header + payload)

    def _reader_loop(self):
        """Background thread — reads all incoming messages from socket."""
        while self.alive:
            try:
                header = self._recv_exact(self.HEADER_SIZE)
                msg_type, size, to, from_id = struct.unpack(self.HEADER_FMT, header)
                if msg_type == self.MT_NODATA:
                    continue
                if msg_type == self.MT_CLOSE:
                    self.alive = False
                    break
                payload = ""
                if size > 0:
                    raw = self._recv_exact(size)
                    payload = raw.decode('utf-16-le')
                with self.inbox_lock:
                    self.inbox.append((from_id, msg_type, to, payload))
            except Exception:
                self.alive = False
                break

    def _recv_exact(self, n):
        """Read exactly n bytes from socket."""
        buf = bytearray(n)
        view = memoryview(buf)
        while view:
            chunk = self.sock.recv_into(view)
            if not chunk:
                raise ConnectionError("Server disconnected")
            view = view[chunk:]
        return bytes(buf)

    def poll(self):
        """Non-blocking: return one message from inbox, or None."""
        with self.inbox_lock:
            if self.inbox:
                return self.inbox.popleft()
        return None

    def disconnect(self):
        """Send MT_QUIT and close socket."""
        try:
            self._send_message(self.ADDR_SERVER, self.MT_QUIT)
        except Exception:
            pass
        self.alive = False
        try:
            self.sock.close()
        except Exception:
            pass


def parse_client_list(data):
    """Parse server client list format 'id:name;id:name;...' into dict."""
    clients = {}
    if not data:
        return clients
    for pair in data.split(';'):
        if ':' in pair:
            parts = pair.split(':', 1)
            try:
                clients[int(parts[0])] = parts[1]
            except (ValueError, IndexError):
                pass
    return clients