import socket
import time
import uuid

from test_upload_errors import (
    HOST, PORT, SID, STORAGE,
    read_line, connect_authenticated,
)


def check_sysinfo(connection):
    response = read_line(connection)
    assert response.startswith("OK SYSINFO "), response
    assert response.endswith(f" {SID}"), response


def read_exact(connection, size):
    data = bytearray()
    while len(data) < size:
        chunk = connection.recv(size - len(data))
        assert chunk, "Connection closed during file download"
        data.extend(chunk)
    return bytes(data)


def test_split_command():
    with socket.create_connection((HOST, PORT), timeout=5) as connection:
        for part in (b"AU", b"TH OPS-", b"0639", b"\n"):
            connection.sendall(part)
            time.sleep(0.05)

        assert read_line(connection) == f"OK AUTHENTICATED {SID}"

        for part in (b"SYS", b"INFO", b"\n"):
            connection.sendall(part)
            time.sleep(0.05)

        check_sysinfo(connection)
        connection.sendall(b"QUIT\n")
        assert read_line(connection) == f"OK BYE {SID}"

    print("PASS: AUTH and SYSINFO work when split across separate sends.")


def test_combined_commands():
    with connect_authenticated() as connection:
        connection.sendall(b"SYSINFO\nEXEC WHOAMI\nQUIT\n")

        check_sysinfo(connection)
        response = read_line(connection)
        assert response.startswith("OK EXEC_RESULT "), response
        assert response.endswith(f" {SID}"), response
        assert read_line(connection) == f"OK BYE {SID}"

    print("PASS: Three commands sent together receive separate ordered replies.")


def test_file_boundaries():
    filename = f"stream_{uuid.uuid4().hex}.bin"
    payload = bytes(range(256)) * 32
    stored_file = STORAGE / filename

    try:
        with connect_authenticated() as connection:
            header = f"PUT {filename} {len(payload)}\n".encode()
            connection.sendall(header + payload + b"SYSINFO\n")

            assert read_line(connection) == (
                f"OK FILE_RECEIVED {filename} {SID}"
            )
            check_sysinfo(connection)
            assert stored_file.read_bytes() == payload

            connection.sendall(f"GET {filename}\nSYSINFO\nQUIT\n".encode())

            assert read_line(connection) == (
                f"OK FILE_SEND {filename} {len(payload)} {SID}"
            )
            downloaded = read_exact(connection, len(payload))
            assert downloaded == payload, "Downloaded binary data differs"

            check_sysinfo(connection)
            assert read_line(connection) == f"OK BYE {SID}"

        print("PASS: File headers, binary payloads and following commands stay separate.")
    finally:
        if stored_file.exists():
            stored_file.unlink()


if __name__ == "__main__":
    test_split_command()
    test_combined_commands()
    test_file_boundaries()
    print("\nALL THREE TCP STREAM TESTS PASSED")
