#
# $Id: Makefile,v 1.1.1.1 1994/04/04 14:57:18 g89r4222 Exp $
#


LIB=	crypt
SRCS=	crypt.c

#MAN3=   crypt.3
#MLINKS= crypt.3 encrypt.3 crypt.3 setkey.3
#MLINKS+=crypt.3 des_cipher.3 crypt.3 des_setkey.3

.include <bsd.lib.mk>
.include <bsd.subdir.mk>

test:
	cd test ; make test ; make clean
