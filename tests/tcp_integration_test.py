import socket


def receive_line(client, pending):
    while b"\n" not in pending:
        chunk = client.recv(1024)

        if not chunk:
            raise ConnectionError("Server closed the connection unexpectedly.")

        pending += chunk

    line, pending = pending.split(b"\n", 1)
    return line + b"\n", pending


with socket.create_connection(("127.0.0.1", 6379), timeout=2) as client:
    pending = b""

    client.sendall(b"SET name Alex\n")
    response, pending = receive_line(client, pending)
    assert response == b"OK\n", response

    client.sendall(b"GET name\n")
    response, pending = receive_line(client, pending)
    assert response == b"Alex\n", response

    client.sendall(b"SET city Seattle\n")
    response, pending = receive_line(client, pending)
    assert response == b"OK\n", response

    client.sendall(b"GET city\n")
    response, pending = receive_line(client, pending)
    assert response == b"Seattle\n", response

    client.sendall(b"DEL name\n")
    response, pending = receive_line(client, pending)
    assert response == b"OK\n", response

    client.sendall(b"GET name\n")
    response, pending = receive_line(client, pending)
    assert response == b"NOT_FOUND\n", response

print("TCP integration test passed.")