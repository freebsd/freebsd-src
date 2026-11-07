import pytest

from http_test_support import (
    HTTPResponseServer,
    StalledHTTPServer,
    assert_basic_get_request,
    captured_output,
    run_client,
    server_url,
)


def run_http_client(loader_kboot_bin, tmp_path, server, path, timeout=5):
    return run_client(
        loader_kboot_bin,
        tmp_path,
        server_url(server, path),
        timeout=timeout,
    )


def test_http_get_with_content_length(loader_kboot_bin, tmp_path):
    response = (
        b"HTTP/1.1 200 OK\r\n"
        b"Server: test-server/fake\r\n"
        b"Content-Length: 6\r\n"
        b"Connection: close\r\n"
        b"\r\n"
        b"-foo-\n"
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/get")

    # Curl seed: curl/tests/http/test_01_basic.py::TestBasic::test_01_01_http_get
    # and curl/tests/http/test_02_download.py::TestDownload::test_02_01_download_1.
    assert proc.returncode == 0, captured_output(proc)
    assert_basic_get_request(server.requests[0], "/get", server.port)
    assert (tmp_path / "get").read_bytes() == b"-foo-\n"


def test_percent_encoded_space_in_url_is_accepted(loader_kboot_bin, tmp_path):
    response = (
        b"HTTP/1.1 200 OK\r\n"
        b"Content-Length: 8\r\n"
        b"\r\n"
        b"encoded\n"
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/has%20space")

    assert proc.returncode == 0, captured_output(proc)
    assert_basic_get_request(server.requests[0], "/has%20space", server.port)
    assert (tmp_path / "has%20space").read_bytes() == b"encoded\n"


@pytest.mark.parametrize("path", ["/has space", "/bad\r\nHost: injected"])
def test_raw_space_or_control_in_url_is_rejected(
    loader_kboot_bin, tmp_path, path
):
    url = f"http://127.0.0.1:1{path}"

    proc = run_client(loader_kboot_bin, tmp_path, url)

    assert proc.returncode == 10
    assert "http_client: url.invalid" in captured_output(proc)


def test_identity_content_encoding_is_accepted(loader_kboot_bin, tmp_path):
    response = (
        b"HTTP/1.1 200 OK\r\n"
        b"Content-Encoding: identity\r\n"
        b"Content-Length: 6\r\n"
        b"Connection: close\r\n"
        b"\r\n"
        b"-foo-\n"
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/identity")

    assert proc.returncode == 0, captured_output(proc)
    assert_basic_get_request(server.requests[0], "/identity", server.port)
    assert (tmp_path / "identity").read_bytes() == b"-foo-\n"


def test_unsupported_content_encoding_is_rejected(loader_kboot_bin, tmp_path):
    response = (
        b"HTTP/1.1 200 OK\r\n"
        b"Content-Encoding: gzip\r\n"
        b"Content-Length: 6\r\n"
        b"Connection: close\r\n"
        b"\r\n"
        b"-foo-\n"
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/gzip")

    assert proc.returncode == 21
    assert (
        "http_client: response.unsupported: unsupported Content-Encoding"
        in captured_output(proc)
    )
    assert_basic_get_request(server.requests[0], "/gzip", server.port)
    assert not (tmp_path / "gzip").exists()


def test_chunked_response(loader_kboot_bin, tmp_path):
    response = (
        b"HTTP/1.1 200 funky chunky!\r\n"
        b"Server: fakeit/0.9 fakeitbad/1.0\r\n"
        b"Transfer-Encoding: chunked\r\n"
        b"Connection: close\r\n"
        b"\r\n"
        b"4\r\n"
        b"aaaa\r\n"
        b"3\r\n"
        b"bbb\r\n"
        b"5;heresatest=moooo\r\n"
        b"ccccc\r\n"
        b"0\r\n"
        b"\r\n"
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/chunked")

    # Curl seed: curl/tests/data/test339 sends a chunked HTTP/1.1 response with
    # fake server headers and chunk extensions.
    assert proc.returncode == 0, captured_output(proc)
    assert_basic_get_request(server.requests[0], "/chunked", server.port)
    assert (tmp_path / "chunked").read_bytes() == b"aaaabbbccccc"


def test_chunked_response_ignores_trailers(loader_kboot_bin, tmp_path):
    response = (
        b"HTTP/1.1 200 funky chunky!\r\n"
        b"Server: fakeit/0.9 fakeitbad/1.0\r\n"
        b"Transfer-Encoding: chunked\r\n"
        b"Connection: close\r\n"
        b"\r\n"
        b"4\r\n"
        b"aaaa\r\n"
        b"3\r\n"
        b"bbb\r\n"
        b"0\r\n"
        b"chunky-trailer: header data\r\n"
        b"another-header: yes\r\n"
        b"\r\n"
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/chunked-trailers")

    # Curl seed: curl/tests/data/test1116 sends chunk trailers after the
    # terminating zero-size chunk.
    assert proc.returncode == 0, captured_output(proc)
    assert_basic_get_request(server.requests[0], "/chunked-trailers", server.port)
    assert (tmp_path / "chunked-trailers").read_bytes() == b"aaaabbb"


def test_connection_close_body(loader_kboot_bin, tmp_path):
    response = (
        b"HTTP/1.1 200 OK\r\n"
        b"Server: test-server/fake\r\n"
        b"Connection: close\r\n"
        b"\r\n"
        b"body until close"
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/close-body")

    # Curl seed: curl/tests/http/test_05_errors.py::TestErrors::
    # test_05_04_unclean_tls_shutdown covers body end detection by close.
    assert proc.returncode == 0, captured_output(proc)
    assert_basic_get_request(server.requests[0], "/close-body", server.port)
    assert (tmp_path / "close-body").read_bytes() == b"body until close"


def test_connection_close_body_larger_than_read_buffer(loader_kboot_bin, tmp_path):
    body = b"Z" * 4097
    response = (
        b"HTTP/1.1 200 OK\r\n"
        b"Server: test-server/fake\r\n"
        b"Connection: close\r\n"
        b"\r\n"
        + body
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/large-close-body")

    # Curl seed: curl/tests/data/test559 has a 2049-byte HTTP body to cross
    # an internal read-buffer boundary.
    assert proc.returncode == 0, captured_output(proc)
    assert_basic_get_request(server.requests[0], "/large-close-body", server.port)
    assert (tmp_path / "large-close-body").read_bytes() == body


def test_duplicate_matching_content_length_is_accepted(loader_kboot_bin, tmp_path):
    response = (
        b"HTTP/1.1 200 OK\r\n"
        b"Server: test-server/fake\r\n"
        b"Content-Length: 6\r\n"
        b"Content-Length: 6\r\n"
        b"Connection: close\r\n"
        b"\r\n"
        b"-foo-\n"
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/dup-cl")

    # Curl seed: curl/tests/data/test767 accepts duplicate Content-Length
    # fields when they agree.
    assert proc.returncode == 0, captured_output(proc)
    assert_basic_get_request(server.requests[0], "/dup-cl", server.port)
    assert (tmp_path / "dup-cl").read_bytes() == b"-foo-\n"


def test_conflicting_content_length_is_rejected(loader_kboot_bin, tmp_path):
    response = (
        b"HTTP/1.1 200 OK\r\n"
        b"Server: test-server/fake\r\n"
        b"Content-Length: 44\r\n"
        b"Content-Length: 6\r\n"
        b"Connection: close\r\n"
        b"\r\n"
        b"-foo-\n"
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/conflicting-cl")

    # Curl seed: curl/tests/data/test771 rejects response headers with
    # conflicting Content-Length values.
    assert proc.returncode != 0
    assert "http_client: header.invalid_content_length" in captured_output(proc)
    assert_basic_get_request(server.requests[0], "/conflicting-cl", server.port)


def test_invalid_content_length_is_observable(loader_kboot_bin, tmp_path):
    response = (
        b"HTTP/1.1 200 OK\r\n"
        b"Content-Length: -6\r\n"
        b"\r\n"
        b"mooooo"
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/bad-cl")

    # Curl seed: curl/tests/data/test178 sends a negative Content-Length.
    assert proc.returncode != 0
    assert "http_client: header.invalid_content_length: -6" in captured_output(proc)
    assert_basic_get_request(server.requests[0], "/bad-cl", server.port)


def test_non_numeric_content_length_is_observable(loader_kboot_bin, tmp_path):
    response = (
        b"HTTP/1.1 200 OK\r\n"
        b"Content-Length: abc\r\n"
        b"\r\n"
        b"mooooo"
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/bad-cl-alpha")

    # Curl seed: local diagnostic variant of curl/tests/data/test178's invalid
    # Content-Length coverage.
    assert proc.returncode != 0
    assert "http_client: header.invalid_content_length: abc" in captured_output(proc)
    assert_basic_get_request(server.requests[0], "/bad-cl-alpha", server.port)


def test_unsupported_transfer_encoding_is_observable(loader_kboot_bin, tmp_path):
    response = (
        b"HTTP/1.1 200 OK\r\n"
        b"Transfer-Encoding: gzip\r\n"
        b"\r\n"
        b"mooooo"
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/bad-te")

    # Curl seed: curl/tests/data/test1170 uses an unsupported gzip transfer
    # coding before chunked.
    assert proc.returncode != 0
    assert (
        "http_client: header.unsupported_transfer_encoding: gzip"
        in captured_output(proc)
    )
    assert_basic_get_request(server.requests[0], "/bad-te", server.port)


def test_combined_transfer_encoding_is_observable(loader_kboot_bin, tmp_path):
    response = (
        b"HTTP/1.1 200 OK\r\n"
        b"Transfer-Encoding: chunked, identity\r\n"
        b"Content-Length: 19\r\n"
        b"\r\n"
        b"stuff server sends"
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/bad-te-list")

    # Curl seed: curl/tests/data/test1495 rejects a combined
    # "chunked, identity" Transfer-Encoding value.
    assert proc.returncode != 0
    assert (
        "http_client: header.unsupported_transfer_encoding: chunked, identity"
        in captured_output(proc)
    )
    assert_basic_get_request(server.requests[0], "/bad-te-list", server.port)


def test_compressed_transfer_encoding_chain_is_observable(loader_kboot_bin, tmp_path):
    response = (
        b"HTTP/1.1 200 OK\r\n"
        b"Transfer-Encoding: gzip, chunked\r\n"
        b"\r\n"
        b"2c\r\n"
        b"compressed bytes would be here\r\n"
        b"0\r\n"
        b"\r\n"
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/bad-te-chain")

    # Curl seed: curl/tests/data/test1170 exercises a "gzip, chunked"
    # Transfer-Encoding chain.
    assert proc.returncode != 0
    assert (
        "http_client: header.unsupported_transfer_encoding: gzip, chunked"
        in captured_output(proc)
    )
    assert_basic_get_request(server.requests[0], "/bad-te-chain", server.port)


def test_malformed_header_line_is_observable(loader_kboot_bin, tmp_path):
    response = (
        b"HTTP/1.1 200 OK\r\n"
        b"Broken Header\r\n"
        b"\r\n"
        b"mooooo"
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/bad-header")

    # Local diagnostic seed: malformed response header with no colon, added to
    # make the parser's header.malformed branch observable.
    assert proc.returncode != 0
    assert "http_client: header.malformed: Broken Header" in captured_output(proc)
    assert_basic_get_request(server.requests[0], "/bad-header", server.port)


def test_truncated_content_length_body_is_observable(loader_kboot_bin, tmp_path):
    response = (
        b"HTTP/1.1 200 OK\r\n"
        b"Content-Length: 6\r\n"
        b"Connection: close\r\n"
        b"\r\n"
        b"abc"
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/short-body")

    # Local diagnostic seed: Content-Length promises more bytes than the server
    # sends, so an exact body read must be classified as a truncated body.
    assert proc.returncode != 0
    assert "http_client: body.truncated" in captured_output(proc)
    assert_basic_get_request(server.requests[0], "/short-body", server.port)
    assert not (tmp_path / "short-body").exists()


def test_too_long_response_header_is_rejected(loader_kboot_bin, tmp_path):
    response = (
        b"HTTP/1.1 200 OK\r\n"
        b"Content-Length: 6\r\n"
        b"Connection: close\r\n"
        b"Long: "
        + b"A" * 102400
        + b"\r\n"
        b"\r\n"
        b"-foo-\n"
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/huge-header")

    # Curl seed: curl/tests/data/test1154 rejects a 100K response header.
    assert proc.returncode != 0
    assert "http_client: header.too_large" in captured_output(proc)
    assert_basic_get_request(server.requests[0], "/huge-header", server.port)


def test_too_many_response_header_bytes_are_rejected(loader_kboot_bin, tmp_path):
    response = (
        b"HTTP/1.1 200 OK\r\n"
        b"Content-Length: 6\r\n"
        b"Connection: close\r\n"
        + b"Tiny: but many.\r\n" * 5001
        + b"\r\n"
        b"-foo-\n"
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/too-many-headers")

    # Local diagnostic seed: many individually small response headers exceed
    # the aggregate response header byte limit.
    assert proc.returncode != 0
    assert "http_client: header.too_large" in captured_output(proc)
    assert_basic_get_request(server.requests[0], "/too-many-headers", server.port)


def test_invalid_chunk_size_is_observable(loader_kboot_bin, tmp_path):
    response = (
        b"HTTP/1.1 200 funky chunky!\r\n"
        b"Transfer-Encoding: chunked\r\n"
        b"\r\n"
        b"2\r\n"
        b"a\n\r\n"
        b"ILLEGAL\r\n"
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/bad-chunk")

    # Curl seed: curl/tests/data/test36 covers bad chunked Transfer-Encoding.
    assert proc.returncode != 0
    assert "http_client: body.invalid_chunk_size" in captured_output(proc)
    assert_basic_get_request(server.requests[0], "/bad-chunk", server.port)


def test_overflow_chunk_size_is_rejected(loader_kboot_bin, tmp_path):
    response = (
        b"HTTP/1.1 200 funky chunky!\r\n"
        b"Transfer-Encoding: chunked\r\n"
        b"\r\n"
        + b"f" * 128
        + b"\r\n"
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/huge-chunk")

    assert proc.returncode != 0
    assert "http_client: body.invalid_chunk_size" in captured_output(proc)
    assert_basic_get_request(server.requests[0], "/huge-chunk", server.port)


def test_invalid_chunk_terminator_is_rejected(loader_kboot_bin, tmp_path):
    response = (
        b"HTTP/1.1 200 funky chunky!\r\n"
        b"Transfer-Encoding: chunked\r\n"
        b"\r\n"
        b"4\r\n"
        b"dataXX"
        b"0\r\n"
        b"\r\n"
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/bad-chunk-terminator")

    assert proc.returncode != 0
    assert "http_client: body.invalid_chunk_size" in captured_output(proc)
    assert_basic_get_request(server.requests[0], "/bad-chunk-terminator", server.port)


def test_chunked_takes_precedence_over_content_length(loader_kboot_bin, tmp_path):
    response = (
        b"HTTP/1.1 200 OK\r\n"
        b"Server: test-server/fake\r\n"
        b"Content-Length: 123456\r\n"
        b"Transfer-Encoding: chunked\r\n"
        b"Connection: close\r\n"
        b"\r\n"
        b"10\r\n"
        b"chunked data fun\r\n"
        b"0\r\n"
        b"\r\n"
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/te-wins")

    # Curl seed: curl/tests/data/test365 expects Transfer-Encoding: chunked
    # to override a conflicting Content-Length.
    assert proc.returncode == 0, captured_output(proc)
    assert_basic_get_request(server.requests[0], "/te-wins", server.port)
    assert (tmp_path / "te-wins").read_bytes() == b"chunked data fun"


def test_premature_chunked_close_is_truncated(loader_kboot_bin, tmp_path):
    response = (
        b"HTTP/1.1 200 funky chunky!\r\n"
        b"Server: fakeit/0.9 fakeitbad/1.0\r\n"
        b"Transfer-Encoding: chunked\r\n"
        b"Connection: close\r\n"
        b"\r\n"
        b"41\r\n"
        b"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
        b"\r\n"
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/partial-chunk")

    # Curl seed: curl/tests/data/test207 covers chunked Transfer-Encoding
    # closed before the terminating zero-size chunk.
    assert proc.returncode != 0
    assert "http_client: body.truncated" in captured_output(proc)
    assert_basic_get_request(server.requests[0], "/partial-chunk", server.port)


def test_timeout_waiting_for_status_line_is_observable(
    loader_kboot_bin, tmp_path
):
    with StalledHTTPServer() as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/no-status", timeout=20)

    assert proc.returncode != 0
    assert "http_client: io: reading status line" in captured_output(proc)
    assert_basic_get_request(server.requests[0], "/no-status", server.port)
    assert not (tmp_path / "no-status").exists()


def test_timeout_waiting_for_content_length_body_is_observable(
    loader_kboot_bin, tmp_path
):
    response_prefix = (
        b"HTTP/1.1 200 OK\r\n"
        b"Content-Length: 6\r\n"
        b"\r\n"
        b"abc"
    )

    with StalledHTTPServer(response_prefix) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/slow-body", timeout=20)

    assert proc.returncode != 0
    assert "http_client: io: reading body" in captured_output(proc)
    assert_basic_get_request(server.requests[0], "/slow-body", server.port)


def test_timeout_waiting_for_chunk_body_is_observable(
    loader_kboot_bin, tmp_path
):
    response_prefix = (
        b"HTTP/1.1 200 OK\r\n"
        b"Transfer-Encoding: chunked\r\n"
        b"\r\n"
        b"6\r\n"
        b"abc"
    )

    with StalledHTTPServer(response_prefix) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/slow-chunk", timeout=20)

    assert proc.returncode != 0
    assert "http_client: io: reading chunk body" in captured_output(proc)
    assert_basic_get_request(server.requests[0], "/slow-chunk", server.port)


def test_timeout_waiting_for_connection_close_body_is_observable(
    loader_kboot_bin, tmp_path
):
    response_prefix = (
        b"HTTP/1.1 200 OK\r\n"
        b"Connection: close\r\n"
        b"\r\n"
        b"body prefix"
    )

    with StalledHTTPServer(response_prefix) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/slow-close", timeout=20)

    assert proc.returncode != 0
    assert "http_client: io: reading body" in captured_output(proc)
    assert_basic_get_request(server.requests[0], "/slow-close", server.port)


@pytest.mark.parametrize(
    ("status", "returncode", "diagnostic"),
    [
        (401, 21, "http_client: response.unsupported: 401 Authorization Required"),
        (404, 51, "http_client: client.error"),
        (502, 50, "http_client: server.error"),
    ],
)
def test_status_code_error_is_observable(
    loader_kboot_bin, tmp_path, status, returncode, diagnostic
):
    response = (
        f"HTTP/1.1 {status} Test Status\r\n".encode("ascii")
        + b"Content-Length: 0\r\n"
        + b"\r\n"
    )
    path = f"/status-{status}"

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, path)

    # Curl seed: curl/tests/http/test_02_download.py::TestDownload::
    # test_02_14_not_found plus curl/tests/http/test_05_errors.py retry tests
    # for 401 and 502 status handling.
    assert proc.returncode == returncode
    assert diagnostic in captured_output(proc)
    assert_basic_get_request(server.requests[0], path, server.port)


def test_http_1_0_status_code_error_is_observable(loader_kboot_bin, tmp_path):
    response = (
        b"HTTP/1.0 401 BAD BOY\r\n"
        b"Server: swsclose\r\n"
        b"Content-Type: text/html\r\n"
        b"\r\n"
        b"This contains a response code >= 400."
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/http10-401")

    # Curl seed: curl/tests/data/test151 uses HTTP/1.0 with a 401 status.
    assert proc.returncode != 0
    assert (
        "http_client: response.unsupported: 401 Authorization Required"
        in captured_output(proc)
    )
    assert_basic_get_request(server.requests[0], "/http10-401", server.port)


def test_status_code_error_body_is_not_written(loader_kboot_bin, tmp_path):
    response = (
        b"HTTP/1.1 404 Nah\r\n"
        b"Connection: close\r\n"
        b"Content-Length: 13\r\n"
        b"Funny-head: yesyes\r\n"
        b"\r\n"
        b"0123456789123"
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/status-body")

    # Curl seed: curl/tests/data/test99 has a 404 response with a body; this
    # client classifies the response before copying the body.
    assert proc.returncode == 51
    assert "http_client: client.error" in captured_output(proc)
    assert_basic_get_request(server.requests[0], "/status-body", server.port)
    assert not (tmp_path / "status-body").exists()


def test_invalid_status_line_is_observable(loader_kboot_bin, tmp_path):
    response = (
        b"No headers at all, only data swsclose\r\n"
        b"\r\n"
        b"Let's get a little test data"
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/bad-status-line")

    # Curl seed: curl/tests/data/test1144 sends body-like bytes where an HTTP
    # status line should be.
    assert proc.returncode != 0
    assert "http_client: status.invalid_line" in captured_output(proc)
    assert_basic_get_request(server.requests[0], "/bad-status-line", server.port)


def test_redirect_status_without_location_is_observable(loader_kboot_bin, tmp_path):
    response = (
        b"HTTP/1.1 307 Temporary Redirect\r\n"
        b"Content-Length: 0\r\n"
        b"\r\n"
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/redirect-no-location")

    # Curl seed: curl/tests/data/test3034 uses a 307 redirect status; this
    # local variant pins the no-Location diagnostic path.
    assert proc.returncode != 0
    assert (
        "http_client: response.unsupported: redirect without Location"
        in captured_output(proc)
    )
    assert_basic_get_request(server.requests[0], "/redirect-no-location", server.port)


def test_redirect_location_is_followed(loader_kboot_bin, tmp_path):
    redirect = (
        b"HTTP/1.1 301 Moved Permanently\r\n"
        b"Location: /final\r\n"
        b"Content-Length: 0\r\n"
        b"\r\n"
    )
    final = (
        b"HTTP/1.1 200 OK\r\n"
        b"Content-Length: 10\r\n"
        b"\r\n"
        b"final body"
    )

    with HTTPResponseServer([redirect, final]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/redirect")

    # Curl seed: curl/tests/data/test1942 and curl/tests/data/test1973 follow a
    # relative Location header.
    assert proc.returncode == 0, captured_output(proc)
    assert len(server.requests) == 2
    assert_basic_get_request(server.requests[0], "/redirect", server.port)
    assert_basic_get_request(server.requests[1], "/final", server.port)
    assert (tmp_path / "redirect").read_bytes() == b"final body"


def test_redirect_location_with_extra_spaces_is_followed(loader_kboot_bin, tmp_path):
    redirect = (
        b"HTTP/1.1 301 Moved Permanently\r\n"
        b"Location:  /spaced/final?logout=TRUE\r\n"
        b"Connection: close\r\n"
        b"\r\n"
    )
    final = (
        b"HTTP/1.1 200 OK\r\n"
        b"Content-Length: 11\r\n"
        b"\r\n"
        b"space final"
    )

    with HTTPResponseServer([redirect, final]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/want/redirect")

    # Curl seed: curl/tests/data/test28 follows a Location header whose value
    # has extra leading spaces.
    assert proc.returncode == 0, captured_output(proc)
    assert len(server.requests) == 2
    assert_basic_get_request(server.requests[0], "/want/redirect", server.port)
    assert_basic_get_request(server.requests[1], "/spaced/final?logout=TRUE", server.port)
    assert (tmp_path / "redirect").read_bytes() == b"space final"


def test_query_only_redirect_location_is_followed(loader_kboot_bin, tmp_path):
    redirect = (
        b"HTTP/1.1 302 OK\r\n"
        b"Location: ?console=comconsole\r\n"
        b"Connection: close\r\n"
        b"\r\n"
    )
    final = (
        b"HTTP/1.1 200 OK\r\n"
        b"Content-Length: 10\r\n"
        b"\r\n"
        b"query body"
    )

    with HTTPResponseServer([redirect, final]) as server:
        proc = run_http_client(
            loader_kboot_bin, tmp_path, server, "/dir/base?console=efi"
        )

    assert proc.returncode == 0, captured_output(proc)
    assert len(server.requests) == 2
    assert_basic_get_request(server.requests[0], "/dir/base?console=efi", server.port)
    assert_basic_get_request(
        server.requests[1], "/dir/base?console=comconsole", server.port
    )
    assert (tmp_path / "base").read_bytes() == b"query body"


@pytest.mark.parametrize(
    "location",
    ["final", "./final", "../final", "#section", "ftp://localhost/final"],
)
def test_unsupported_redirect_locations_are_rejected(
    loader_kboot_bin, tmp_path, location
):
    redirect = (
        b"HTTP/1.1 302 OK\r\n"
        + f"Location: {location}\r\n".encode("ascii")
        + b"Connection: close\r\n"
        b"\r\n"
    )

    with HTTPResponseServer([redirect]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/dir/base")

    assert proc.returncode != 0
    assert (
        "http_client: response.unsupported: unsupported Location"
        in captured_output(proc)
    )
    assert_basic_get_request(server.requests[0], "/dir/base", server.port)


def test_1xx_response_then_final_response(loader_kboot_bin, tmp_path):
    response = (
        b"HTTP/1.1 100 Continue\r\n"
        b"\r\n"
        b"HTTP/1.1 200 OK\r\n"
        b"Content-Length: 7\r\n"
        b"\r\n"
        b"final!\n"
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/continue")

    # Curl seed: curl/tests/data/test246, test1002, and test2059 send a 100
    # Continue response before the final response.
    assert proc.returncode == 0, captured_output(proc)
    assert_basic_get_request(server.requests[0], "/continue", server.port)
    assert (tmp_path / "continue").read_bytes() == b"final!\n"


def test_1xx_response_then_status_error(loader_kboot_bin, tmp_path):
    response = (
        b"HTTP/1.1 100 Continue\r\n"
        b"Server: Microsoft-IIS/5.0\r\n"
        b"\r\n"
        b"HTTP/1.1 401 authentication please\r\n"
        b"Content-Length: 0\r\n"
        b"\r\n"
    )

    with HTTPResponseServer([response]) as server:
        proc = run_http_client(loader_kboot_bin, tmp_path, server, "/continue-then-401")

    # Curl seed: curl/tests/data/test1002 sends a 100 Continue response before
    # the final 401 response.
    assert proc.returncode != 0
    assert (
        "http_client: response.unsupported: 401 Authorization Required"
        in captured_output(proc)
    )
    assert_basic_get_request(server.requests[0], "/continue-then-401", server.port)
