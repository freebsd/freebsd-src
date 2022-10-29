
PROG=	wg
MAN=	wg.8

LIBADD=	nv

.PATH: ${SRCTOP}/contrib/wireguard-tools
.PATH: ${SRCTOP}/contrib/wireguard-tools/man

SRCS+=	wg.c

SRCS+=	config.c
SRCS+=	encoding.c
SRCS+=	ipc.c
SRCS+=	terminal.c

SRCS+=	curve25519.c

# Subcommands
SRCS+=	show.c
SRCS+=	showconf.c
SRCS+=	set.c
SRCS+=	setconf.c
SRCS+=	genkey.c
SRCS+=	pubkey.c

# Base should pull RUNSTATEDIR from paths.h, _PATH_VARRUN
CFLAGS+=	-include paths.h
CFLAGS+=	-DRUNSTATEDIR="_PATH_VARRUN"

# Not going to change this one.
CWARNFLAGS.curve25519.c+=	-Wno-shadow

# sockaddr{,_in} alignment
CWARNFLAGS.config.c+=	-Wno-cast-align
CWARNFLAGS.ipc.c+=	-Wno-cast-align

# qsort comparator
CWARNFLAGS.show.c+=	-Wno-cast-qual

.include <bsd.prog.mk>
