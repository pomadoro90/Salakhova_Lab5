#!/usr/bin/env python3
"""Tkinter GUI client for Salakhova message server."""

import tkinter as tk
from tkinter import ttk, messagebox
import threading
import time

from Message import SalakhovaClient, parse_client_list


class SalakhovaGuiClient:
    """Tkinter-based chat GUI wrapping SalakhovaClient."""

    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("Salakhova Chat")
        self.root.minsize(500, 350)

        self.client: SalakhovaClient | None = None
        self.poller_thread: threading.Thread | None = None
        self.ping_thread: threading.Thread | None = None
        self._running = False

        self._build_ui()

    # ------------------------------------------------------------------ UI
    def _build_ui(self):
        # --- Top row: connection controls ---
        top = ttk.Frame(self.root, padding=4)
        top.pack(fill=tk.X)

        ttk.Label(top, text="Host:").pack(side=tk.LEFT)
        self.host_var = tk.StringVar(value="127.0.0.1")
        ttk.Entry(top, textvariable=self.host_var, width=14).pack(side=tk.LEFT, padx=2)

        ttk.Label(top, text="Port:").pack(side=tk.LEFT)
        self.port_var = tk.StringVar(value="12345")
        ttk.Entry(top, textvariable=self.port_var, width=6).pack(side=tk.LEFT, padx=2)

        ttk.Label(top, text="Name:").pack(side=tk.LEFT)
        self.name_var = tk.StringVar(value="PyClient")
        ttk.Entry(top, textvariable=self.name_var, width=10).pack(side=tk.LEFT, padx=2)

        self.btn_connect = ttk.Button(top, text="Connect", command=self._on_connect)
        self.btn_connect.pack(side=tk.LEFT, padx=4)

        self.btn_disconnect = ttk.Button(top, text="Disconnect", command=self._on_disconnect, state=tk.DISABLED)
        self.btn_disconnect.pack(side=tk.LEFT, padx=2)

        # --- Middle: message area ---
        mid = ttk.Frame(self.root, padding=4)
        mid.pack(fill=tk.BOTH, expand=True)

        self.msg_text = tk.Text(mid, state=tk.DISABLED, wrap=tk.WORD)
        scrollbar = ttk.Scrollbar(mid, orient=tk.VERTICAL, command=self.msg_text.yview)
        self.msg_text.configure(yscrollcommand=scrollbar.set)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        self.msg_text.pack(fill=tk.BOTH, expand=True)

        # --- Bottom row: send controls ---
        bot = ttk.Frame(self.root, padding=4)
        bot.pack(fill=tk.X)

        ttk.Label(bot, text="To:").pack(side=tk.LEFT)
        self.recipient_var = tk.StringVar()
        self.recipient_combo = ttk.Combobox(bot, textvariable=self.recipient_var, width=14, state="readonly")
        self.recipient_combo.pack(side=tk.LEFT, padx=2)

        ttk.Label(bot, text="Msg:").pack(side=tk.LEFT)
        self.msg_var = tk.StringVar()
        self.msg_entry = ttk.Entry(bot, textvariable=self.msg_var)
        self.msg_entry.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=2)
        self.msg_entry.bind("<Return>", lambda _: self._on_send())

        self.btn_send = ttk.Button(bot, text="Send", command=self._on_send)
        self.btn_send.pack(side=tk.LEFT, padx=2)

        # --- Status bar ---
        self.status_var = tk.StringVar(value="Disconnected")
        ttk.Label(self.root, textvariable=self.status_var, relief=tk.SUNKEN, anchor=tk.W, padding=2).pack(fill=tk.X)

    # ----------------------------------------------------------- connect / disconnect
    def _on_connect(self):
        host = self.host_var.get().strip()
        try:
            port = int(self.port_var.get().strip())
        except ValueError:
            messagebox.showerror("Error", "Port must be a number")
            return
        name = self.name_var.get().strip() or "PyClient"

        self.client = SalakhovaClient()
        try:
            self.client.connect(host, port, name)
        except Exception as exc:
            messagebox.showerror("Connection Error", str(exc))
            self.client = None
            return

        if self.client.client_id == 0:
            messagebox.showwarning("Warning", "Did not receive client ID from server")

        self._running = True

        # Start background threads
        self.poller_thread = threading.Thread(target=self._poller_loop, daemon=True)
        self.ping_thread = threading.Thread(target=self._ping_loop, daemon=True)
        self.poller_thread.start()
        self.ping_thread.start()

        # Start UI refresh timer
        self._refresh_ui()

        # Update controls
        self.btn_connect.config(state=tk.DISABLED)
        self.btn_disconnect.config(state=tk.NORMAL)
        self.status_var.set(f"Connected as #{self.client.client_id}")

    def _on_disconnect(self):
        self._running = False
        if self.client:
            self.client.disconnect()
            self.client = None
        self.btn_connect.config(state=tk.NORMAL)
        self.btn_disconnect.config(state=tk.DISABLED)
        self.recipient_combo["values"] = []
        self.recipient_var.set("")
        self.status_var.set("Disconnected")

    # ----------------------------------------------------------- background threads
    def _poller_loop(self):
        """Periodically ask server for queued messages (MT_GETDATA)."""
        while self._running and self.client and self.client.alive:
            try:
                self.client._send_message(SalakhovaClient.ADDR_SERVER, SalakhovaClient.MT_GETDATA)
            except Exception:
                break
            time.sleep(0.05)

    def _ping_loop(self):
        """Send MT_INFO keep-alive every 10 seconds."""
        while self._running and self.client and self.client.alive:
            try:
                self.client._send_message(SalakhovaClient.ADDR_SERVER, SalakhovaClient.MT_INFO)
            except Exception:
                break
            time.sleep(10)

    # ----------------------------------------------------------- UI refresh (main thread)
    def _refresh_ui(self):
        """Drain inbox and update UI — scheduled on the Tk main loop."""
        if not self.client or not self.client.alive:
            # Connection lost
            if self._running:
                self._on_disconnect()
                messagebox.showwarning("Disconnected", "Connection to server lost.")
            return

        while True:
            msg = self.client.poll()
            if msg is None:
                break
            from_id, msg_type, to, payload = msg

            if msg_type == SalakhovaClient.MT_DATA:
                self._append_message(from_id, payload)
            elif msg_type == SalakhovaClient.MT_CONFIRM:
                clients = parse_client_list(payload)
                if clients:
                    self.client.clients = clients
                    self._update_recipients(clients)

        self.root.after(50, self._refresh_ui)

    def _append_message(self, from_id: int, text: str):
        self.msg_text.config(state=tk.NORMAL)
        sender = self.client.clients.get(from_id, f"#{from_id}") if self.client else f"#{from_id}"
        self.msg_text.insert(tk.END, f"[{sender}]: {text}\n")
        self.msg_text.config(state=tk.DISABLED)
        self.msg_text.see(tk.END)

    def _update_recipients(self, clients: dict):
        """Populate recipient combobox from server client list."""
        my_id = self.client.client_id if self.client else -1
        values = []
        for cid, cname in clients.items():
            if cid == my_id:
                continue
            label = f"{cid}: {cname}"
            values.append(label)

        # Add broadcast option
        values.append("-1: Broadcast (all)")

        self.recipient_combo["values"] = values
        # Keep current selection if still valid
        if not self.recipient_var.get() and values:
            self.recipient_combo.current(0)

    # ----------------------------------------------------------- send
    def _on_send(self):
        if not self.client or not self.client.alive:
            messagebox.showwarning("Not connected", "Connect to server first.")
            return

        raw = self.recipient_var.get()
        if not raw:
            messagebox.showwarning("No recipient", "Select a recipient.")
            return

        # Parse "id: Name" format → extract id
        try:
            target_id = int(raw.split(":", 1)[0].strip())
        except (ValueError, IndexError):
            messagebox.showwarning("Invalid recipient", f"Cannot parse recipient: {raw}")
            return

        text = self.msg_var.get().strip()
        if not text:
            return

        try:
            self.client._send_message(target_id, SalakhovaClient.MT_DATA, text)
        except Exception as exc:
            messagebox.showerror("Send Error", str(exc))
            return

        # Show sent message locally
        my_name = self.client.clients.get(self.client.client_id, f"#{self.client.client_id}")
        self.msg_text.config(state=tk.NORMAL)
        self.msg_text.insert(tk.END, f"[{my_name} -> {raw.split(':', 1)[1].strip() if ':' in raw else raw}]: {text}\n")
        self.msg_text.config(state=tk.DISABLED)
        self.msg_text.see(tk.END)

        self.msg_var.set("")


def main():
    root = tk.Tk()
    SalakhovaGuiClient(root)
    root.mainloop()


if __name__ == "__main__":
    main()