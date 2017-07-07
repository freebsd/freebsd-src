.. _keytab_file_format:

Keytab file format
==================

There are two versions of the file format used by the FILE keytab
type.  The first byte of the file always has the value 5, and the
value of the second byte contains the version number (1 or 2).
Version 1 of the file format uses native byte order for integer
representations.  Version 2 always uses big-endian byte order.

After the two-byte version indicator, the file contains a sequence of
signed 32-bit record lengths followed by key records or holes.  A
positive record length indicates a valid key entry whose size is equal
to or less than the record length.  A negative length indicates a
zero-filled hole whose size is the inverse of the length.  A length of
0 indicates the end of the file.


Key entry format
----------------

A key entry may be smaller in size than the record length which
precedes it, because it may have replaced a hole which is larger than
the key entry.  Key entries use the following informal grammar::

    entry ::=
        principal
        timestamp (32 bits)
        key version (8 bits)
        enctype (16 bits)
        key length (16 bits)
        key contents
        key version (32 bits) [in release 1.14 and later]

    principal ::=
        count of components (16 bits) [includes realm in version 1]
        realm (data)
        component1 (data)
        component2 (data)
        ...
        name type (32 bits) [omitted in version 1]

    data ::=
        length (16 bits)
        value (length bytes)

The 32-bit key version overrides the 8-bit key version.  To determine
if it is present, the implementation must check that at least 4 bytes
remain in the record after the other fields are read, and that the
value of the 32-bit integer contained in those bytes is non-zero.
