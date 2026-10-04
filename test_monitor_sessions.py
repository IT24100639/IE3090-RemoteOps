import socket
import time
from contextlib import ExitStack

from test_upload_errors import SID, read_line, connect_authenticated


def command(connection, text, expected):
    connection.sendall((text + "\n").encode())
    response = read_line(connection)
    assert response == f"{expected} {SID}", response


def check_sysinfo(connection):
    connection.sendall(b"SYSINFO\n")
    response = read_line(connection)
    assert response.startswith("OK SYSINFO "), response
    assert response.endswith(f" {SID}"), response


def udp_listener(stack):
    listener = stack.enter_context(
        socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    )
    listener.bind(("127.0.0.1", 0))
    listener.settimeout(5)
    return listener


def receive_stats(listener):
    listener.settimeout(5)
    data, peer = listener.recvfrom(4096)
    text = data.decode()
    fields = text.split()

    assert peer[0] == "127.0.0.1", peer
    assert text.endswith("\n"), text
    assert len(fields) == 5, text
    assert fields[0] == "SYSINFO", text
    assert fields[4] == SID, text
    assert float(fields[1]) >= 0, text
    assert float(fields[2]) >= 0, text
    assert int(fields[3]) >= 0, text


def check_silent(listener):
    # Allow cleanup to finish, then discard already queued datagrams.
    time.sleep(0.3)
    listener.setblocking(False)
    deadline = time.monotonic() + 1

    while True:
        try:
            listener.recvfrom(4096)
        except BlockingIOError:
            break
        assert time.monotonic() < deadline, "UDP queue did not drain"

    # Observe for longer than the configured two-second interval.
    listener.settimeout(3)
    try:
        listener.recvfrom(4096)
    except socket.timeout:
        return
    raise AssertionError("Monitoring continued after it should have stopped")


def start_monitor(connection, listener):
    port = listener.getsockname()[1]
    command(connection, f"MONITOR START {port}", "OK MONITOR_STARTED")
    receive_stats(listener)


def main():
    with ExitStack() as stack:
        clients = [
            stack.enter_context(connect_authenticated())
            for _ in range(5)
        ]

        for connection in clients:
            check_sysinfo(connection)

        print("PASS: Five authenticated clients remain connected and answer SYSINFO.",
              flush=True)

        first = udp_listener(stack)
        second = udp_listener(stack)

        start_monitor(clients[0], first)
        start_monitor(clients[1], second)
        print("PASS: Two client sessions receive valid UDP statistics on separate ports.",
              flush=True)

        command(clients[0], "MONITOR STOP", "OK MONITOR_STOPPED")
        check_silent(first)
        receive_stats(second)
        check_sysinfo(clients[0])
        print("PASS: MONITOR STOP stops only its own session; TCP still works.",
              flush=True)

        start_monitor(clients[0], first)
        command(clients[0], "QUIT", "OK BYE")
        assert clients[0].recv(1) == b"", "QUIT did not close TCP"
        check_silent(first)
        receive_stats(second)
        print("PASS: QUIT stops that session's UDP; the other session continues.",
              flush=True)

        clients[1].close()
        check_silent(second)
        print("PASS: Closing TCP without QUIT also stops that session's UDP.",
              flush=True)

        for connection in clients[2:]:
            check_sysinfo(connection)
            command(connection, "QUIT", "OK BYE")

        print("PASS: The remaining three clients continue working.",
              flush=True)

    print("\nALL MONITOR SESSION TESTS PASSED")


if __name__ == "__main__":
    main()
