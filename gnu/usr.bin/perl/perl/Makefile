#
# 

PROG= perl

SRCS+= array.c cmd.c cons.c consarg.c doarg.c doio.c dolist.c dump.c
SRCS+= eval.c form.c hash.c malloc.c perl.c perly.c regcomp.c regexec.c
SRCS+= stab.c str.c toke.c util.c usersub.c
CFLAGS+= -I${.CURDIR} 
LDADD= -lm 
DPADD= ${LIBM} 

.if exists (/usr/lib/libcrypt.a) 
DPADD+= ${LIBCRYPT}
LDADD+= -lcrypt
.else
SRCS+= crypt.c
.endif

.include <bsd.prog.mk>
