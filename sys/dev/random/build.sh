# $FreeBSD$
#
# Basic script to build crude unit tests.
#
cc -g -O0 -pthread -DRANDOM_DEBUG -DRANDOM_YARROW \
	-I../.. -lstdthreads -Wall \
	unit_test.c \
	yarrow.c \
	hash.c \
	../../crypto/rijndael/rijndael-api-fst.c \
	../../crypto/rijndael/rijndael-alg-fst.c \
	../../crypto/sha2/sha2.c \
	../../crypto/sha2/sha256c.c \
	-o yunit_test
cc -g -O0 -pthread -DRANDOM_DEBUG -DRANDOM_FORTUNA \
	-I../.. -lstdthreads -Wall \
	unit_test.c \
	fortuna.c \
	hash.c \
	../../crypto/rijndael/rijndael-api-fst.c \
	../../crypto/rijndael/rijndael-alg-fst.c \
	../../crypto/sha2/sha2.c \
	../../crypto/sha2/sha256c.c \
	-o funit_test
