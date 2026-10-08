import socket
import threading
import time
import subprocess
import os
import signal

SERVER_BIN = "./build/miniredis_server"
WAL_FILE = "miniredis.wal"
NUM_THREADS = 10
REQUESTS_PER_THREAD = 2000

def cleanup():
    if os.path.exists(WAL_FILE):
        os.remove(WAL_FILE)

def run_worker(cmd_bytes, num_requests, results, index):
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('127.0.0.1', 6379))
        start_t = time.time()
        for _ in range(num_requests):
            s.sendall(cmd_bytes)
            res = s.recv(1024)
        duration = time.time() - start_t
        results[index] = duration
        s.close()
    except Exception as e:
        print(f"Worker error: {e}")

def benchmark_command(name, cmd_bytes):
    threads = []
    results = [0.0] * NUM_THREADS
    start_total = time.time()

    for i in range(NUM_THREADS):
        t = threading.Thread(target=run_worker, args=(cmd_bytes, REQUESTS_PER_THREAD, results, i))
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    total_time = time.time() - start_total
    total_ops = NUM_THREADS * REQUESTS_PER_THREAD
    rps = total_ops / total_time
    avg_lat_ms = (total_time / total_ops) * 1000

    print(f"{name:<15} | Throughput: {rps:>10.2f} req/sec | Avg Latency: {avg_lat_ms:>6.3f} ms")
    return rps, avg_lat_ms

def main():
    cleanup()
    proc = subprocess.Popen([SERVER_BIN], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(1)

    print("=====================================================================")
    print(f" MiniRedis Benchmark (Clients: {NUM_THREADS}, Total Req: {NUM_THREADS * REQUESTS_PER_THREAD})")
    print("=====================================================================")

    try:
        benchmark_command("PING", b"*1\r\n$4\r\nPING\r\n")
        benchmark_command("SET", b"*3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$3\r\nval\r\n")
        benchmark_command("GET", b"*2\r\n$3\r\nGET\r\n$3\r\nkey\r\n")
        benchmark_command("INCR", b"*2\r\n$4\r\nINCR\r\n$7\r\ncounter\r\n")
        print("=====================================================================")
    finally:
        os.kill(proc.pid, signal.SIGKILL)
        proc.wait()
        cleanup()

if __name__ == "__main__":
    main()
