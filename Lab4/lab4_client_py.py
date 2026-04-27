import socket
import struct
import time
import random
import array
import sys

CMD_CONFIG = 0x01
CMD_SEND_DATA = 0x02
CMD_START = 0x03
CMD_STATUS = 0x04
CMD_GET_RESULT = 0x05
CMD_DISCONNECT = 0x06


def recv_all(sock, length):
    data = bytearray()
    while len(data) < length:
        packet = sock.recv(length - len(data))
        if not packet:
            return None
        data.extend(packet)
    return data


def send_command(sock, cmd_id, payload=b''):
    header = struct.pack('>II', cmd_id, len(payload))
    sock.sendall(header)
    if payload:
        sock.sendall(payload)


def print_server_response(sock):
    try:
        response = sock.recv(1024)
        if response:
            print(f"[Server] {response.decode('utf-8', errors='ignore')}")
        else:
            print("[Server] Connection closed by server.")
    except Exception as e:
        print(f"[Error] Failed to read response: {e}")


def main():
    HOST = '127.0.0.1'
    PORT = 8080
    N = 5000

    print("[Client] Starting Python Client...")

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as client_socket:
        try:
            client_socket.connect((HOST, PORT))
            print(f"[Client] Connected to the server at {HOST}:{PORT}.")
        except ConnectionRefusedError:
            print("[Error] Connection failed! Start the C++ server first.")
            return

        #INITIAL CONFIG
        print(f"[Client] Sending initial config (N={N})...")
        # >ii означає: Big Endian, два int32_t (n та threads)
        config_payload = struct.pack('>ii', N, 1)
        send_command(client_socket, CMD_CONFIG, config_payload)
        print_server_response(client_socket)  # Очікуємо "OK"

        #DATA PREPARATION
        print(f"[Client] Generating random data for N={N}...")
        # Використовуємо модуль array ('d' = double 64-bit) для швидкості і C++ сумісності
        # Він зберігає дані в пам'яті як сирі байти (Little Endian на x86)
        matrix = array.array('d', (random.uniform(1.0, 10.0) for _ in range(N * N)))
        vec_b = array.array('d', (random.uniform(1.0, 10.0) for _ in range(N)))

        matrix_bytes = matrix.tobytes()
        vec_b_bytes = vec_b.tobytes()
        data_payload = matrix_bytes + vec_b_bytes

        print(f"[Client] Uploading matrix and vector ({len(data_payload)} bytes)...")
        send_command(client_socket, CMD_SEND_DATA, data_payload)
        print_server_response(client_socket)

        thread_configs = [3, 6, 12, 24, 48, 96]

        for threads in thread_configs:
            #CONFIG
            config_payload = struct.pack('>ii', N, threads)
            send_command(client_socket, CMD_CONFIG, config_payload)
            print_server_response(client_socket)

            #START
            send_command(client_socket, CMD_START)
            print_server_response(client_socket)

            #STATUS POLLING
            while True:
                send_command(client_socket, CMD_STATUS)
                status_bytes = recv_all(client_socket, 4)

                if not status_bytes:
                    print("\n[Error] Lost connection to server during polling.")
                    return

                status = struct.unpack('<i', status_bytes)[0]

                status_text = {0: "IDLE", 1: "IN_PROGRESS", 2: "READY", 3: "ERROR_STATE"}.get(status, "UNKNOWN")

                sys.stdout.write(f"\r[Client] Status: {status_text} ({status})    ")
                sys.stdout.flush()

                if status == 2:
                    print()
                    break

                time.sleep(0.1)

            #GET_RESULT & TIME
            send_command(client_socket, CMD_GET_RESULT)

            time_bytes = recv_all(client_socket, 8)
            execution_time = struct.unpack('<d', time_bytes)[0]

            result_bytes = recv_all(client_socket, N * 8)


            print(f"{threads}\t| {execution_time:.6f} s")

        print("-------------------------------")
        print("[Client] Benchmarking finished. Disconnecting.")

        #DISCONNECT ---
        send_command(client_socket, CMD_DISCONNECT)
        print_server_response(client_socket)


if __name__ == "__main__":
    main()