#!/usr/bin/env python3
"""
Simple Tkinter GUI to select RGB effect and global brightness on the Pico Game Controller over HID Feature reports.
Requires: hidapi (pip install hidapi).

Debugging:
- Run with --debug to print extra logs to the console.
- Use the "Debug HID" button to list all HID interfaces for this VID/PID.
"""

import sys
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

# Debug flag
DEBUG = "--debug" in sys.argv

# Simple logger


def log(*args):
    if DEBUG:
        try:
            print("[DEBUG]", *args)
        except Exception:
            pass


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


def hid_enumerate_info():
    """Return a human-readable list of HID interfaces for our VID/PID."""
    if hid is None:
        return "hidapi no disponible"
    lines = []
    try:
        for d in hid.enumerate(VID, PID):
            path = d.get("path")
            if isinstance(path, (bytes, bytearray)):
                try:
                    path = path.decode("utf-8", "ignore")
                except Exception:
                    path = str(path)
            lines.append(
                f"path={path}\n  usage_page=0x{int(d.get('usage_page', 0)):04X} usage=0x{int(d.get('usage', 0)):04X} interface={d.get('interface_number')}\n  product={d.get('product_string')} serial={d.get('serial_number')}"
            )
    except HIDErrors as e:
        lines.append(f"enumerate error: {e}")
    if not lines:
        lines.append("No se encontraron interfaces HID para este VID/PID")
    return "\n\n".join(lines)


def _find_config_path():
    """Return HID path for our vendor-specific config collection, if present."""
    try:
        all_devs = list(hid.enumerate(VID, PID))
        log(f"enumerate count={len(all_devs)}")
        for d in all_devs:
            # Prefer the vendor usage page we defined in the firmware (0xFFAF)
            if d.get("usage_page") == 0xFFAF:
                path = d.get("path")
                log("selected vendor path", path)
                return path
    except HIDErrors:
        pass
    return None


def open_device():
    if hid is None:
        raise RuntimeError("hidapi is not available. pip install hidapi")
    dev = hid.device()
    path = _find_config_path()
    if path:
        dev.open_path(path)
    else:
        # Fallback: open the first interface for this VID/PID
        dev.open(VID, PID)
    dev.set_nonblocking(True)
    log("device opened (path?", bool(path), ")")
    return dev


def get_status(dev):
    """Return (effect_id, brightness) or (None, None) if not available."""
    try:
        # Ask for feature data (report id prefix is required by hidapi)
        payload = bytes([REPORT_ID_CONFIG, 0x00] + [0] * 7)
        log("send_feature_report GET:", list(payload))
        dev.send_feature_report(payload)
        data = dev.get_feature_report(REPORT_ID_CONFIG, 9)  # id + 8 bytes
        log("get_feature_report ->", list(data) if data else None)
        if data and len(data) >= 4:
            # data[0] = report id, data[1]=status(0), data[2]=effect, data[3]=brightness
            return data[2], data[3]
    except HIDErrors as e:
        log("get_status error:", e)
    return None, None


def set_effect(dev, effect_id: int):
    payload = [REPORT_ID_CONFIG, CMD_SET_EFFECT,
               int(effect_id) & 0xFF] + [0] * 6
    log("send_feature_report SET_EFFECT:", payload)
    dev.send_feature_report(bytes(payload))


def set_brightness(dev, value: int):
    v = max(0, min(255, int(value)))
    payload = [REPORT_ID_CONFIG, CMD_SET_BRIGHTNESS, v] + [0] * 6
    log("send_feature_report SET_BRIGHTNESS:", payload)
    dev.send_feature_report(bytes(payload))


def reboot_to_bootsel(dev):
    """Ask firmware to reboot into BOOTSEL (UF2) mode."""
    payload = [REPORT_ID_CONFIG, CMD_REBOOT_BOOTSEL] + [0] * 7
    log("send_feature_report REBOOT_BOOTSEL:", payload)
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

        # Debug button
        self.btn_debug = ttk.Button(
            frm, text="Debug HID", command=self.on_debug)
        self.btn_debug.grid(row=3, column=0, pady=(8, 0), sticky="w")

        self.status_var = tk.StringVar(value="Not connected")
        ttk.Label(frm, textvariable=self.status_var).grid(
            row=4, column=0, columnspan=3, sticky="w", pady=(12, 0)
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
            log("auto_connect error:", e)
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
            log("apply error:", e)
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
            log("reboot error:", e)
            messagebox.showerror("Error", str(e))

    def on_debug(self):
        info = hid_enumerate_info()
        log("HID enumerate info:\n" + info)
        messagebox.showinfo("HID Interfaces", info)


if __name__ == "__main__":
    # Headless CLI: send reboot-to-bootloader if requested
    if "--reboot-bootsel" in sys.argv:
        try:
            dev = open_device()
            reboot_to_bootsel(dev)
            print("OK: reboot command sent")
            sys.exit(0)
        except HIDErrors as e:
            log("cli reboot error:", e)
            print(f"ERR: {e}")
            sys.exit(1)
    # Default: launch GUI
    app = App()
    app.mainloop()
