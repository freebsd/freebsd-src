# $FreeBSD$

# DCE 1.1 UUID implementation sources

.PATH: ${.CURDIR}/uuid

SRCS+= \
	uuid_compare.c \
	uuid_create.c \
	uuid_create_nil.c \
	uuid_equal.c \
	uuid_from_string.c \
	uuid_hash.c \
	uuid_is_nil.c \
	uuid_stream.c \
	uuid_to_string.c

SYM_MAPS += ${.CURDIR}/uuid/Symbol.map
