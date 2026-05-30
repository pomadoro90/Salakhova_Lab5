import sys
import time
import threading
from datetime import datetime
from Message import SalakhovaClient, parse_client_list


def main():
    host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 12345
    name = sys.argv[3] if len(sys.argv) > 3 else "PyClient"

    client = SalakhovaClient()
    try:
        client.connect(host, port, name)
    except Exception as e:
        print(f"[ERROR] Connection failed: {e}")
        sys.exit(1)

    print(f"Connected as Client #{client.client_id}")

    running = True

    def printer_loop():
        nonlocal running
        while running and client.alive:
            msg = client.poll()
            if msg:
                from_id, msg_type, to, payload = msg
                if msg_type == client.MT_DATA:
                    ts = datetime.now().strftime('%H:%M:%S')
                    print(f"\n[{ts}] Client #{from_id}: {payload}")
                    print("> ", end="", flush=True)
                elif msg_type == client.MT_CONFIRM:
                    client.clients = parse_client_list(payload)
            else:
                time.sleep(0.01)

    def poller_loop():
        nonlocal running
        while running and client.alive:
            try:
                client._send_message(client.ADDR_SERVER, client.MT_GETDATA)
            except:
                pass
            time.sleep(0.5)

    def ping_loop():
        nonlocal running
        while running and client.alive:
            try:
                client._send_message(client.ADDR_SERVER, client.MT_INFO)
            except:
                pass
            time.sleep(10)

    threading.Thread(target=printer_loop, daemon=True).start()
    threading.Thread(target=poller_loop, daemon=True).start()
    threading.Thread(target=ping_loop, daemon=True).start()

    print("Commands: /list, /send <id|all> <text>, /quit")
    try:
        while running and client.alive:
            try:
                line = input("> ").strip()
            except (EOFError, KeyboardInterrupt):
                print()
                break
            if not line:
                continue
            if line == "/quit":
                break
            elif line == "/list":
                if client.clients:
                    for cid, cname in client.clients.items():
                        marker = " (you)" if cid == client.client_id else ""
                        print(f"  #{cid}: {cname}{marker}")
                else:
                    print("  (no clients)")
            elif line.startswith("/send "):
                rest = line[6:].strip()
                parts = rest.split(' ', 1)
                if len(parts) < 2 or not parts[1]:
                    print("Usage: /send <id|all> <text>")
                    continue
                target = parts[0]
                text = parts[1]
                try:
                    target_id = client.ADDR_BROADCAST if target == "all" else int(target)
                    client._send_message(target_id, client.MT_DATA, text)
                except (ValueError, Exception) as e:
                    print(f"[ERROR] {e}")
            else:
                print("Unknown command. Use /list, /send <id|all> <text>, /quit")
    finally:
        running = False
        client.disconnect()
        time.sleep(0.1)


if __name__ == '__main__':
    main()
