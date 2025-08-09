#!/usr/bin/env python3
"""
Simple Tkinter GUI to select RGB effect and global brightness on the Pico Game Controller over HID Feature reports.
Requires: hidapi (pip install hidapi).
"""

import tkinter as tk
from tkinter import ttk, messagebox

try:
    import hid  # from hidapi
except ImportError:
    hid = None
    HIDErrors = (OSError,)
else:
    HIDException = getattr(hid, "HIDException", OSError)
    HIDErrors = (HIDException, OSError)

# Match firmware VID/PID
VID = 0x1CCF
PID = 0x8048

# Report IDs must match firmware
REPORT_ID_CONFIG = 5

# Commands
CMD_SET_EFFECT = 0x01
CMD_SET_BRIGHTNESS = 0x02
CMD_REBOOT_BOOTSEL = 0x03

EFFECTS = [
    (0, "Color Cycle"),
    (1, "Turbocharger"),
    (2, "Trail"),
    (3, "Dual Orbit"),
    (4, "Velocity Comet"),
    (5, "Button Ripples"),
    (6, "Spokes"),
    (7, "Counter Stripes"),
    (8, "Palette Tint Gradient"),
    (9, "Multipoint Snap"),
    (10, "Center Pulse"),
    (11, "Sector Equalizer"),
    (12, "Radar Sweep"),
]


def open_device():
    if hid is None:
        raise RuntimeError("hidapi is not available. pip install hidapi")
    dev = hid.device()
    dev.open(VID, PID)
    dev.set_nonblocking(True)
    return dev


def get_status(dev):
    """Return (effect_id, brightness) or (None, None) if not available."""
    try:
        # Ask for feature data (report id prefix is required by hidapi)
        dev.send_feature_report(bytes([REPORT_ID_CONFIG, 0x00] + [0] * 7))
        data = dev.get_feature_report(REPORT_ID_CONFIG, 9)  # id + 8 bytes
        if data and len(data) >= 4:
            # data[0] = report id, data[1]=status(0), data[2]=effect, data[3]=brightness
            return data[2], data[3]
    except HIDErrors:
        pass
    return None, None


def set_effect(dev, effect_id: int):
    payload = [REPORT_ID_CONFIG, CMD_SET_EFFECT,
               int(effect_id) & 0xFF] + [0] * 6
    dev.send_feature_report(bytes(payload))


def set_brightness(dev, value: int):
    v = max(0, min(255, int(value)))
    payload = [REPORT_ID_CONFIG, CMD_SET_BRIGHTNESS, v] + [0] * 6
    dev.send_feature_report(bytes(payload))


def reboot_to_bootsel(dev):
    """Ask firmware to reboot into BOOTSEL (UF2) mode."""
    payload = [REPORT_ID_CONFIG, CMD_REBOOT_BOOTSEL] + [0] * 7
    dev.send_feature_report(bytes(payload))


class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Pico IIDX Effect Selector")
        self.geometry("400x210")
        self.dev = None

        frm = ttk.Frame(self, padding=10)
        frm.pack(fill=tk.BOTH, expand=True)

        ttk.Label(frm, text="RGB Effect:").grid(row=0, column=0, sticky="w")
        self.effect_var = tk.StringVar()
        self.combo = ttk.Combobox(
            frm,
            textvariable=self.effect_var,
            values=[name for _, name in EFFECTS],
            state="readonly",
        )
        self.combo.grid(row=0, column=1, sticky="ew", padx=(8, 0))
        frm.columnconfigure(1, weight=1)

        # Brightness controls
        ttk.Label(frm, text="Brightness:").grid(
            row=1, column=0, sticky="w", pady=(10, 0))
        self.brightness_val_label = ttk.Label(frm, text="255")
        self.brightness_val_label.grid(
            row=1, column=2, sticky="w", padx=(6, 0))
        self.brightness_scale = ttk.Scale(
            frm,
            from_=0,
            to=255,
            orient=tk.HORIZONTAL,
            command=self.on_brightness_change,
        )
        self.brightness_scale.set(255)
        self.brightness_scale.grid(
            row=1, column=1, sticky="ew", padx=(8, 0), pady=(8, 0))

        self.btn_refresh = ttk.Button(
            frm, text="Refresh", command=self.refresh)
        self.btn_refresh.grid(row=2, column=0, pady=(12, 0), sticky="w")
        self.btn_apply = ttk.Button(frm, text="Apply", command=self.apply)
        self.btn_apply.grid(row=2, column=1, pady=(12, 0), sticky="e")

        # Reboot to BOOTSEL button
        self.btn_bootsel = ttk.Button(
            frm, text="Reboot to BOOTSEL", command=self.on_bootsel
        )
        self.btn_bootsel.grid(row=2, column=2, pady=(12, 0), sticky="e")

        self.status_var = tk.StringVar(value="Not connected")
        ttk.Label(frm, textvariable=self.status_var).grid(
            row=3, column=0, columnspan=3, sticky="w", pady=(12, 0)
        )

        self.after(200, self.auto_connect)

    def on_brightness_change(self, v):
        self.brightness_val_label.configure(text=str(int(float(v))))

    def auto_connect(self):
        try:
            self.dev = open_device()
            self.status_var.set("Connected")
            self.refresh()
        except HIDErrors as e:
            self.status_var.set(f"Not connected: {e}")
            # retry later
            self.after(1500, self.auto_connect)

    def refresh(self):
        if not self.dev:
            messagebox.showwarning("Device", "Not connected")
            return
        eff, bri = get_status(self.dev)
        if eff is not None and 0 <= eff < len(EFFECTS):
            self.combo.current(eff)
        else:
            self.combo.current(0)
        if bri is not None:
            self.brightness_scale.set(bri)
            self.brightness_val_label.configure(text=str(int(bri)))

    def apply(self):
        if not self.dev:
            messagebox.showwarning("Device", "Not connected")
            return
        idx = self.combo.current()
        try:
            set_effect(self.dev, idx)
            bri = int(float(self.brightness_scale.get()))
            set_brightness(self.dev, bri)
            self.status_var.set(
                f"Applied: {EFFECTS[idx][1]}, Brightness {bri}")
        except HIDErrors as e:
            messagebox.showerror("Error", str(e))

    def on_bootsel(self):
        if not self.dev:
            messagebox.showwarning("Device", "Not connected")
            return
        if not messagebox.askyesno(
            "Reboot to BOOTSEL",
            "The device will reboot into BOOTSEL (UF2) mode. Continue?",
        ):
            return
        try:
            reboot_to_bootsel(self.dev)
            self.status_var.set("Rebooting to BOOTSEL…")
        except HIDErrors as e:
            messagebox.showerror("Error", str(e))


if __name__ == "__main__":
    app = App()
    app.mainloop()
