# $Id$
#
# Build definitions for Darwin

# Apple ships libarchive, but for some reason does not provide the headers.
# Build against a homebrew-provided libarchive library and headers.
LDFLAGS+=	-L/usr/local/opt/libarchive/lib
CFLAGS+=	-I/usr/local/opt/libarchive/include
