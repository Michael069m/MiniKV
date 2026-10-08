import socket
import time

def main():
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('127.0.0.1', 6379))

    # Test PING
    s.sendall(b"*1\r\n$4\r\nPING\r\n")
    data = s.recv(1024)
    assert data == b"+PONG\r\n", f"Expected +PONG\\r\\n, got {data}"
    print("✓ PING -> +PONG")

    # Test SET with EX
    s.sendall(b"*5\r\n$3\r\nSET\r\n$4\r\ntemp\r\n$3\r\nval\r\n$2\r\nEX\r\n$1\r\n1\r\n")
    data = s.recv(1024)
    assert data == b"+OK\r\n", f"Expected +OK\\r\\n, got {data}"
    print("✓ SET temp val EX 1 -> +OK")

    # Test TTL
    s.sendall(b"*2\r\n$3\r\nTTL\r\n$4\r\ntemp\r\n")
    data = s.recv(1024)
    assert data.startswith(b":1") or data.startswith(b":0"), f"Expected positive TTL, got {data}"
    print(f"✓ TTL temp -> {data.strip().decode()}")

    # Sleep to let key expire
    time.sleep(1.1)

    # Test GET on expired key
    s.sendall(b"*2\r\n$3\r\nGET\r\n$4\r\ntemp\r\n")
    data = s.recv(1024)
    assert data == b"$-1\r\n", f"Expected $-1\\r\\n (nil), got {data}"
    print("✓ GET temp (after expiry) -> $-1\\r\\n (nil)")

    # Test EXPIRE command
    s.sendall(b"*3\r\n$3\r\nSET\r\n$7\r\nsession\r\n$6\r\nactive\r\n")
    s.recv(1024)
    s.sendall(b"*3\r\n$6\r\nEXPIRE\r\n$7\r\nsession\r\n$1\r\n1\r\n")
    data = s.recv(1024)
    assert data == b":1\r\n", f"Expected :1\\r\\n, got {data}"
    print("✓ EXPIRE session 1 -> :1")

    time.sleep(1.1)

    # Test TTL on expired key
    s.sendall(b"*2\r\n$3\r\nTTL\r\n$7\r\nsession\r\n")
    data = s.recv(1024)
    assert data == b":-2\r\n", f"Expected :-2\\r\\n, got {data}"
    print("✓ TTL session (after expiry) -> :-2")

    s.close()
    print("All TCP network integration tests (including Expiry) passed successfully!")

if __name__ == "__main__":
    main()
