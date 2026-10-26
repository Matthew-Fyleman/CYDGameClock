"""
CYD Game Clock - link to the clock

Sends stats to the clock over WiFi (UDP, encrypted and authenticated with
AES-256-GCM using a key derived from the clock's pairing code) and falls back
to the USB serial port when the clock does not answer over WiFi.

Packet: b"CGC1" | 12-byte nonce | ciphertext | 16-byte tag (AAD = b"CGC1").
The plaintext JSON carries "sid" (random session id), "seq" (counter) and
"t" (unix time) so recorded packets cannot be replayed.
"""

import hashlib
import json
import os
import secrets
import socket
import time

from cryptography.hazmat.primitives.ciphers.aead import AESGCM

try:
    import serial
    import serial.tools.list_ports
    SERIAL_AVAILABLE = True
except ImportError:
    SERIAL_AVAILABLE = False

MAGIC = b"CGC1"
KDF_SALT = b"CYDGameClock/v1"   # must match the firmware (security.h)
KDF_ITERATIONS = 10000
CODE_ALPHABET = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"
CODE_LENGTH = 16

SERIAL_BAUD = 115200
PROBE = b"?CYDGC\n"
PROBE_REPLY = "!CYDGC"

# WiFi counts as working while the clock has acknowledged a packet this recently
WIFI_ACK_TIMEOUT = 12.0
# How often to look for the clock on the COM ports while WiFi is down
SERIAL_SEARCH_INTERVAL = 30.0
# How often to look the clock's host name up again
DNS_REFRESH_INTERVAL = 300.0


def normalise_pairing_code(code):
    """Upper-case the code and drop dashes/spaces. Raises ValueError if invalid."""
    cleaned = "".join(c for c in (code or "").upper() if c.isalnum())
    if len(cleaned) != CODE_LENGTH or any(c not in CODE_ALPHABET for c in cleaned):
        raise ValueError(
            "The pairing code is 16 letters and digits, shown on the clock's "
            "information screen (tap the (i) icon), e.g. ABCD-EFGH-JKLM-NPQR")
    return cleaned


def derive_key(code):
    return hashlib.pbkdf2_hmac("sha256", normalise_pairing_code(code).encode(),
                               KDF_SALT, KDF_ITERATIONS, 32)


class UdpLink:
    """Encrypted UDP link to the clock."""

    def __init__(self, host, port, pairing_code):
        self.host = host
        self.port = int(port)
        self.aead = AESGCM(derive_key(pairing_code))
        self.sid = secrets.randbits(32)
        self.seq = 0
        self.addr = None
        self.addr_time = 0.0
        self.last_ack_time = 0.0
        self.last_ack = None
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setblocking(False)

    def _resolve(self):
        now = time.time()
        if self.addr is None or now - self.addr_time > DNS_REFRESH_INTERVAL:
            try:
                self.addr = (socket.gethostbyname(self.host), self.port)
                self.addr_time = now
            except OSError:
                self.addr = None
        return self.addr

    def send(self, payload):
        addr = self._resolve()
        if addr is None:
            return False
        self.seq += 1
        body = dict(payload, sid=self.sid, seq=self.seq, t=int(time.time()))
        plaintext = json.dumps(body, separators=(",", ":")).encode("utf-8")
        nonce = os.urandom(12)
        packet = MAGIC + nonce + self.aead.encrypt(nonce, plaintext, MAGIC)
        try:
            self.sock.sendto(packet, addr)
            return True
        except OSError:
            self.addr = None  # look the address up again next time
            return False

    def poll(self, wait=0.0):
        """Read any replies from the clock; returns the newest ack or None."""
        deadline = time.time() + wait
        newest = None
        while True:
            try:
                data, _ = self.sock.recvfrom(2048)
            except BlockingIOError:
                if newest is not None or time.time() >= deadline:
                    break
                time.sleep(0.02)
                continue
            except OSError:
                break
            if len(data) <= len(MAGIC) + 12 + 16 or not data.startswith(MAGIC):
                continue
            nonce, sealed = data[4:16], data[16:]
            try:
                ack = json.loads(self.aead.decrypt(nonce, sealed, MAGIC))
            except Exception:
                continue  # not from our clock
            if ack.get("sid") == self.sid:
                newest = ack
        if newest is not None:
            self.last_ack = newest
            self.last_ack_time = time.time()
        return newest

    def is_alive(self):
        return time.time() - self.last_ack_time < WIFI_ACK_TIMEOUT

    def close(self):
        self.sock.close()


def _open_serial(port):
    """Open a COM port without pulsing DTR/RTS (which would reset an ESP32)."""
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = SERIAL_BAUD
    ser.timeout = 0
    ser.write_timeout = 1
    ser.dtr = False
    ser.rts = False
    ser.open()
    return ser


def find_clock_port(preferred=None, log=print):
    """
    Return the COM port of a CYD Game Clock, or None.

    Each free port is asked "?CYDGC"; only this clock's firmware answers
    "!CYDGC", so other CYD boards and serial devices are skipped. Ports that
    another program already has open are left alone.
    """
    if not SERIAL_AVAILABLE:
        return None
    ports = [p.device for p in serial.tools.list_ports.comports()]
    if preferred in ports:
        ports.remove(preferred)
        ports.insert(0, preferred)
    for port in ports:
        try:
            ser = _open_serial(port)
        except (OSError, serial.SerialException):
            continue  # busy or not a real port
        try:
            ser.reset_input_buffer()
            ser.write(b"\n" + PROBE)
            buf = b""
            deadline = time.time() + 1.5
            while time.time() < deadline:
                chunk = ser.read(256)
                if chunk:
                    buf += chunk
                    if PROBE_REPLY.encode() in buf:
                        log(f"  Found the CYD Game Clock on {port}")
                        return port
                else:
                    time.sleep(0.05)
        except (OSError, serial.SerialException):
            pass
        finally:
            ser.close()
    return None


class SerialLink:
    """Plain JSON lines over the USB cable (the cable itself is the security)."""

    def __init__(self, port):
        self.port = port
        self.ser = _open_serial(port)
        self.sid = secrets.randbits(32)
        self.seq = 0
        self.buf = b""
        self.last_ack = None
        self.last_ack_time = time.time()  # give a new link time to answer

    def send(self, payload):
        self.seq += 1
        body = dict(payload, sid=self.sid, seq=self.seq, t=int(time.time()))
        try:
            self.ser.write(json.dumps(body, separators=(",", ":")).encode("utf-8") + b"\n")
            return True
        except (OSError, serial.SerialException):
            return False

    def poll(self, wait=0.0):
        deadline = time.time() + wait
        newest = None
        while True:
            try:
                chunk = self.ser.read(4096)
            except (OSError, serial.SerialException):
                break
            if chunk:
                self.buf += chunk
                *lines, self.buf = self.buf.split(b"\n")
                for line in lines:
                    line = line.strip()
                    if line.startswith(b"!ACK "):
                        try:
                            newest = json.loads(line[5:])
                        except ValueError:
                            pass
                continue
            if newest is not None or time.time() >= deadline:
                break
            time.sleep(0.02)
        if newest is not None:
            self.last_ack = newest
            self.last_ack_time = time.time()
        return newest

    def is_alive(self):
        return time.time() - self.last_ack_time < WIFI_ACK_TIMEOUT

    def close(self):
        try:
            self.ser.close()
        except Exception:
            pass


class ClockLink:
    """
    Chooses the transport: WiFi while the clock acknowledges it, otherwise the
    USB serial port (found automatically). transport = "auto", "wifi" or "serial".
    """

    def __init__(self, config, log=print):
        self.log = log
        self.transport = config.get("transport", "auto")
        self.serial_port_hint = config.get("serial_port") or None
        self.udp = None
        self.serial = None
        self.last_serial_search = 0.0
        self.started = time.time()
        self.want = None  # metric IDs the clock shows; None = unknown (send all)
        if self.transport in ("auto", "wifi"):
            host = config.get("clock_host", "").strip()
            code = config.get("pairing_code", "")
            if host and code:
                self.udp = UdpLink(host, config.get("udp_port", 4210), code)
            elif self.transport == "wifi":
                raise ValueError("WiFi needs the clock's address and pairing code")

    @property
    def active(self):
        if self.udp and self.udp.is_alive():
            return "wifi"
        if self.serial:
            return "serial"
        return None

    def _need_serial(self):
        if self.transport == "serial":
            return True
        if self.transport == "wifi":
            return False
        if self.udp is None:
            return True
        # Give WiFi a few seconds after start-up before falling back
        return not self.udp.is_alive() and time.time() - self.started > WIFI_ACK_TIMEOUT

    def send(self, payload):
        """Send one stats packet; returns the transport used ("wifi"/"serial") or None."""
        used = None
        if self.udp:
            self.udp.send(payload)
            ack = self.udp.poll(wait=0.25)
            if ack is not None:
                self._update_want(ack)
                used = "wifi"

        if used is None and self._need_serial():
            if self.serial is None and time.time() - self.last_serial_search > SERIAL_SEARCH_INTERVAL:
                self.last_serial_search = time.time()
                port = find_clock_port(self.serial_port_hint, self.log)
                if port:
                    try:
                        self.serial = SerialLink(port)
                    except Exception as e:
                        self.log(f"  Could not open {port}: {e}")
            if self.serial:
                if self.serial.send(payload):
                    ack = self.serial.poll(wait=0.3)
                    if ack is not None:
                        self._update_want(ack)
                    used = "serial"
                if not self.serial.is_alive() or used is None:
                    self.log(f"  Lost the clock on {self.serial.port}")
                    self.serial.close()
                    self.serial = None
        elif used == "wifi" and self.serial:
            # WiFi is back: let go of the COM port
            self.serial.close()
            self.serial = None
        return used

    def _update_want(self, ack):
        want = ack.get("want")
        if isinstance(want, list):
            self.want = set(int(i) for i in want)

    def close(self):
        if self.udp:
            self.udp.close()
        if self.serial:
            self.serial.close()
