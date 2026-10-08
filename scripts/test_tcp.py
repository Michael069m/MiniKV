import socket
import time
import sys

def main():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('127.0.0.1', 6379))

    # Test PING
    s.sendall(b"*1\r\n$4\r\nPING\r\n")
    data = s.recv(1024)
    assert data == b"+PONG\r\n", f"Expected +PONG\\r\\n, got {data}"
    print("✓ PING -> +PONG")

    # Test SET
    s.sendall(b"*3\r\n$3\r\nSET\r\n$4\r\nuser\r\n$5\r\nAlice\r\n")
    data = s.recv(1024)
    assert data == b"+OK\r\n", f"Expected +OK\\r\\n, got {data}"
    print("✓ SET user Alice -> +OK")

    # Test GET
    s.sendall(b"*2\r\n$3\r\nGET\r\n$4\r\nuser\r\n")
    data = s.recv(1024)
    assert data == b"$5\r\nAlice\r\n", f"Expected $5\\r\\nAlice\\r\\n, got {data}"
    print("✓ GET user -> $5\\r\\nAlice\\r\\n")

    # Test INCR
    s.sendall(b"*2\r\n$4\r\nINCR\r\n$7\r\ncounter\r\n")
    data = s.recv(1024)
    assert data == b":1\r\n", f"Expected :1\\r\\n, got {data}"
    print("✓ INCR counter -> :1")

    # Test EXISTS
    s.sendall(b"*2\r\n$6\r\nEXISTS\r\n$4\r\nuser\r\n")
    data = s.recv(1024)
    assert data == b":1\r\n", f"Expected :1\\r\\n, got {data}"
    print("✓ EXISTS user -> :1")

    # Test KEYS *
    s.sendall(b"*2\r\n$4\r\nKEYS\r\n$1\r\n*\r\n")
    data = s.recv(1024)
    assert b"user" in data and b"counter" in data, f"KEYS failed: {data}"
    print("✓ KEYS * -> returned keys")

    # Test DEL
    s.sendall(b"*2\r\n$3\r\nDEL\r\n$4\r\nuser\r\n")
    data = s.recv(1024)
    assert data == b":1\r\n", f"Expected :1\\r\\n, got {data}"
    print("✓ DEL user -> :1")

    s.close()
    print("All TCP network integration tests passed successfully!")

if __name__ == "__main__":
    main()
