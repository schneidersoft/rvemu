#!/usr/bin/env python3
# Hermetic runner for the net test: one file that both serves the local
# HTTP server and runs the emulator against it.
#
# Serves this directory (test/net/) — whose index.html carries the marker
# rvemu-net-ok — on 127.0.0.1:8931, waits for it to accept connections,
# then runs the given command and exits with that command's exit code:
#
#   python3 test/net/net.py ./bin/riscv64i bin/net.bin
#   python3 test/net/net.py qemu-riscv64 out/riscv64i-riscv64 bin/net.bin
#
# The net guest (test/net.c) dials 127.0.0.1:8931, so the whole test runs
# over loopback: no DNS, no external network.
#
# The per-request access log is passed to stderr to help debugging if
# something goes wrong on the server side.
#
# exit status: that of the command, except:
#   2  no command given (usage error)
#   4  the server could not start or never accepted connections
import functools
import signal
import socket
import subprocess
import sys
import threading
import time
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

HOST = "127.0.0.1"
PORT = 8931
DIR = Path(__file__).resolve().parent


def main(argv):
    if len(argv) < 2:
        print("usage: net.py <command> [args...]", file=sys.stderr)
        return 2

    # Same server class `python3 -m http.server` uses: every connection is
    # handled in its own thread, so the readiness probe below cannot starve
    # the guest's real request. The bind happens synchronously, so a busy
    # port is a definite failure (exit 4) rather than a hang.
    # Serve test/net/ regardless of the caller's working directory.
    handler = functools.partial(SimpleHTTPRequestHandler, directory=str(DIR))
    try:
        server = ThreadingHTTPServer((HOST, PORT), handler)
    except OSError as exc:
        print(f"net: cannot listen on {HOST}:{PORT}: {exc}", file=sys.stderr)
        return 4

    thread = threading.Thread(
        target=server.serve_forever, kwargs={"poll_interval": 0.1}, daemon=True
    )
    thread.start()
    print(f"serving http://{HOST}:{PORT} from {DIR}", file=sys.stderr)

    # Wait until the socket accepts connections (up to ~10 s).
    for attempt in range(50):
        try:
            with socket.create_connection((HOST, PORT), timeout=0.2):
                break
        except OSError:
            if attempt == 49:
                print(f"net: server not accepting on {HOST}:{PORT} after 10 s", file=sys.stderr)
                return 4
            time.sleep(0.2)

    state = {"proc": None}

    def forward(signum, _frame):
        # When we are killed (e.g. by the matrix `timeout`), take the
        # emulator down with us instead of orphaning it.
        proc = state["proc"]
        if proc is not None and proc.poll() is None:
            proc.kill()
        raise SystemExit(128 + signum)

    signal.signal(signal.SIGTERM, forward)
    signal.signal(signal.SIGINT, forward)

    try:
        state["proc"] = subprocess.Popen(argv[1:])
        rc = state["proc"].wait()
    finally:
        server.shutdown()
        server.server_close()

    return rc


if __name__ == "__main__":
    sys.exit(main(sys.argv))
