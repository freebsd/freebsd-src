# kboot HTTP client

The interface is declared in [`include/http.h`](include/http.h). `http_get()`
retrieves an HTTP or HTTPS URL into a caller-provided sink:

```c
#include "http_file_sink.h"

struct http_file_sink fs;
http_req_t req;

http_file_sink_init(&fs, ".");
req = (http_req_t) {
	.url = url,
	.sink = &fs.sink,
};

return (http_get(req));
```

The URL and sink are required. HTTPS is used when the URL omits a scheme.
`http_get()` returns `HTTP_OK` on success or an `http_error_t` error code;
`http_error_name()` returns its diagnostic name. See `http_test_get()` in
[`kboot/main.c`](kboot/main.c) for a complete example.

## Sink

A sink implements three callbacks:

```c
typedef struct http_sink {
	int (*open)(struct http_sink *sink, const char *name);
	int (*write)(struct http_sink *sink, const void *data, size_t len);
	int (*close)(struct http_sink *sink, bool complete);
} sink_t;
```

Each callback returns zero on success and non-zero on failure.

- `open()` prepares the output using the suggested name derived from the URL.
- `write()` consumes all `len` bytes.
- `close()` finalizes the output. `complete` is false when partial output
  should be discarded.

`close()` is called only after `open()` succeeds.

## Limitations

- Bracketed IPv6 literals are not supported.
- Retrieval is not integrated with the loader device or `devsw` lookup.

The test build and pytest harness are described in `README` and
[`tests/README.md`](tests/README.md).
