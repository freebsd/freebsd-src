#source: start.s
#source: sec-8a.s
#source: sec-8b.s
#source: sec-8m.s
#source: sec-8d.s
#ld: -m mmo
#objdump: -s

# Distantly related to sec-7m.s in that section lengths mattered for the
# bug.  When one input-section (seen in mmo.c as a chunk of data to
# output) had a length not a multiple of four, the last bytes were saved
# to be concatenated with the next chunk.  If it was followed by a chunk
# with a leading multiple-of-four number of zero bytes, those zero bytes
# would be omitted, and the "saved" bytes would be concatenated with the
# following (not-all-zeros) bytes.  Hence a shift of the last bytes of the
# first chunk.

.*:     file format mmo

Contents of section \.text:
 00000 e3fd0001 2a000000 00000000 00000000  .*
#...
 07ff0 00000000 00000000 00000000 2b2c0000  .*
#...
 0fff0 00000000 00000000 00002d00 00000000  .*
 10000 00000000 00000000 0000002e 2f303132  .*
 10010 33000000 00000000 00000000 00000000  .*
 10020 00300000 00000000 00000000 00000000  .*
#...
 18020 31        .*
