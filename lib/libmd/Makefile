#	$Id: Makefile,v 1.24 1997/09/05 11:49:43 peter Exp $

LIB=	md
.if ${BINFORMAT} != elf
NOPIC=	true
.endif
SRCS=	md2c.c md4c.c md5c.c md2hl.c md4hl.c md5hl.c
MAN3+=  md2.3 md4.3 md5.3
MLINKS+=md2.3 MD2Init.3 md2.3 MD2Update.3 md2.3 MD2Final.3
MLINKS+=md2.3 MD2End.3  md2.3 MD2File.3   md2.3 MD2Data.3
MLINKS+=md4.3 MD4Init.3 md4.3 MD4Update.3 md4.3 MD4Final.3
MLINKS+=md4.3 MD4End.3  md4.3 MD4File.3   md4.3 MD4Data.3
MLINKS+=md5.3 MD5Init.3 md5.3 MD5Update.3 md5.3 MD5Final.3
MLINKS+=md5.3 MD5End.3  md5.3 MD5File.3   md5.3 MD5Data.3
CLEANFILES+=	md[245]hl.c md[245].ref md[245].3 mddriver
CFLAGS+= -I${.CURDIR}

all: md2.3 md4.3 md5.3

md2hl.c: mdXhl.c
	sed -e 's/mdX/md2/g' -e 's/MDX/MD2/g' $> > $@

md4hl.c: mdXhl.c
	sed -e 's/mdX/md4/g' -e 's/MDX/MD4/g' $> > $@

md5hl.c: mdXhl.c
	sed -e 's/mdX/md5/g' -e 's/MDX/MD5/g' $> > $@

md2.3: ${.CURDIR}/mdX.3
	sed -e 's/mdX/md2/g' -e 's/MDX/MD2/g' $> > $@
	cat ${.CURDIR}/md2.copyright >> $@

md4.3: ${.CURDIR}/mdX.3
	sed -e 's/mdX/md4/g' -e 's/MDX/MD4/g' $> > $@
	cat ${.CURDIR}/md4.copyright >> $@

md5.3: ${.CURDIR}/mdX.3
	sed -e 's/mdX/md5/g' -e 's/MDX/MD5/g' $> > $@
	cat ${.CURDIR}/md5.copyright >> $@

md2.ref:
	echo 'MD2 test suite:' > $@
	@echo 'MD2 ("") = 8350e5a3e24c153df2275c9f80692773' >> $@
	@echo 'MD2 ("a") = 32ec01ec4a6dac72c0ab96fb34c0b5d1' >> $@
	@echo 'MD2 ("abc") = da853b0d3f88d99b30283a69e6ded6bb' >> $@
	@echo 'MD2 ("message digest") = ab4f496bfb2a530b219ff33031fe06b0' >> $@
	@echo 'MD2 ("abcdefghijklmnopqrstuvwxyz") = 4e8ddff3650292ab5a4108c3aa47940b' >> $@
	@echo 'MD2 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") = da33def2a42df13975352846c30338cd' >> $@
	@echo 'MD2 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") = d5976f79d83d3a0dc9806c3c66f3efd8' >> $@

md4.ref:
	echo 'MD4 test suite:' > $@
	@echo 'MD4 ("") = 31d6cfe0d16ae931b73c59d7e0c089c0' >> $@
	@echo 'MD4 ("a") = bde52cb31de33e46245e05fbdbd6fb24' >> $@
	@echo 'MD4 ("abc") = a448017aaf21d8525fc10ae87aa6729d' >> $@
	@echo 'MD4 ("message digest") = d9130a8164549fe818874806e1c7014b' >> $@
	@echo 'MD4 ("abcdefghijklmnopqrstuvwxyz") = d79e1c308aa5bbcdeea8ed63df412da9' >> $@
	@echo 'MD4 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") = 043f8582f241db351ce627e153e7f0e4' >> $@
	@echo 'MD4 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") = e33b4ddc9c38f2199c3e7b164fcc0536' >> $@

md5.ref:
	echo 'MD5 test suite:' > $@
	@echo 'MD5 ("") = d41d8cd98f00b204e9800998ecf8427e' >> $@
	@echo 'MD5 ("a") = 0cc175b9c0f1b6a831c399e269772661' >> $@
	@echo 'MD5 ("abc") = 900150983cd24fb0d6963f7d28e17f72' >> $@
	@echo 'MD5 ("message digest") = f96b697d7cb7938d525a2f31aaf161d0' >> $@
	@echo 'MD5 ("abcdefghijklmnopqrstuvwxyz") = c3fcd3d76192e4007dfb496cca67e13b' >> $@
	@echo 'MD5 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") = d174ab98d277d9f5a5611c2c9f419d9f' >> $@
	@echo 'MD5 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") = 57edf4a22be3c955ac49da2e2107b67a' >> $@

test:	md2.ref md4.ref md5.ref
	@${ECHO} if any of these test fail, the code produces wrong results
	@${ECHO} and should NOT be used.
	${CC} -static ${CFLAGS} ${LDFLAGS} -DMD=2 -o mddriver ${.CURDIR}/mddriver.c -L. -lmd
	./mddriver | cmp md2.ref -
	@${ECHO} MD2 passed test
	${CC} -static ${CFLAGS} ${LDFLAGS} -DMD=4 -o mddriver ${.CURDIR}/mddriver.c -L. -lmd
	./mddriver | cmp md4.ref -
	@${ECHO} MD4 passed test
	${CC} -static ${CFLAGS} ${LDFLAGS} -DMD=5 -o mddriver ${.CURDIR}/mddriver.c -L. -lmd
	./mddriver | cmp md5.ref -
	@${ECHO} MD5 passed test
	-rm -f mddriver

beforeinstall:
.for i in md2.h md4.h md5.h
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/$i \
		${DESTDIR}/usr/include
.endfor

.include <bsd.lib.mk>
