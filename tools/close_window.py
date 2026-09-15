"""Send WM_DELETE_WINDOW to an X11 window, like clicking its close button.

Usage: close_window.py <window id>
Uses libX11 through ctypes, so no extra Python packages are needed.
"""
import ctypes
import sys

CLIENT_MESSAGE = 33


class XClientMessageEvent(ctypes.Structure):
    _fields_ = [
        ("type", ctypes.c_int),
        ("serial", ctypes.c_ulong),
        ("send_event", ctypes.c_int),
        ("display", ctypes.c_void_p),
        ("window", ctypes.c_ulong),
        ("message_type", ctypes.c_ulong),
        ("format", ctypes.c_int),
        ("data", ctypes.c_long * 5),
    ]


class XEvent(ctypes.Union):
    _fields_ = [("xclient", XClientMessageEvent), ("pad", ctypes.c_long * 24)]


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    window = int(sys.argv[1], 0)

    x11 = ctypes.cdll.LoadLibrary("libX11.so.6")
    x11.XOpenDisplay.restype = ctypes.c_void_p
    x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
    x11.XInternAtom.restype = ctypes.c_ulong
    x11.XInternAtom.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int]
    x11.XSendEvent.argtypes = [ctypes.c_void_p, ctypes.c_ulong, ctypes.c_int, ctypes.c_long, ctypes.c_void_p]
    x11.XFlush.argtypes = [ctypes.c_void_p]
    x11.XCloseDisplay.argtypes = [ctypes.c_void_p]

    display = x11.XOpenDisplay(None)
    if not display:
        print("close_window: cannot open X display", file=sys.stderr)
        return 1

    event = XEvent()
    event.xclient.type = CLIENT_MESSAGE
    event.xclient.window = window
    event.xclient.message_type = x11.XInternAtom(display, b"WM_PROTOCOLS", False)
    event.xclient.format = 32
    event.xclient.data[0] = x11.XInternAtom(display, b"WM_DELETE_WINDOW", False)
    event.xclient.data[1] = 0  # CurrentTime

    status = x11.XSendEvent(display, window, False, 0, ctypes.byref(event))
    x11.XFlush(display)
    x11.XCloseDisplay(display)
    return 0 if status else 1


if __name__ == "__main__":
    sys.exit(main())
