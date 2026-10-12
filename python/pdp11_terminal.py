"""Terminal: the DL11 console on this program's stdin and stdout.

FIX: the Python's Terminal was a PySimpleGUI stub that couldn't read the
keyboard and printed the character code as a number (newchar was never
read from XBUF).  This one writes each transmitted byte to stdout and feeds
keys from stdin to the DL11 receiver, like the C, Go and Rust versions.
"""
import logging
import os
import queue
import sys
import threading


class Terminal:
    """stdin/stdout terminal on a DL11"""
    def __init__(self, dl11, sw):
        logging.info('initializing terminal')
        self.dl11 = dl11
        self.sw = sw
        self.buffer = 0  # for holding LF after CR was sent
        self.out = sys.stdout.buffer
        self.keys = None  # queue of input bytes; None until keys_from()

    def keys_from(self, stream):
        """Read keys from stream (a file with fileno()) on a daemon thread.
        The CPU loop takes them one at a time when the receiver is empty."""
        self.keys = queue.Queue(256)
        fd = stream.fileno()

        def reader():
            while True:
                try:
                    data = os.read(fd, 64)
                except OSError:
                    data = b''
                if not data:
                    return  # end of input: no more keys
                for byte in data:
                    self.keys.put(byte)

        threading.Thread(target=reader, daemon=True).start()

    def cycle(self):
        """One cycle of the terminal, after each instruction"""
        dl11 = self.dl11
        # If there's a character in our buffer, send it to the DL11
        if self.buffer != 0 and dl11.RCSR & dl11.RCSR_RCVR_DONE == 0:
            dl11.write_RBUF(self.buffer)
            self.buffer = 0

        # if there's a character in the dl11 transmit buffer, then eat it
        if dl11.XCSR & dl11.XCSR_XMIT_RDY == 0:
            newchar = dl11.read_XBUF()
            # Sure, DL11 can send me nulls; I just won't show them.
            if newchar != 0:
                self.out.write(bytes([newchar & 0o377]))
                self.out.flush()

        # take a key if the receiver has room for it
        if self.keys is not None and dl11.RCSR & dl11.RCSR_RCVR_DONE == 0:
            try:
                key = self.keys.get_nowait()
            except queue.Empty:
                return
            dl11.write_RBUF(key)


def raw_input(stream=sys.stdin):
    """Put the terminal into character-at-a-time mode without echo.
    Returns a function that restores it (a no-op if stream isn't a tty)."""
    if not stream.isatty():
        return lambda: None
    import termios  # pylint: disable=import-outside-toplevel
    fd = stream.fileno()
    saved = termios.tcgetattr(fd)
    raw = termios.tcgetattr(fd)
    raw[3] &= ~(termios.ICANON | termios.ECHO)  # lflag; keep ISIG for Ctrl-C
    raw[0] &= ~termios.ICRNL                     # iflag: Return arrives as CR
    raw[6][termios.VMIN] = 1
    raw[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, raw)
    return lambda: termios.tcsetattr(fd, termios.TCSANOW, saved)
