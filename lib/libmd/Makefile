# $FreeBSD$

LIB=	md
SRCS=	md2c.c md4c.c md5c.c md2hl.c md4hl.c md5hl.c \
	rmd160c.c rmd160hl.c \
	sha0c.c sha0hl.c sha1c.c sha1hl.c
INCS=	md2.h md4.h md5.h ripemd.h sha.h

MAN+=	md2.3 md4.3 md5.3 ripemd.3 sha.3
MLINKS+=md2.3 MD2Init.3 md2.3 MD2Update.3 md2.3 MD2Final.3
MLINKS+=md2.3 MD2End.3  md2.3 MD2File.3   md2.3 MD2Data.3
MLINKS+=md4.3 MD4Init.3 md4.3 MD4Update.3 md4.3 MD4Final.3
MLINKS+=md4.3 MD4End.3  md4.3 MD4File.3   md4.3 MD4Data.3
MLINKS+=md5.3 MD5Init.3 md5.3 MD5Update.3 md5.3 MD5Final.3
MLINKS+=md5.3 MD5End.3  md5.3 MD5File.3   md5.3 MD5Data.3
MLINKS+=ripemd.3 RIPEMD160_Init.3  ripemd.3 RIPEMD160_Update.3
MLINKS+=ripemd.3 RIPEMD160_Final.3 ripemd.3 RIPEMD160_Data.3
MLINKS+=ripemd.3 RIPEMD160_End.3   ripemd.3 RIPEMD160_File.3
MLINKS+=sha.3 SHA_Init.3  sha.3 SHA_Update.3  sha.3 SHA_Final.3
MLINKS+=sha.3 SHA_End.3   sha.3 SHA_File.3    sha.3 SHA_Data.3
MLINKS+=sha.3 SHA1_Init.3 sha.3 SHA1_Update.3 sha.3 SHA1_Final.3
MLINKS+=sha.3 SHA1_End.3  sha.3 SHA1_File.3   sha.3 SHA1_Data.3
CLEANFILES+=	md[245]hl.c md[245].ref md[245].3 mddriver \
		rmd160.ref rmd160hl.c rmddriver \
		sha0.ref sha0hl.c sha1.ref sha1hl.c shadriver
CFLAGS+= -I${.CURDIR}
.PATH: ${.CURDIR}/${MACHINE_ARCH}

.if ${OBJFORMAT} == "elf"
.if exists(${MACHINE_ARCH}/sha.S)
SRCS+=	sha.S
CFLAGS+= -DSHA1_ASM -DELF
.endif
.if exists(${MACHINE_ARCH}/rmd160.S)
SRCS+=	rmd160.S
CFLAGS+= -DRMD160_ASM -DELF
.endif
.endif

md2hl.c: mdXhl.c
	(echo '#define LENGTH 16'; \
		sed -e 's/mdX/md2/g' -e 's/MDX/MD2/g' ${.ALLSRC}) > ${.TARGET}

md4hl.c: mdXhl.c
	(echo '#define LENGTH 16'; \
		sed -e 's/mdX/md4/g' -e 's/MDX/MD4/g' ${.ALLSRC}) > ${.TARGET}

md5hl.c: mdXhl.c
	(echo '#define LENGTH 16'; \
		sed -e 's/mdX/md5/g' -e 's/MDX/MD5/g' ${.ALLSRC}) > ${.TARGET}

sha0hl.c: mdXhl.c
	(echo '#define LENGTH 20'; \
		sed -e 's/mdX/sha/g' -e 's/MDX/SHA_/g' -e 's/SHA__/SHA_/g' \
		${.ALLSRC}) > ${.TARGET}

sha1hl.c: mdXhl.c
	(echo '#define LENGTH 20'; \
		sed -e 's/mdX/sha/g' -e 's/MDX/SHA1_/g' -e 's/SHA1__/SHA1_/g' \
		${.ALLSRC}) > ${.TARGET}

rmd160hl.c: mdXhl.c
	(echo '#define LENGTH 20'; \
		sed -e 's/mdX/ripemd/g' -e 's/MDX/RIPEMD160_/g' \
			-e 's/RIPEMD160__/RIPEMD160_/g' \
			${.ALLSRC}) > ${.TARGET}

.for i in 2 4 5
md${i}.3: ${.CURDIR}/mdX.3
	sed -e "s/mdX/md${i}/g" -e "s/MDX/MD${i}/g" ${.ALLSRC} > ${.TARGET}
	cat ${.CURDIR}/md${i}.copyright >> ${.TARGET}
.endfor

md2.ref:
	echo 'MD2 test suite:' > ${.TARGET}
	@echo 'MD2 ("") = 8350e5a3e24c153df2275c9f80692773' >> ${.TARGET}
	@echo 'MD2 ("a") = 32ec01ec4a6dac72c0ab96fb34c0b5d1' >> ${.TARGET}
	@echo 'MD2 ("abc") = da853b0d3f88d99b30283a69e6ded6bb' >> ${.TARGET}
	@echo 'MD2 ("message digest") = ab4f496bfb2a530b219ff33031fe06b0' >> ${.TARGET}
	@echo 'MD2 ("abcdefghijklmnopqrstuvwxyz") = 4e8ddff3650292ab5a4108c3aa47940b' >> ${.TARGET}
	@echo 'MD2 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") = da33def2a42df13975352846c30338cd' >> ${.TARGET}
	@echo 'MD2 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") = d5976f79d83d3a0dc9806c3c66f3efd8' >> ${.TARGET}

md4.ref:
	echo 'MD4 test suite:' > ${.TARGET}
	@echo 'MD4 ("") = 31d6cfe0d16ae931b73c59d7e0c089c0' >> ${.TARGET}
	@echo 'MD4 ("a") = bde52cb31de33e46245e05fbdbd6fb24' >> ${.TARGET}
	@echo 'MD4 ("abc") = a448017aaf21d8525fc10ae87aa6729d' >> ${.TARGET}
	@echo 'MD4 ("message digest") = d9130a8164549fe818874806e1c7014b' >> ${.TARGET}
	@echo 'MD4 ("abcdefghijklmnopqrstuvwxyz") = d79e1c308aa5bbcdeea8ed63df412da9' >> ${.TARGET}
	@echo 'MD4 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") = 043f8582f241db351ce627e153e7f0e4' >> ${.TARGET}
	@echo 'MD4 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") = e33b4ddc9c38f2199c3e7b164fcc0536' >> ${.TARGET}

md5.ref:
	echo 'MD5 test suite:' > ${.TARGET}
	@echo 'MD5 ("") = d41d8cd98f00b204e9800998ecf8427e' >> ${.TARGET}
	@echo 'MD5 ("a") = 0cc175b9c0f1b6a831c399e269772661' >> ${.TARGET}
	@echo 'MD5 ("abc") = 900150983cd24fb0d6963f7d28e17f72' >> ${.TARGET}
	@echo 'MD5 ("message digest") = f96b697d7cb7938d525a2f31aaf161d0' >> ${.TARGET}
	@echo 'MD5 ("abcdefghijklmnopqrstuvwxyz") = c3fcd3d76192e4007dfb496cca67e13b' >> ${.TARGET}
	@echo 'MD5 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") = d174ab98d277d9f5a5611c2c9f419d9f' >> ${.TARGET}
	@echo 'MD5 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") = 57edf4a22be3c955ac49da2e2107b67a' >> ${.TARGET}

sha0.ref:
	(echo 'SHA-0 test suite:'; \
	echo 'SHA-0 ("") = f96cea198ad1dd5617ac084a3d92c6107708c0ef'; \
	echo 'SHA-0 ("abc") = 0164b8a914cd2a5e74c4f7ff082c4d97f1edf880'; \
	echo 'SHA-0 ("message digest") =' \
		'c1b0f222d150ebb9aa36a40cafdc8bcbed830b14'; \
	echo 'SHA-0 ("abcdefghijklmnopqrstuvwxyz") =' \
		'b40ce07a430cfd3c033039b9fe9afec95dc1bdcd'; \
	echo 'SHA-0 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") =' \
		'79e966f7a3a990df33e40e3d7f8f18d2caebadfa'; \
	echo 'SHA-0 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") =' \
		'4aa29d14d171522ece47bee8957e35a41f3e9cff' ) > ${.TARGET}

sha1.ref:
	(echo 'SHA-1 test suite:'; \
	echo 'SHA-1 ("") = da39a3ee5e6b4b0d3255bfef95601890afd80709'; \
	echo 'SHA-1 ("abc") = a9993e364706816aba3e25717850c26c9cd0d89d'; \
	echo 'SHA-1 ("message digest") =' \
		'c12252ceda8be8994d5fa0290a47231c1d16aae3'; \
	echo 'SHA-1 ("abcdefghijklmnopqrstuvwxyz") =' \
		'32d10c7b8cf96570ca04ce37f2a19d84240d3a89'; \
	echo 'SHA-1 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") =' \
		'761c457bf73b14d27e9e9265c46f4b4dda11f940'; \
	echo 'SHA-1 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") =' \
		'50abf5706a150990a08b2c5ea40fa0e585554732' ) > ${.TARGET}

rmd160.ref:
	(echo 'RIPEMD160 test suite:'; \
	echo 'RIPEMD160 ("") = 9c1185a5c5e9fc54612808977ee8f548b2258d31'; \
	echo 'RIPEMD160 ("abc") = 8eb208f7e05d987a9b044a8e98c6b087f15a0bfc'; \
	echo 'RIPEMD160 ("message digest") =' \
		'5d0689ef49d2fae572b881b123a85ffa21595f36'; \
	echo 'RIPEMD160 ("abcdefghijklmnopqrstuvwxyz") =' \
		'f71c27109c692c1b56bbdceb5b9d2865b3708dbc'; \
	echo 'RIPEMD160 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") =' \
		'b0e20b6e3116640286ed3a87a5713079b21f5189'; \
	echo 'RIPEMD160 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") =' \
		'9b752e45573d4b39f4dbd3323cab82bf63326bfb' ) > ${.TARGET}

test:	md2.ref md4.ref md5.ref sha0.ref rmd160.ref sha1.ref
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
	${CC} -static ${CFLAGS} ${LDFLAGS} -o rmddriver ${.CURDIR}/rmddriver.c -L. -lmd
	./rmddriver | cmp rmd160.ref -
	@${ECHO} RIPEMD160 passed test
	-rm -f rmddriver
	${CC} -static ${CFLAGS} ${LDFLAGS} -DSHA=0 -o shadriver ${.CURDIR}/shadriver.c -L. -lmd
	./shadriver | cmp sha0.ref -
	@${ECHO} SHA-0 passed test
	${CC} -static ${CFLAGS} ${LDFLAGS} -DSHA=1 -o shadriver ${.CURDIR}/shadriver.c -L. -lmd
	./shadriver | cmp sha1.ref -
	@${ECHO} SHA-1 passed test
	-rm -f shadriver

.include <bsd.lib.mk>
