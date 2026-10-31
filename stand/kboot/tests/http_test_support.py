import socket
import ssl
import subprocess
import threading


SERVER_HOST = "127.0.0.1"


def server_url(server, path, scheme="http", host=SERVER_HOST):
    return f"{scheme}://{host}:{server.port}{path}"


class HTTPResponseServer:
    def __init__(self, responses, tls_context=None):
        self._responses = [
            [response] if isinstance(response, bytes) else list(response)
            for response in responses
        ]
        self._tls_context = tls_context
        self.requests = []
        self.error = None
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._sock.bind((SERVER_HOST, 0))
        self._sock.listen()
        self._sock.settimeout(5)
        self.port = self._sock.getsockname()[1]
        self._thread = threading.Thread(target=self._serve, daemon=True)

    def __enter__(self):
        self._thread.start()
        return self

    def __exit__(self, exc_type, exc, tb):
        self._thread.join(timeout=6)
        self._sock.close()
        if exc_type is None and self.error is not None:
            raise self.error

    def _serve(self):
        try:
            for response in self._responses:
                try:
                    conn, _ = self._sock.accept()
                except socket.timeout:
                    break

                conn.settimeout(5)
                try:
                    if self._tls_context is not None:
                        conn = self._tls_context.wrap_socket(
                            conn, server_side=True
                        )
                    with conn:
                        self.requests.append(read_request(conn))
                        for chunk in response:
                            conn.sendall(chunk)
                except (ConnectionError, OSError, ssl.SSLError):
                    conn.close()
        except Exception as exc:
            self.error = exc
        finally:
            self._sock.close()


class ClosingTCPServer:
    def __init__(self):
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._sock.bind((SERVER_HOST, 0))
        self._sock.listen()
        self._sock.settimeout(5)
        self.port = self._sock.getsockname()[1]
        self._thread = threading.Thread(target=self._serve, daemon=True)

    def __enter__(self):
        self._thread.start()
        return self

    def __exit__(self, exc_type, exc, tb):
        self._thread.join(timeout=6)
        self._sock.close()

    def _serve(self):
        try:
            conn, _ = self._sock.accept()
            conn.close()
        except (OSError, socket.timeout):
            pass
        finally:
            self._sock.close()


class StalledTCPServer:
    def __init__(self):
        self._stop = threading.Event()
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._sock.bind((SERVER_HOST, 0))
        self._sock.listen()
        self._sock.settimeout(5)
        self.port = self._sock.getsockname()[1]
        self._thread = threading.Thread(target=self._serve, daemon=True)

    def __enter__(self):
        self._thread.start()
        return self

    def __exit__(self, exc_type, exc, tb):
        self._stop.set()
        self._thread.join(timeout=6)
        self._sock.close()

    def _serve(self):
        try:
            conn, _ = self._sock.accept()
            try:
                with conn:
                    self._stop.wait(timeout=20)
            except OSError:
                pass
        except (OSError, socket.timeout):
            pass
        finally:
            self._sock.close()


class StalledHTTPServer:
    def __init__(self, response_prefix=b""):
        self._response_prefix = response_prefix
        self._stop = threading.Event()
        self.requests = []
        self.error = None
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._sock.bind((SERVER_HOST, 0))
        self._sock.listen()
        self._sock.settimeout(5)
        self.port = self._sock.getsockname()[1]
        self._thread = threading.Thread(target=self._serve, daemon=True)

    def __enter__(self):
        self._thread.start()
        return self

    def __exit__(self, exc_type, exc, tb):
        self._stop.set()
        self._thread.join(timeout=6)
        self._sock.close()
        if exc_type is None and self.error is not None:
            raise self.error

    def _serve(self):
        try:
            conn, _ = self._sock.accept()
            conn.settimeout(5)
            try:
                with conn:
                    self.requests.append(read_request(conn))
                    if self._response_prefix:
                        conn.sendall(self._response_prefix)
                    self._stop.wait(timeout=20)
            except (ConnectionError, OSError):
                pass
        except (OSError, socket.timeout):
            pass
        except Exception as exc:
            self.error = exc
        finally:
            self._sock.close()


def create_tls_server_context(certfile, keyfile):
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = ssl.TLSVersion.TLSv1_2
    context.maximum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(certfile, keyfile)
    return context


def read_request(conn):
    data = bytearray()
    while b"\r\n\r\n" not in data:
        chunk = conn.recv(4096)
        if not chunk:
            break
        data.extend(chunk)
    return bytes(data)


def run_client(loader_bin, tmp_path, url, timeout=5):
    return subprocess.run(
        [str(loader_bin), "--http-test-get", url],
        cwd=tmp_path,
        text=True,
        capture_output=True,
        timeout=timeout,
    )


def captured_output(proc):
    return proc.stdout + proc.stderr


def assert_basic_get_request(request, path, port, host="127.0.0.1"):
    text = request.decode("ascii")
    assert text.startswith(f"GET {path} HTTP/1.1\r\n")
    assert f"Host: {host}:{port}\r\n" in text
    assert "User-Agent: kboot-http-client/0.1\r\n" in text
    assert "Accept: */*\r\n" in text
    assert "Accept-Encoding: identity\r\n" in text
    assert "Connection: close\r\n" in text
