# $FreeBSD$

PACKAGE=	tests

ATF_TESTS_SH=	libmd_test

MDTESTS=	md rmd sha skein

MDTESTS_md=	md4 md5
MDTESTS_rmd=	rmd160
MDTESTS_sha=	sha0 sha1 sha224 sha256 sha384 sha512 sha512t224 sha512t256
MDTESTS_skein=	skein256 skein512 skein1024

LIBADD=	md

.for mdt in ${MDTESTS}
REFS_${mdt}=	${MDTESTS_${mdt}:C/$/.ref/}
${PACKAGE}FILES+=	${REFS_${mdt}}
CLEANFILES+=	${REFS_${mdt}}

# This target is simply convenient for auditing, letting you build all of one
# digest suite with `make <family>driver`.
.PHONY: ${mdt}driver

. for mdtc in ${MDTESTS_${mdt}}
PROGS+=	${mdtc}driver
DIGESTS+=	${mdtc}

# Avoid obj collision, we'll build the same driver.c multiple times.
${mdtc}driver.c: ${mdt}driver.c
	${CP} ${.CURDIR}/${mdt}driver.c ${.TARGET}

SRCS_${mdtc}driver=	${mdtc}driver.c
BINDIR_${mdtc}driver=	${TESTSDIR}
.if ${mdt} == "sha"
CFLAGS_${mdtc}driver=	-DSHA=${mdtc:C/sha//:C/t//}
.else
CFLAGS_${mdtc}driver=	-D${mdt:tu}=${mdtc:C/${mdt}//}
.endif

${mdt}driver: ${mdtc}driver

. endfor
.endfor

CLEANFILES+=	libmd_test.sh
libmd_test.sh:	Makefile
	:> ${.TARGET}
.for _digest in ${DIGESTS}
	@echo "atf_test_case ${_digest}" >> ${.TARGET}
	@echo "${_digest}_body() {" >> ${.TARGET}
	@echo "	atf_check -o file:\$$(atf_get_srcdir)/${_digest}.ref \\" >> \
	    ${.TARGET}
	@echo "	\$$(atf_get_srcdir)/${_digest}driver" >> ${.TARGET}
	@echo "}" >> ${.TARGET}
.endfor
	@echo "atf_init_test_cases() {" >> ${.TARGET}
.for _digest in ${DIGESTS}
	@echo "	atf_add_test_case ${_digest}" >> ${.TARGET}
.endfor
	@echo "}" >> ${.TARGET}

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

sha224.ref:
	echo 'SHA-224 test suite:' > ${.TARGET}
	@echo 'SHA-224 ("") = d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f' >> ${.TARGET}
	@echo 'SHA-224 ("abc") =' \
		'23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7' >> ${.TARGET}
	@echo 'SHA-224 ("message digest") =' \
		'2cb21c83ae2f004de7e81c3c7019cbcb65b71ab656b22d6d0c39b8eb' >> ${.TARGET}
	@echo 'SHA-224 ("abcdefghijklmnopqrstuvwxyz") =' \
		'45a5f72c39c5cff2522eb3429799e49e5f44b356ef926bcf390dccc2' >> ${.TARGET}
	@echo 'SHA-224 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") =' \
		'bff72b4fcb7d75e5632900ac5f90d219e05e97a7bde72e740db393d9' >> ${.TARGET}
	@echo 'SHA-224 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") =' \
		'b50aecbe4e9bb0b57bc5f3ae760a8e01db24f203fb3cdcd13148046e' >> ${.TARGET}

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

sha384.ref:
	echo 'SHA-384 test suite:' > ${.TARGET}
	@echo 'SHA-384 ("") =' \
		'38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b' >> ${.TARGET}
	@echo 'SHA-384 ("abc") =' \
		'cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7' >> ${.TARGET}
	@echo 'SHA-384 ("message digest") =' \
		'473ed35167ec1f5d8e550368a3db39be54639f828868e9454c239fc8b52e3c61dbd0d8b4de1390c256dcbb5d5fd99cd5' >> ${.TARGET}
	@echo 'SHA-384 ("abcdefghijklmnopqrstuvwxyz") =' \
		'feb67349df3db6f5924815d6c3dc133f091809213731fe5c7b5f4999e463479ff2877f5f2936fa63bb43784b12f3ebb4' >> ${.TARGET}
	@echo 'SHA-384 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") =' \
		'1761336e3f7cbfe51deb137f026f89e01a448e3b1fafa64039c1464ee8732f11a5341a6f41e0c202294736ed64db1a84' >> ${.TARGET}
	@echo 'SHA-384 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") =' \
		'b12932b0627d1c060942f5447764155655bd4da0c9afa6dd9b9ef53129af1b8fb0195996d2de9ca0df9d821ffee67026' >> ${.TARGET}

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

sha512t224.ref:
	echo 'SHA-512224 test suite:' > ${.TARGET}
	@echo 'SHA-512224 ("") =' \
		'6ed0dd02806fa89e25de060c19d3ac86cabb87d6a0ddd05c333b84f4' >> ${.TARGET}
	@echo 'SHA-512224 ("abc") =' \
		'4634270f707b6a54daae7530460842e20e37ed265ceee9a43e8924aa' >> ${.TARGET}
	@echo 'SHA-512224 ("message digest") =' \
		'ad1a4db188fe57064f4f24609d2a83cd0afb9b398eb2fcaeaae2c564' >> ${.TARGET}
	@echo 'SHA-512224 ("abcdefghijklmnopqrstuvwxyz") =' \
		'ff83148aa07ec30655c1b40aff86141c0215fe2a54f767d3f38743d8' >> ${.TARGET}
	@echo 'SHA-512224 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") =' \
		'a8b4b9174b99ffc67d6f49be9981587b96441051e16e6dd036b140d3' >> ${.TARGET}
	@echo 'SHA-512224 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") =' \
		'ae988faaa47e401a45f704d1272d99702458fea2ddc6582827556dd2' >> ${.TARGET}

sha512t256.ref:
	echo 'SHA-512256 test suite:' > ${.TARGET}
	@echo 'SHA-512256 ("") =' \
		'c672b8d1ef56ed28ab87c3622c5114069bdd3ad7b8f9737498d0c01ecef0967a' >> ${.TARGET}
	@echo 'SHA-512256 ("abc") =' \
		'53048e2681941ef99b2e29b76b4c7dabe4c2d0c634fc6d46e0e2f13107e7af23' >> ${.TARGET}
	@echo 'SHA-512256 ("message digest") =' \
		'0cf471fd17ed69d990daf3433c89b16d63dec1bb9cb42a6094604ee5d7b4e9fb' >> ${.TARGET}
	@echo 'SHA-512256 ("abcdefghijklmnopqrstuvwxyz") =' \
		'fc3189443f9c268f626aea08a756abe7b726b05f701cb08222312ccfd6710a26' >> ${.TARGET}
	@echo 'SHA-512256 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") =' \
		'cdf1cc0effe26ecc0c13758f7b4a48e000615df241284185c39eb05d355bb9c8' >> ${.TARGET}
	@echo 'SHA-512256 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") =' \
		'2c9fdbc0c90bdd87612ee8455474f9044850241dc105b1e8b94b8ddf5fac9148' >> ${.TARGET}

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

skein256.ref:
	echo 'SKEIN256 test suite:' > ${.TARGET}
	@echo 'SKEIN256 ("") = c8877087da56e072870daa843f176e9453115929094c3a40c463a196c29bf7ba' >> ${.TARGET}
	@echo 'SKEIN256 ("abc") = 258bdec343b9fde1639221a5ae0144a96e552e5288753c5fec76c05fc2fc1870' >> ${.TARGET}
	@echo 'SKEIN256 ("message digest") =' \
		'4d2ce0062b5eb3a4db95bc1117dd8aa014f6cd50fdc8e64f31f7d41f9231e488' >> ${.TARGET}
	@echo 'SKEIN256 ("abcdefghijklmnopqrstuvwxyz") =' \
		'46d8440685461b00e3ddb891b2ecc6855287d2bd8834a95fb1c1708b00ea5e82' >> ${.TARGET}
	@echo 'SKEIN256 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") =' \
		'7c5eb606389556b33d34eb2536459528dc0af97adbcd0ce273aeb650f598d4b2' >> ${.TARGET}
	@echo 'SKEIN256 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") =' \
		'4def7a7e5464a140ae9c3a80279fbebce4bd00f9faad819ab7e001512f67a10d' >> ${.TARGET}

skein512.ref:
	echo 'SKEIN512 test suite:' > ${.TARGET}
	@echo 'SKEIN512 ("") =' \
		'bc5b4c50925519c290cc634277ae3d6257212395cba733bbad37a4af0fa06af41fca7903d06564fea7a2d3730dbdb80c1f85562dfcc070334ea4d1d9e72cba7a' >> ${.TARGET}
	@echo 'SKEIN512 ("abc") =' \
		'8f5dd9ec798152668e35129496b029a960c9a9b88662f7f9482f110b31f9f93893ecfb25c009baad9e46737197d5630379816a886aa05526d3a70df272d96e75' >> ${.TARGET}
	@echo 'SKEIN512 ("message digest") =' \
		'15b73c158ffb875fed4d72801ded0794c720b121c0c78edf45f900937e6933d9e21a3a984206933d504b5dbb2368000411477ee1b204c986068df77886542fcc' >> ${.TARGET}
	@echo 'SKEIN512 ("abcdefghijklmnopqrstuvwxyz") =' \
		'23793ad900ef12f9165c8080da6fdfd2c8354a2929b8aadf83aa82a3c6470342f57cf8c035ec0d97429b626c4d94f28632c8f5134fd367dca5cf293d2ec13f8c' >> ${.TARGET}
	@echo 'SKEIN512 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") =' \
		'0c6bed927e022f5ddcf81877d42e5f75798a9f8fd3ede3d83baac0a2f364b082e036c11af35fe478745459dd8f5c0b73efe3c56ba5bb2009208d5a29cc6e469c' >> ${.TARGET}
	@echo 'SKEIN512 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") =' \
		'2ca9fcffb3456f297d1b5f407014ecb856f0baac8eb540f534b1f187196f21e88f31103128c2f03fcc9857d7a58eb66f9525e2302d88833ee069295537a434ce' >> ${.TARGET}

skein1024.ref:
	echo 'SKEIN1024 test suite:' > ${.TARGET}
	@echo 'SKEIN1024 ("") =' \
		'0fff9563bb3279289227ac77d319b6fff8d7e9f09da1247b72a0a265cd6d2a62645ad547ed8193db48cff847c06494a03f55666d3b47eb4c20456c9373c86297d630d5578ebd34cb40991578f9f52b18003efa35d3da6553ff35db91b81ab890bec1b189b7f52cb2a783ebb7d823d725b0b4a71f6824e88f68f982eefc6d19c6' >> ${.TARGET}
	@echo 'SKEIN1024 ("abc") =' \
		'35a599a0f91abcdb4cb73c19b8cb8d947742d82c309137a7caed29e8e0a2ca7a9ff9a90c34c1908cc7e7fd99bb15032fb86e76df21b72628399b5f7c3cc209d7bb31c99cd4e19465622a049afbb87c03b5ce3888d17e6e667279ec0aa9b3e2712624c01b5f5bbe1a564220bdcf6990af0c2539019f313fdd7406cca3892a1f1f' >> ${.TARGET}
	@echo 'SKEIN1024 ("message digest") =' \
		'ea891f5268acd0fac97467fc1aa89d1ce8681a9992a42540e53babee861483110c2d16f49e73bac27653ff173003e40cfb08516cd34262e6af95a5d8645c9c1abb3e813604d508b8511b30f9a5c1b352aa0791c7d2f27b2706dccea54bc7de6555b5202351751c3299f97c09cf89c40f67187e2521c0fad82b30edbb224f0458' >> ${.TARGET}
	@echo 'SKEIN1024 ("abcdefghijklmnopqrstuvwxyz") =' \
		'f23d95c2a25fbcd0e797cd058fec39d3c52d2b5afd7a9af1df934e63257d1d3dcf3246e7329c0f1104c1e51e3d22e300507b0c3b9f985bb1f645ef49835080536becf83788e17fed09c9982ba65c3cb7ffe6a5f745b911c506962adf226e435c42f6f6bc08d288f9c810e807e3216ef444f3db22744441deefa4900982a1371f' >> ${.TARGET}
	@echo 'SKEIN1024 ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789") =' \
		'cf3889e8a8d11bfd3938055d7d061437962bc5eac8ae83b1b71c94be201b8cf657fdbfc38674997a008c0c903f56a23feb3ae30e012377f1cfa080a9ca7fe8b96138662653fb3335c7d06595bf8baf65e215307532094cfdfa056bd8052ab792a3944a2adaa47b30335b8badb8fe9eb94fe329cdca04e58bbc530f0af709f469' >> ${.TARGET}
	@echo 'SKEIN1024 ("12345678901234567890123456789012345678901234567890123456789012345678901234567890") =' \
		'cf21a613620e6c119eca31fdfaad449a8e02f95ca256c21d2a105f8e4157048f9fe1e897893ea18b64e0e37cb07d5ac947f27ba544caf7cbc1ad094e675aed77a366270f7eb7f46543bccfa61c526fd628408058ed00ed566ac35a9761d002e629c4fb0d430b2f4ad016fcc49c44d2981c4002da0eecc42144160e2eaea4855a' >> ${.TARGET}

.include <bsd.test.mk>
