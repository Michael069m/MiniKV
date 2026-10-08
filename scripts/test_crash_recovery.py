import socket
import subprocess
import time
import os
import signal
import sys

SERVER_BIN = "./build/miniredis_server"
WAL_FILE = "miniredis.wal"

def cleanup():
    if os.path.exists(WAL_FILE):
        os.remove(WAL_FILE)

def send_cmd(s, cmd_bytes):
    s.sendall(cmd_bytes)
    return s.recv(4096)

def main():
    cleanup()
    print("--- 1. Starting MiniRedis Server ---")
    proc = subprocess.Popen([SERVER_BIN], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(1)

    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('127.0.0.1', 6379))

        print("--- 2. Populating 500 keys over TCP ---")
        for i in range(500):
            cmd = f"*3\r\n$3\r\nSET\r\n$7\r\nkey_{i:03d}\r\n$9\r\nvalue_{i:03d}\r\n".encode()
            res = send_cmd(s, cmd)
            assert res == b"+OK\r\n", f"Set failed at key_{i}"

        s.close()
        print("✓ Successfully populated 500 keys.")

        print("--- 3. Simulating ungraceful crash (kill -9) ---")
        os.kill(proc.pid, signal.SIGKILL)
        proc.wait()
        print("✓ Server killed abruptly mid-execution.")

        print("--- 4. Restarting MiniRedis Server (Triggering WAL Replay) ---")
        proc = subprocess.Popen([SERVER_BIN], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(1)

        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('127.0.0.1', 6379))

        print("--- 5. Verifying data integrity post-recovery ---")
        for i in range(500):
            cmd = f"*2\r\n$3\r\nGET\r\n$7\r\nkey_{i:03d}\r\n".encode()
            res = send_cmd(s, cmd)
            expected = f"$9\r\nvalue_{i:03d}\r\n".encode()
            assert res == expected, f"Mismatch at key_{i}: expected {expected}, got {res}"

        s.close()
        print("✓ All 500 keys verified perfectly after crash recovery!")

    finally:
        if proc.poll() is None:
            os.kill(proc.pid, signal.SIGKILL)
            proc.wait()
        cleanup()
        print("--- Crash Recovery Integration Test PASSED ---")

if __name__ == "__main__":
    main()
