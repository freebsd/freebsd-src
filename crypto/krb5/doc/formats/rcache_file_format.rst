Replay cache file format
========================

This section documents the second version of the replay cache file
format, used by the "file2" replay cache type (new in release 1.18).
The first version of the file replay cache format is not documented.

All accesses to the replay cache file take place under an exclusive
POSIX or Windows file lock, obtained when the file is opened and
released when it is closed.  Replay cache files are automatically
created when first accessed.

For each store operation, a tag is derived from the checksum part of
the :RFC:`3961` ciphertext of the authenticator.  The checksum is
coerced to a fixed length of 12 bytes, either through truncation or
right-padding with zero bytes.  A four-byte timestamp is appended to
the tag to produce a total record length of 16 bytes.

Bytes 0 through 15 of the file contain a hash seed for the SipHash-2-4
algorithm (siphash_); this field is populated with random bytes when
the file is first created.  All remaining bytes are divided into a
series of expanding hash tables:

* Bytes 16-16383: hash table 1 (1023 slots)
* Bytes 16384-49151: hash table 2 (2048 slots)
* Bytes 49152-114687: hash table 3 (4096 slots)
* ...

Only some hash tables will be present in the file at any specific
time, and the final table may be only partially filled.  Replay cache
files may be sparse if the filesystem supports it.

For each table present in the file, the tag is hashed with SipHash-2-4
using the seed recorded in the file.  The first byte of the seed is
incremented by one (modulo 256) for each table after the first.  The
resulting hash value is taken modulo one less than the table size
(1022 for the first hash table, 2047 for the second) to produce the
index.  The record may be found at the slot given by the index or at
the next slot.

All candidate locations for the record must be searched until a slot
is found with a timestamp of zero (indicating a slot which has never
been written to) or an offset is reached at or beyond the end of the
file.  Any candidate location with a timestamp value of zero, with a
timestamp value less than the current time minus clockskew, or at or
beyond the end of the file is available for writing.  When all
candidate locations have been searched without finding a match, the
new entry is written to the earliest candidate available for writing.

.. _siphash: https://131002.net/siphash/siphash.pdf
