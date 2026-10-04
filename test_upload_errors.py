import socket
import time
import uuid
from pathlib import Path

HOST = "127.0.0.1"
PORT = 9410
SID = "SID:9360"
STORAGE = Path("agentfiles/IT24100639")


def read_line(connection):
    data = bytearray()
    while True:
        character = connection.recv(1)
        if not character:
            raise AssertionError("Connection closed before a complete response")
        if character == b"\n":
            return data.decode()
        data.extend(character)
        if len(data) > 4096:
            raise AssertionError("Response is too long")


def connect_authenticated():
    connection = socket.create_connection((HOST, PORT), timeout=5)
    try:
        connection.sendall(b"AUTH OPS-0639\n")
        response = read_line(connection)
        assert response == f"OK AUTHENTICATED {SID}", response
        return connection
    except Exception:
        connection.close()
        raise


def test_oversized_upload():
    filename = f"oversized_{uuid.uuid4().hex}.bin"

    with connect_authenticated() as connection:
        connection.sendall(
            f"PUT {filename} 104857601\n".encode()
        )
        response = read_line(connection)
        print("Agent response:", response)
        assert response == f"ERR 004 FILE_TOO_LARGE {SID}", response
        assert connection.recv(1) == b"", "Agent did not close the connection"

    assert not (STORAGE / filename).exists(), "Oversized file was created"
    print("PASS: Upload above 100 MiB rejected; connection closed; no file created.")


def test_interrupted_upload():
    filename = f"interrupted_{uuid.uuid4().hex}.bin"
    previous_temporary_files = set(STORAGE.glob(".upload-*"))

    with connect_authenticated() as connection:
        connection.sendall(
            f"PUT {filename} 8192\n".encode() + b"partial data"
        )
        connection.shutdown(socket.SHUT_WR)
        assert connection.recv(1) == b"", "Expected Agent to close incomplete upload"

    deadline = time.monotonic() + 5
    while True:
        new_temporary_files = (
            set(STORAGE.glob(".upload-*")) - previous_temporary_files
        )
        if not new_temporary_files:
            break
        if time.monotonic() >= deadline:
            raise AssertionError("Incomplete temporary upload was not removed")
        time.sleep(0.1)

    assert not (STORAGE / filename).exists(), "Incomplete final file exists"
    print("PASS: Interrupted upload removed; no incomplete final or temporary file.")


def test_agent_still_works():
    with connect_authenticated() as connection:
        connection.sendall(b"SYSINFO\n")
        response = read_line(connection)
        assert response.startswith("OK SYSINFO "), response
        assert response.endswith(f" {SID}"), response
        print("Agent response:", response)

        connection.sendall(b"QUIT\n")
        assert read_line(connection) == f"OK BYE {SID}"

    print("PASS: Agent accepts a new connection and responds after both tests.")


if __name__ == "__main__":
    assert STORAGE.is_dir(), "Run this script from ~/IE3090_RemoteOps"
    test_oversized_upload()
    test_interrupted_upload()
    test_agent_still_works()
    print("\nALL THREE TESTS PASSED")
