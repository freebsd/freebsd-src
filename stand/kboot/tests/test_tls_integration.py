import pytest

from http_test_support import (
    ClosingTCPServer,
    HTTPResponseServer,
    StalledTCPServer,
    assert_basic_get_request,
    captured_output,
    create_tls_server_context,
    run_client,
    server_url,
)


@pytest.fixture(scope="session")
def tls_material(freebsd_src):
    source_dir = freebsd_src / "crypto/openssl/demos/guide"
    source_ca = source_dir / "rootcert.pem"
    server_cert = source_dir / "servercert.pem"
    server_key = source_dir / "serverkey.pem"

    for path in (source_ca, server_cert, server_key):
        if not path.is_file():
            pytest.fail(f"missing FreeBSD TLS test fixture: {path}")

    return {
        "cert": server_cert,
        "key": server_key,
    }


def test_https_get(loader_kboot_bin, tls_material, tmp_path):
    response = b"HTTP/1.1 200 OK\r\nContent-Length: 6\r\n\r\nsecure"
    context = create_tls_server_context(
        tls_material["cert"], tls_material["key"]
    )

    with HTTPResponseServer([response], tls_context=context) as server:
        proc = run_client(
            loader_kboot_bin,
            tmp_path,
            server_url(server, "/secure", scheme="https", host="localhost"),
        )

    assert proc.returncode == 0, captured_output(proc)
    assert (tmp_path / "secure").read_bytes() == b"secure"
    assert_basic_get_request(
        server.requests[0], "/secure", server.port, host="localhost"
    )


def test_https_rejects_hostname_mismatch(
    loader_kboot_bin, tls_material, tmp_path
):
    response = b"HTTP/1.1 200 OK\r\nContent-Length: 6\r\n\r\nsecure"
    context = create_tls_server_context(
        tls_material["cert"], tls_material["key"]
    )

    with HTTPResponseServer([response], tls_context=context) as server:
        proc = run_client(
            loader_kboot_bin,
            tmp_path,
            server_url(server, "/mismatch", scheme="https"),
        )

    assert proc.returncode != 0
    assert not (tmp_path / "mismatch").exists()


def test_https_fragmented_response(loader_kboot_bin, tls_material, tmp_path):
    response = b"HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nfragmented"
    fragments = [response[i : i + 1] for i in range(len(response))]
    context = create_tls_server_context(
        tls_material["cert"], tls_material["key"]
    )

    with HTTPResponseServer([fragments], tls_context=context) as server:
        proc = run_client(
            loader_kboot_bin,
            tmp_path,
            server_url(
                server, "/fragmented", scheme="https", host="localhost"
            ),
        )

    assert proc.returncode == 0, captured_output(proc)
    assert (tmp_path / "fragmented").read_bytes() == b"fragmented"


def test_https_handshake_close_is_observable(loader_kboot_bin, tmp_path):
    with ClosingTCPServer() as server:
        proc = run_client(
            loader_kboot_bin,
            tmp_path,
            server_url(server, "/closed", scheme="https", host="localhost"),
        )

    assert proc.returncode != 0
    assert not (tmp_path / "closed").exists()


def test_https_handshake_stall_times_out(loader_kboot_bin, tmp_path):
    with StalledTCPServer() as server:
        proc = run_client(
            loader_kboot_bin,
            tmp_path,
            server_url(server, "/stall", scheme="https", host="localhost"),
            timeout=20,
        )

    assert proc.returncode != 0
    assert "http_client: io: sending request" in captured_output(proc)
    assert not (tmp_path / "stall").exists()


def test_http_redirect_to_https(loader_kboot_bin, tls_material, tmp_path):
    final = b"HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nfinal"
    context = create_tls_server_context(
        tls_material["cert"], tls_material["key"]
    )

    with HTTPResponseServer([final], tls_context=context) as tls_server:
        location = server_url(
            tls_server, "/final", scheme="https", host="localhost"
        )
        redirect = (
            "HTTP/1.1 302 Found\r\n"
            f"Location: {location}\r\n"
            "Content-Length: 0\r\n"
            "\r\n"
        ).encode("ascii")
        with HTTPResponseServer([redirect]) as http_server:
            proc = run_client(
                loader_kboot_bin,
                tmp_path,
                server_url(http_server, "/redirect"),
            )

    assert proc.returncode == 0, captured_output(proc)
    assert (tmp_path / "redirect").read_bytes() == b"final"
    assert_basic_get_request(
        tls_server.requests[0],
        "/final",
        tls_server.port,
        host="localhost",
    )
