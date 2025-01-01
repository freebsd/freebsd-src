# libder

## What is libder?

libder is a small library for encoding/decoding DER-encoded objects.  It is
expected to be able to decode any BER-encoded buffer, and an attempt to
re-encode the resulting tree would apply any normalization expected by a DER
decoder.  The author's use is primarily to decode/encode ECC keys for
interoperability with OpenSSL.

The authoritative source for this software is located at
https://git.kevans.dev/kevans/libder, but it's additionally mirrored to
[GitHub](https://github.com/kevans91/libder) for user-facing interactions.
Pull requests and issues are open on GitHub.

## What is libder not?

libder is not intended to be a general-purpose library for working with DER/BER
specified objects.  It may provide some helpers for building more primitive
data types, but libder will quickly punt on anything even remotely complex and
require the library consumer to supply it as a type/payload/size triple that it
will treat as relatively opaque (modulo some encoding normalization rules that
can be applied without deeply understanding the data contained within).

libder also doesn't do strict validation of what it reads in today, for better
or worse.  e.g., a boolean may occupy more than one byte and libder will happily
present it to the application in that way.  It would be normalized on
re-encoding to 0xff or 0x00 depending on whether any bits are set or not.
