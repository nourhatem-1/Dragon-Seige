import tkinter as tk
from tkinter import ttk, messagebox
import serial
import serial.tools.list_ports
import threading
import time
import subprocess
import sys

BAUD = 115200
WHITE = "#FFFFFF"
BLUE = "#7CCBFF"
BLUE_DARK = "#1595DF"
TEXT_BLUE = "#138FD1"

class SerialReader:
    def __init__(self, on_line):
        self.on_line = on_line
        self.ser = None
        self.running = False

    def connect(self, port):
        self.disconnect()
        self.ser = serial.Serial(port, BAUD, timeout=0.1)
        self.running = True
        threading.Thread(target=self.reader, daemon=True).start()

    def disconnect(self):
        self.running = False
        try:
            if self.ser and self.ser.is_open:
                self.ser.close()
        except Exception:
            pass
        self.ser = None

    def ok(self):
        return self.ser is not None and self.ser.is_open

    def send(self, msg):
        if self.ok():
            self.ser.write((msg.strip() + "\n").encode("utf-8"))

    def reader(self):
        buf = b""
        while self.running:
            try:
                if self.ser and self.ser.in_waiting:
                    buf += self.ser.read(self.ser.in_waiting)
                    while b"\n" in buf:
                        line, buf = buf.split(b"\n", 1)
                        text = line.decode(errors="ignore").strip()
                        if text:
                            self.on_line(text)
                time.sleep(0.02)
            except Exception:
                self.on_line("DISCONNECTED")
                break

class App:
    def __init__(self, root):
        self.root = root
        self.root.title("Castle Defence LCD Mirror")
        self.root.geometry("760x500")
        self.root.configure(bg=WHITE)
        self.root.resizable(False, False)

        self.bt = SerialReader(self.handle_line)
        self.build()
        self.refresh_ports()
        self.root.after(1000, self.tick)

    def build(self):
        tk.Label(self.root, text="Castle Defence", bg=WHITE, fg=TEXT_BLUE,
                 font=("Segoe UI", 30, "bold")).pack(anchor="w", padx=28, pady=(22, 0))

        tk.Label(self.root, text="LCD mirror through ESP1 Bluetooth bridge",
                 bg=WHITE, fg=TEXT_BLUE, font=("Segoe UI", 11, "bold")).pack(anchor="w", padx=31, pady=(0, 18))

        top = tk.Frame(self.root, bg=BLUE)
        top.pack(fill="x", padx=28, pady=(0, 20))

        tk.Label(top, text="Connection", bg=BLUE, fg=WHITE,
                 font=("Segoe UI", 16, "bold")).pack(anchor="w", padx=18, pady=(14, 8))

        row = tk.Frame(top, bg=BLUE)
        row.pack(fill="x", padx=18, pady=(0, 14))

        self.port_box = ttk.Combobox(row, state="readonly", width=18)
        self.port_box.pack(side="left", padx=(0, 8), ipady=4)

        self.button(row, "Refresh", self.refresh_ports).pack(side="left", padx=4)
        self.button(row, "Connect ESP1", self.connect).pack(side="left", padx=4)
        self.button(row, "Disconnect", self.disconnect).pack(side="left", padx=4)
        self.button(row, "Bluetooth Settings", self.open_bluetooth).pack(side="right", padx=4)

        self.status = tk.Label(top, text="Not connected", bg=BLUE, fg=WHITE,
                               font=("Segoe UI", 11, "bold"))
        self.status.pack(anchor="w", padx=18, pady=(0, 14))

        lcd_card = tk.Frame(self.root, bg=BLUE)
        lcd_card.pack(fill="both", expand=True, padx=90, pady=(0, 22))

        tk.Label(lcd_card, text="LCD DISPLAY", bg=BLUE, fg=WHITE,
                 font=("Segoe UI", 13, "bold")).pack(pady=(18, 8))

        lcd_inner = tk.Frame(lcd_card, bg=BLUE_DARK)
        lcd_inner.pack(fill="both", expand=True, padx=28, pady=(0, 26))

        self.l1 = tk.Label(lcd_inner, text="Waiting", bg=BLUE_DARK, fg=WHITE,
                           font=("Consolas", 30, "bold"))
        self.l1.pack(expand=True)

        self.l2 = tk.Label(lcd_inner, text="for ESP1", bg=BLUE_DARK, fg=WHITE,
                           font=("Consolas", 30, "bold"))
        self.l2.pack(expand=True)

        log_card = tk.Frame(self.root, bg=BLUE)
        log_card.pack(fill="x", padx=28, pady=(0, 20))

        self.log = tk.Text(log_card, height=4, bg=BLUE_DARK, fg=WHITE,
                           relief="flat", font=("Consolas", 9))
        self.log.pack(fill="x", padx=16, pady=16)

    def button(self, parent, text, cmd):
        return tk.Button(parent, text=text, command=cmd, bg=WHITE, fg=TEXT_BLUE,
                         activebackground="#EAF7FF", activeforeground=TEXT_BLUE,
                         relief="flat", bd=0, font=("Segoe UI", 9, "bold"),
                         padx=10, pady=7)

    def refresh_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.port_box["values"] = ports
        if "COM20" in ports:
            self.port_box.set("COM20")
        elif "COM17" in ports:
            self.port_box.set("COM17")
        elif ports:
            self.port_box.set(ports[0])
        self.log_msg("Ports refreshed: " + (", ".join(ports) if ports else "none"))

    def connect(self):
        port = self.port_box.get().strip()
        if not port:
            messagebox.showwarning("No port", "Pair ESP1 Bluetooth first, then refresh.")
            return
        try:
            self.bt.connect(port)
            self.status.config(text=f"Connected to ESP1 on {port}")
            self.log_msg(f"Connected to ESP1 on {port}")
            self.bt.send("STATUS")
        except Exception as e:
            messagebox.showerror("Connection failed", str(e))
            self.log_msg("Connection failed: " + str(e))

    def disconnect(self):
        self.bt.disconnect()
        self.status.config(text="Not connected")
        self.log_msg("Disconnected")

    def handle_line(self, line):
        def ui():
            self.log_msg(line)
            if line.startswith("LCD:"):
                payload = line[4:]
                if "|" in payload:
                    a, b = payload.split("|", 1)
                    self.l1.config(text=a[:16])
                    self.l2.config(text=b[:16])
            elif line == "DISCONNECTED":
                self.status.config(text="Disconnected")
        self.root.after(0, ui)

    def open_bluetooth(self):
        if sys.platform.startswith("win"):
            subprocess.Popen(["start", "ms-settings:bluetooth"], shell=True)
        else:
            messagebox.showinfo("Bluetooth", "Open Bluetooth settings and pair ESP1.")

    def log_msg(self, msg):
        self.log.insert("end", time.strftime("%H:%M:%S") + "  " + msg + "\n")
        self.log.see("end")

    def tick(self):
        if self.bt.ok():
            self.status.config(text=f"Connected to ESP1 on {self.bt.ser.port}")
        self.root.after(1000, self.tick)

if __name__ == "__main__":
    root = tk.Tk()
    App(root)
    root.mainloop()
