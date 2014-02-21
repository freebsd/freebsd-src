# $FreeBSD$

LIB=	md
SHLIB_MAJOR= 6
SHLIBDIR?= /lib
SRCS=	md4c.c md5c.c md4hl.c md5hl.c \
	rmd160c.c rmd160hl.c \
	sha0c.c sha0hl.c sha1c.c sha1hl.c \
	sha256c.c sha256hl.c \
	sha512c.c sha512hl.c
INCS=	md4.h md5.h ripemd.h sha.h sha256.h sha512.h

WARNS?=	0

MAN+=	md4.3 md5.3 ripemd.3 sha.3 sha256.3 sha512.3
MLINKS+=md4.3 MD4Init.3 md4.3 MD4Update.3 md4.3 MD4Final.3
MLINKS+=md4.3 MD4End.3  md4.3 MD4File.3   md4.3 MD4FileChunk.3
MLINKS+=md4.3 MD4Data.3
MLINKS+=md5.3 MD5Init.3 md5.3 MD5Update.3 md5.3 MD5Final.3
MLINKS+=md5.3 MD5End.3  md5.3 MD5File.3   md5.3 MD5FileChunk.3
MLINKS+=md5.3 MD5Data.3
MLINKS+=ripemd.3 RIPEMD160_Init.3  ripemd.3 RIPEMD160_Update.3
MLINKS+=ripemd.3 RIPEMD160_Final.3 ripemd.3 RIPEMD160_Data.3
MLINKS+=ripemd.3 RIPEMD160_End.3   ripemd.3 RIPEMD160_File.3
MLINKS+=ripemd.3 RIPEMD160_FileChunk.3
MLINKS+=sha.3 SHA_Init.3  sha.3 SHA_Update.3  sha.3 SHA_Final.3
MLINKS+=sha.3 SHA_End.3   sha.3 SHA_File.3    sha.3 SHA_FileChunk.3
MLINKS+=sha.3 SHA_Data.3
MLINKS+=sha.3 SHA1_Init.3 sha.3 SHA1_Update.3 sha.3 SHA1_Final.3
MLINKS+=sha.3 SHA1_End.3  sha.3 SHA1_File.3   sha.3 SHA1_FileChunk.3
MLINKS+=sha.3 SHA1_Data.3
MLINKS+=sha256.3 SHA256_Init.3  sha256.3 SHA256_Update.3
MLINKS+=sha256.3 SHA256_Final.3 sha256.3 SHA256_End.3
MLINKS+=sha256.3 SHA256_File.3  sha256.3 SHA256_FileChunk.3
MLINKS+=sha256.3 SHA256_Data.3
MLINKS+=sha512.3 SHA512_Init.3  sha512.3 SHA512_Update.3
MLINKS+=sha512.3 SHA512_Final.3 sha512.3 SHA512_End.3
MLINKS+=sha512.3 SHA512_File.3  sha512.3 SHA512_FileChunk.3
MLINKS+=sha512.3 SHA512_Data.3
CLEANFILES+=	md[245]hl.c md[245].ref md[245].3 mddriver \
		rmd160.ref rmd160hl.c rmddriver \
		sha0.ref sha0hl.c sha1.ref sha1hl.c shadriver \
		sha256.ref sha256hl.c sha512.ref sha512hl.c

CFLAGS+= -I${.CURDIR}
.PATH: ${.CURDIR}/${MACHINE_ARCH}

.if exists(${MACHINE_ARCH}/sha.S)
SRCS+=	sha.S
CFLAGS+= -DSHA1_ASM
.endif
.if exists(${MACHINE_ARCH}/rmd160.S)
SRCS+=	rmd160.S
CFLAGS+= -DRMD160_ASM
.endif
.if exists(${MACHINE_ARCH}/sha.S) || exists(${MACHINE_ARCH}/rmd160.S)
ACFLAGS+= -DELF -Wa,--noexecstack
.endif

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

sha256hl.c: mdXhl.c
	(echo '#define LENGTH 32'; \
		sed -e 's/mdX/sha256/g' -e 's/MDX/SHA256_/g'	\
			-e  's/SHA256__/SHA256_/g' \
		${.ALLSRC}) > ${.TARGET}

sha512hl.c: mdXhl.c
	(echo '#define LENGTH 64'; \
		sed -e 's/mdX/sha512/g' -e 's/MDX/SHA512_/g'	\
			-e  's/SHA512__/SHA512_/g' \
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

md4.ref:
	echo 'MD4 test suite:' > ${.TARGET}
	@echo 'MD4 ("") = 31d6cfe0d16ae931b73c59d7e0c089c0' >> ${.TARGET}
	@echo 'MD4 ("a") = bde52cb31de33e46245e05fbdbd6fb24' >> ${.TARGET}
	@echo 'MD4 ("abc") = a448017aaf21d8525fc10ae87aa6729d' >> ${.TARGET}
	@echo 'MD4 ("message digest") = d9130a8164549fe818874806e1c7014b' >> ${.TARGET}
	@echo 'MD4 ("abcdefghijklmnopqrstuvwxyz") = d79e1c308aa5bbcdeea8ed63df412da9' >> ${.TARGET}
	@echo 'MD4 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") =' \
		'043f8582f241db351ce627e153e7f0e4' >> ${.TARGET}
	@echo 'MD4 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") =' \
		'e33b4ddc9c38f2199c3e7b164fcc0536' >> ${.TARGET}

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
	echo 'SHA-0 test suite:' > ${.TARGET}
	@echo 'SHA-0 ("") = f96cea198ad1dd5617ac084a3d92c6107708c0ef' >> ${.TARGET}
	@echo 'SHA-0 ("abc") = 0164b8a914cd2a5e74c4f7ff082c4d97f1edf880' >> ${.TARGET}
	@echo 'SHA-0 ("message digest") =' \
		'c1b0f222d150ebb9aa36a40cafdc8bcbed830b14' >> ${.TARGET}
	@echo 'SHA-0 ("abcdefghijklmnopqrstuvwxyz") =' \
		'b40ce07a430cfd3c033039b9fe9afec95dc1bdcd' >> ${.TARGET}
	@echo 'SHA-0 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") =' \
		'79e966f7a3a990df33e40e3d7f8f18d2caebadfa' >> ${.TARGET}
	@echo 'SHA-0 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") =' \
		'4aa29d14d171522ece47bee8957e35a41f3e9cff' >> ${.TARGET}

sha1.ref:
	echo 'SHA-1 test suite:' > ${.TARGET}
	@echo 'SHA-1 ("") = da39a3ee5e6b4b0d3255bfef95601890afd80709' >> ${.TARGET}
	@echo 'SHA-1 ("abc") = a9993e364706816aba3e25717850c26c9cd0d89d' >> ${.TARGET}
	@echo 'SHA-1 ("message digest") =' \
		'c12252ceda8be8994d5fa0290a47231c1d16aae3' >> ${.TARGET}
	@echo 'SHA-1 ("abcdefghijklmnopqrstuvwxyz") =' \
		'32d10c7b8cf96570ca04ce37f2a19d84240d3a89' >> ${.TARGET}
	@echo 'SHA-1 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") =' \
		'761c457bf73b14d27e9e9265c46f4b4dda11f940' >> ${.TARGET}
	@echo 'SHA-1 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") =' \
		'50abf5706a150990a08b2c5ea40fa0e585554732' >> ${.TARGET}

sha256.ref:
	echo 'SHA-256 test suite:' > ${.TARGET}
	@echo 'SHA-256 ("") = e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855' >> ${.TARGET}
	@echo 'SHA-256 ("abc") =' \
		'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad' >> ${.TARGET}
	@echo 'SHA-256 ("message digest") =' \
		'f7846f55cf23e14eebeab5b4e1550cad5b509e3348fbc4efa3a1413d393cb650' >> ${.TARGET}
	@echo 'SHA-256 ("abcdefghijklmnopqrstuvwxyz") =' \
		'71c480df93d6ae2f1efad1447c66c9525e316218cf51fc8d9ed832f2daf18b73' >> ${.TARGET}
	@echo 'SHA-256 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") =' \
		'db4bfcbd4da0cd85a60c3c37d3fbd8805c77f15fc6b1fdfe614ee0a7c8fdb4c0' >> ${.TARGET}
	@echo 'SHA-256 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") =' \
		'f371bc4a311f2b009eef952dd83ca80e2b60026c8e935592d0f9c308453c813e' >> ${.TARGET}

sha512.ref:
	echo 'SHA-512 test suite:' > ${.TARGET}
	@echo 'SHA-512 ("") =' \
		'cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e' >> ${.TARGET}
	@echo 'SHA-512 ("abc") =' \
		'ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f' >> ${.TARGET}
	@echo 'SHA-512 ("message digest") =' \
		'107dbf389d9e9f71a3a95f6c055b9251bc5268c2be16d6c13492ea45b0199f3309e16455ab1e96118e8a905d5597b72038ddb372a89826046de66687bb420e7c' >> ${.TARGET}
	@echo 'SHA-512 ("abcdefghijklmnopqrstuvwxyz") =' \
		'4dbff86cc2ca1bae1e16468a05cb9881c97f1753bce3619034898faa1aabe429955a1bf8ec483d7421fe3c1646613a59ed5441fb0f321389f77f48a879c7b1f1' >> ${.TARGET}
	@echo 'SHA-512 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") =' \
		'1e07be23c26a86ea37ea810c8ec7809352515a970e9253c26f536cfc7a9996c45c8370583e0a78fa4a90041d71a4ceab7423f19c71b9d5a3e01249f0bebd5894' >> ${.TARGET}
	@echo 'SHA-512 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") =' \
		'72ec1ef1124a45b047e8b7c75a932195135bb61de24ec0d1914042246e0aec3a2354e093d76f3048b456764346900cb130d2a4fd5dd16abb5e30bcb850dee843' >> ${.TARGET}

rmd160.ref:
	echo 'RIPEMD160 test suite:' > ${.TARGET}
	@echo 'RIPEMD160 ("") = 9c1185a5c5e9fc54612808977ee8f548b2258d31' >> ${.TARGET}
	@echo 'RIPEMD160 ("abc") = 8eb208f7e05d987a9b044a8e98c6b087f15a0bfc' >> ${.TARGET}
	@echo 'RIPEMD160 ("message digest") =' \
		'5d0689ef49d2fae572b881b123a85ffa21595f36' >> ${.TARGET}
	@echo 'RIPEMD160 ("abcdefghijklmnopqrstuvwxyz") =' \
		'f71c27109c692c1b56bbdceb5b9d2865b3708dbc' >> ${.TARGET}
	@echo 'RIPEMD160 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") =' \
		'b0e20b6e3116640286ed3a87a5713079b21f5189' >> ${.TARGET}
	@echo 'RIPEMD160 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") =' \
		'9b752e45573d4b39f4dbd3323cab82bf63326bfb' >> ${.TARGET}

test:	md4.ref md5.ref sha0.ref rmd160.ref sha1.ref sha256.ref sha512.ref
	@${ECHO} if any of these test fail, the code produces wrong results
	@${ECHO} and should NOT be used.
	${CC} ${CFLAGS} ${LDFLAGS} -DMD=4 -o mddriver ${.CURDIR}/mddriver.c libmd.a
	./mddriver | cmp md4.ref -
	@${ECHO} MD4 passed test
	${CC} ${CFLAGS} ${LDFLAGS} -DMD=5 -o mddriver ${.CURDIR}/mddriver.c libmd.a
	./mddriver | cmp md5.ref -
	@${ECHO} MD5 passed test
	-rm -f mddriver
	${CC} ${CFLAGS} ${LDFLAGS} -o rmddriver ${.CURDIR}/rmddriver.c libmd.a
	./rmddriver | cmp rmd160.ref -
	@${ECHO} RIPEMD160 passed test
	-rm -f rmddriver
	${CC} ${CFLAGS} ${LDFLAGS} -DSHA=0 -o shadriver ${.CURDIR}/shadriver.c libmd.a
	./shadriver | cmp sha0.ref -
	@${ECHO} SHA-0 passed test
	${CC} ${CFLAGS} ${LDFLAGS} -DSHA=1 -o shadriver ${.CURDIR}/shadriver.c libmd.a
	./shadriver | cmp sha1.ref -
	@${ECHO} SHA-1 passed test
	${CC} ${CFLAGS} ${LDFLAGS} -DSHA=256 -o shadriver ${.CURDIR}/shadriver.c libmd.a
	./shadriver | cmp sha256.ref -
	@${ECHO} SHA-256 passed test
	${CC} ${CFLAGS} ${LDFLAGS} -DSHA=512 -o shadriver ${.CURDIR}/shadriver.c libmd.a
	./shadriver | cmp sha512.ref -
	@${ECHO} SHA-512 passed test
	-rm -f shadriver

.include <bsd.lib.mk>
