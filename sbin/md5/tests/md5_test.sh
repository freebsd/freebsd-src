#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2022 Kyle Evans <kevans@FreeBSD.org>
# Copyright (c) 2023 Klara, Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

n=8
algorithms="md5 sha1 sha224 sha256 sha384 sha512 sha512t224 sha512t256 rmd160 skein256 skein512 skein1024"

name_bsd_md5="MD5"
name_bsd_sha1="SHA1"
name_bsd_sha224="SHA224"
name_bsd_sha256="SHA256"
name_bsd_sha384="SHA384"
name_bsd_sha512="SHA512"
name_bsd_sha512t224="SHA512t224"
name_bsd_sha512t256="SHA512t256"
name_bsd_rmd160="RMD160"
name_bsd_skein256="Skein256"
name_bsd_skein512="Skein512"
name_bsd_skein1024="Skein1024"

name_perl_sha1="SHA1"
name_perl_sha224="SHA224"
name_perl_sha256="SHA256"
name_perl_sha384="SHA384"
name_perl_sha512="SHA512"
name_perl_sha512t224="SHA512/224"
name_perl_sha512t256="SHA512/256"

alg_perl_sha1=""
alg_perl_sha224="-a 224"
alg_perl_sha256="-a 256"
alg_perl_sha384="-a 384"
alg_perl_sha512="-a 512"
alg_perl_sha512t224="-a 512224"
alg_perl_sha512t256="-a 512256"

inp_1=""
inp_2="a"
inp_3="abc"
inp_4="message digest"
inp_5="abcdefghijklmnopqrstuvwxyz"
inp_6="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
inp_7="12345678901234567890123456789012345678901234567890123456789012345678901234567890"
inp_8="MD5 has not yet (2001-09-03) been broken, but sufficient attacks have been made that its security is in some doubt"

out_1_md5="d41d8cd98f00b204e9800998ecf8427e"
out_2_md5="0cc175b9c0f1b6a831c399e269772661"
out_3_md5="900150983cd24fb0d6963f7d28e17f72"
out_4_md5="f96b697d7cb7938d525a2f31aaf161d0"
out_5_md5="c3fcd3d76192e4007dfb496cca67e13b"
out_6_md5="d174ab98d277d9f5a5611c2c9f419d9f"
out_7_md5="57edf4a22be3c955ac49da2e2107b67a"
out_8_md5="b50663f41d44d92171cb9976bc118538"

out_1_sha1="da39a3ee5e6b4b0d3255bfef95601890afd80709"
out_2_sha1="86f7e437faa5a7fce15d1ddcb9eaeaea377667b8"
out_3_sha1="a9993e364706816aba3e25717850c26c9cd0d89d"
out_4_sha1="c12252ceda8be8994d5fa0290a47231c1d16aae3"
out_5_sha1="32d10c7b8cf96570ca04ce37f2a19d84240d3a89"
out_6_sha1="761c457bf73b14d27e9e9265c46f4b4dda11f940"
out_7_sha1="50abf5706a150990a08b2c5ea40fa0e585554732"
out_8_sha1="18eca4333979c4181199b7b4fab8786d16cf2846"

out_1_sha224="d14a028c2a3a2bc9476102bb288234c415a2b01f828ea62ac5b3e42f"
out_2_sha224="abd37534c7d9a2efb9465de931cd7055ffdb8879563ae98078d6d6d5"
out_3_sha224="23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7"
out_4_sha224="2cb21c83ae2f004de7e81c3c7019cbcb65b71ab656b22d6d0c39b8eb"
out_5_sha224="45a5f72c39c5cff2522eb3429799e49e5f44b356ef926bcf390dccc2"
out_6_sha224="bff72b4fcb7d75e5632900ac5f90d219e05e97a7bde72e740db393d9"
out_7_sha224="b50aecbe4e9bb0b57bc5f3ae760a8e01db24f203fb3cdcd13148046e"
out_8_sha224="5ae55f3779c8a1204210d7ed7689f661fbe140f96f272ab79e19d470"

out_1_sha256="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
out_2_sha256="ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb"
out_3_sha256="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
out_4_sha256="f7846f55cf23e14eebeab5b4e1550cad5b509e3348fbc4efa3a1413d393cb650"
out_5_sha256="71c480df93d6ae2f1efad1447c66c9525e316218cf51fc8d9ed832f2daf18b73"
out_6_sha256="db4bfcbd4da0cd85a60c3c37d3fbd8805c77f15fc6b1fdfe614ee0a7c8fdb4c0"
out_7_sha256="f371bc4a311f2b009eef952dd83ca80e2b60026c8e935592d0f9c308453c813e"
out_8_sha256="e6eae09f10ad4122a0e2a4075761d185a272ebd9f5aa489e998ff2f09cbfdd9f"

out_1_sha384="38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b"
out_2_sha384="54a59b9f22b0b80880d8427e548b7c23abd873486e1f035dce9cd697e85175033caa88e6d57bc35efae0b5afd3145f31"
out_3_sha384="cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7"
out_4_sha384="473ed35167ec1f5d8e550368a3db39be54639f828868e9454c239fc8b52e3c61dbd0d8b4de1390c256dcbb5d5fd99cd5"
out_5_sha384="feb67349df3db6f5924815d6c3dc133f091809213731fe5c7b5f4999e463479ff2877f5f2936fa63bb43784b12f3ebb4"
out_6_sha384="1761336e3f7cbfe51deb137f026f89e01a448e3b1fafa64039c1464ee8732f11a5341a6f41e0c202294736ed64db1a84"
out_7_sha384="b12932b0627d1c060942f5447764155655bd4da0c9afa6dd9b9ef53129af1b8fb0195996d2de9ca0df9d821ffee67026"
out_8_sha384="99428d401bf4abcd4ee0695248c9858b7503853acfae21a9cffa7855f46d1395ef38596fcd06d5a8c32d41a839cc5dfb"

out_1_sha512="cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e"
out_2_sha512="1f40fc92da241694750979ee6cf582f2d5d7d28e18335de05abc54d0560e0f5302860c652bf08d560252aa5e74210546f369fbbbce8c12cfc7957b2652fe9a75"
out_3_sha512="ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f"
out_4_sha512="107dbf389d9e9f71a3a95f6c055b9251bc5268c2be16d6c13492ea45b0199f3309e16455ab1e96118e8a905d5597b72038ddb372a89826046de66687bb420e7c"
out_5_sha512="4dbff86cc2ca1bae1e16468a05cb9881c97f1753bce3619034898faa1aabe429955a1bf8ec483d7421fe3c1646613a59ed5441fb0f321389f77f48a879c7b1f1"
out_6_sha512="1e07be23c26a86ea37ea810c8ec7809352515a970e9253c26f536cfc7a9996c45c8370583e0a78fa4a90041d71a4ceab7423f19c71b9d5a3e01249f0bebd5894"
out_7_sha512="72ec1ef1124a45b047e8b7c75a932195135bb61de24ec0d1914042246e0aec3a2354e093d76f3048b456764346900cb130d2a4fd5dd16abb5e30bcb850dee843"
out_8_sha512="e8a835195e039708b13d9131e025f4441dbdc521ce625f245a436dcd762f54bf5cb298d96235e6c6a304e087ec8189b9512cbdf6427737ea82793460c367b9c3"

out_1_sha512t224="6ed0dd02806fa89e25de060c19d3ac86cabb87d6a0ddd05c333b84f4"
out_2_sha512t224="d5cdb9ccc769a5121d4175f2bfdd13d6310e0d3d361ea75d82108327"
out_3_sha512t224="4634270f707b6a54daae7530460842e20e37ed265ceee9a43e8924aa"
out_4_sha512t224="ad1a4db188fe57064f4f24609d2a83cd0afb9b398eb2fcaeaae2c564"
out_5_sha512t224="ff83148aa07ec30655c1b40aff86141c0215fe2a54f767d3f38743d8"
out_6_sha512t224="a8b4b9174b99ffc67d6f49be9981587b96441051e16e6dd036b140d3"
out_7_sha512t224="ae988faaa47e401a45f704d1272d99702458fea2ddc6582827556dd2"
out_8_sha512t224="b3c3b945249b0c8c94aba76ea887bcaad5401665a1fbeb384af4d06b"

out_1_sha512t256="c672b8d1ef56ed28ab87c3622c5114069bdd3ad7b8f9737498d0c01ecef0967a"
out_2_sha512t256="455e518824bc0601f9fb858ff5c37d417d67c2f8e0df2babe4808858aea830f8"
out_3_sha512t256="53048e2681941ef99b2e29b76b4c7dabe4c2d0c634fc6d46e0e2f13107e7af23"
out_4_sha512t256="0cf471fd17ed69d990daf3433c89b16d63dec1bb9cb42a6094604ee5d7b4e9fb"
out_5_sha512t256="fc3189443f9c268f626aea08a756abe7b726b05f701cb08222312ccfd6710a26"
out_6_sha512t256="cdf1cc0effe26ecc0c13758f7b4a48e000615df241284185c39eb05d355bb9c8"
out_7_sha512t256="2c9fdbc0c90bdd87612ee8455474f9044850241dc105b1e8b94b8ddf5fac9148"
out_8_sha512t256="dd095fc859b336c30a52548b3dc59fcc0d1be8616ebcf3368fad23107db2d736"

out_1_rmd160="9c1185a5c5e9fc54612808977ee8f548b2258d31"
out_2_rmd160="0bdc9d2d256b3ee9daae347be6f4dc835a467ffe"
out_3_rmd160="8eb208f7e05d987a9b044a8e98c6b087f15a0bfc"
out_4_rmd160="5d0689ef49d2fae572b881b123a85ffa21595f36"
out_5_rmd160="f71c27109c692c1b56bbdceb5b9d2865b3708dbc"
out_6_rmd160="b0e20b6e3116640286ed3a87a5713079b21f5189"
out_7_rmd160="9b752e45573d4b39f4dbd3323cab82bf63326bfb"
out_8_rmd160="5feb69c6bf7c29d95715ad55f57d8ac5b2b7dd32"

out_1_skein256="c8877087da56e072870daa843f176e9453115929094c3a40c463a196c29bf7ba"
out_2_skein256="7fba44ff1a31d71a0c1f82e6e82fb5e9ac6c92a39c9185b9951fed82d82fe635"
out_3_skein256="258bdec343b9fde1639221a5ae0144a96e552e5288753c5fec76c05fc2fc1870"
out_4_skein256="4d2ce0062b5eb3a4db95bc1117dd8aa014f6cd50fdc8e64f31f7d41f9231e488"
out_5_skein256="46d8440685461b00e3ddb891b2ecc6855287d2bd8834a95fb1c1708b00ea5e82"
out_6_skein256="7c5eb606389556b33d34eb2536459528dc0af97adbcd0ce273aeb650f598d4b2"
out_7_skein256="4def7a7e5464a140ae9c3a80279fbebce4bd00f9faad819ab7e001512f67a10d"
out_8_skein256="d9c017dbe355f318d036469eb9b5fbe129fc2b5786a9dc6746a516eab6fe0126"

out_1_skein512="bc5b4c50925519c290cc634277ae3d6257212395cba733bbad37a4af0fa06af41fca7903d06564fea7a2d3730dbdb80c1f85562dfcc070334ea4d1d9e72cba7a"
out_2_skein512="b1cd8d33f61b3737adfd59bb13ad82f4a9548e92f22956a8976cca3fdb7fee4fe91698146c4197cec85d38b83c5d93bdba92c01fd9a53870d0c7f967bc62bdce"
out_3_skein512="8f5dd9ec798152668e35129496b029a960c9a9b88662f7f9482f110b31f9f93893ecfb25c009baad9e46737197d5630379816a886aa05526d3a70df272d96e75"
out_4_skein512="15b73c158ffb875fed4d72801ded0794c720b121c0c78edf45f900937e6933d9e21a3a984206933d504b5dbb2368000411477ee1b204c986068df77886542fcc"
out_5_skein512="23793ad900ef12f9165c8080da6fdfd2c8354a2929b8aadf83aa82a3c6470342f57cf8c035ec0d97429b626c4d94f28632c8f5134fd367dca5cf293d2ec13f8c"
out_6_skein512="0c6bed927e022f5ddcf81877d42e5f75798a9f8fd3ede3d83baac0a2f364b082e036c11af35fe478745459dd8f5c0b73efe3c56ba5bb2009208d5a29cc6e469c"
out_7_skein512="2ca9fcffb3456f297d1b5f407014ecb856f0baac8eb540f534b1f187196f21e88f31103128c2f03fcc9857d7a58eb66f9525e2302d88833ee069295537a434ce"
out_8_skein512="1131f2aaa0e97126c9314f9f968cc827259bbfabced2943bb8c9274448998fb3b78738b4580dd500c76105fd3c03e465e1414f2c29664286b1f79d3e51128125"

out_1_skein1024="0fff9563bb3279289227ac77d319b6fff8d7e9f09da1247b72a0a265cd6d2a62645ad547ed8193db48cff847c06494a03f55666d3b47eb4c20456c9373c86297d630d5578ebd34cb40991578f9f52b18003efa35d3da6553ff35db91b81ab890bec1b189b7f52cb2a783ebb7d823d725b0b4a71f6824e88f68f982eefc6d19c6"
out_2_skein1024="6ab4c4ba9814a3d976ec8bffa7fcc638ceba0544a97b3c98411323ffd2dc936315d13dc93c13c4e88cda6f5bac6f2558b2d8694d3b6143e40d644ae43ca940685cb37f809d3d0550c56cba8036dee729a4f8fb960732e59e64d57f7f7710f8670963cdcdc95b41daab4855fcf8b6762a64b173ee61343a2c7689af1d293eba97"
out_3_skein1024="35a599a0f91abcdb4cb73c19b8cb8d947742d82c309137a7caed29e8e0a2ca7a9ff9a90c34c1908cc7e7fd99bb15032fb86e76df21b72628399b5f7c3cc209d7bb31c99cd4e19465622a049afbb87c03b5ce3888d17e6e667279ec0aa9b3e2712624c01b5f5bbe1a564220bdcf6990af0c2539019f313fdd7406cca3892a1f1f"
out_4_skein1024="ea891f5268acd0fac97467fc1aa89d1ce8681a9992a42540e53babee861483110c2d16f49e73bac27653ff173003e40cfb08516cd34262e6af95a5d8645c9c1abb3e813604d508b8511b30f9a5c1b352aa0791c7d2f27b2706dccea54bc7de6555b5202351751c3299f97c09cf89c40f67187e2521c0fad82b30edbb224f0458"
out_5_skein1024="f23d95c2a25fbcd0e797cd058fec39d3c52d2b5afd7a9af1df934e63257d1d3dcf3246e7329c0f1104c1e51e3d22e300507b0c3b9f985bb1f645ef49835080536becf83788e17fed09c9982ba65c3cb7ffe6a5f745b911c506962adf226e435c42f6f6bc08d288f9c810e807e3216ef444f3db22744441deefa4900982a1371f"
out_6_skein1024="cf3889e8a8d11bfd3938055d7d061437962bc5eac8ae83b1b71c94be201b8cf657fdbfc38674997a008c0c903f56a23feb3ae30e012377f1cfa080a9ca7fe8b96138662653fb3335c7d06595bf8baf65e215307532094cfdfa056bd8052ab792a3944a2adaa47b30335b8badb8fe9eb94fe329cdca04e58bbc530f0af709f469"
out_7_skein1024="cf21a613620e6c119eca31fdfaad449a8e02f95ca256c21d2a105f8e4157048f9fe1e897893ea18b64e0e37cb07d5ac947f27ba544caf7cbc1ad094e675aed77a366270f7eb7f46543bccfa61c526fd628408058ed00ed566ac35a9761d002e629c4fb0d430b2f4ad016fcc49c44d2981c4002da0eecc42144160e2eaea4855a"
out_8_skein1024="e6799b78db54085a2be7ff4c8007f147fa88d326abab30be0560b953396d8802feee9a15419b48a467574e9283be15685ca8a079ee52b27166b64dd70b124b1d4e4f6aca37224c3f2685e67e67baef9f94b905698adc794a09672aba977a61b20966912acdb08c21a2c37001785355dc884751a21f848ab36e590331ff938138"

for alg in $algorithms ; do
	eval "
atf_test_case self_test_${alg}
self_test_${alg}_head() {
	atf_set descr \"self-test for \$name_bsd_${alg}\"
	atf_set require.progs \"${alg}\"
}
self_test_${alg}_body() {
	atf_check -o ignore ${alg} --self-test
}
"
	for i in $(seq $n) ; do
		eval "
atf_test_case bsd_${alg}_vec${i}
bsd_${alg}_vec${i}_head() {
	atf_set descr \"BSD mode \$name_bsd_${alg} test vector ${i}\"
	atf_set require.progs \"${alg}\"
}
bsd_${alg}_vec${i}_body() {
	printf '%s' \"\$inp_${i}\" >in
	atf_check -o inline:\"\$out_${i}_${alg}\n\" ${alg} <in
	atf_check -o inline:\"\$name_bsd_${alg} (in) = \$out_${i}_${alg}\n\" ${alg} in
	atf_check -o inline:\"\$name_bsd_${alg} (-) = \$out_${i}_${alg}\n\" ${alg} - <in
	atf_check -o inline:\"\$out_${i}_${alg} in\n\" ${alg} -r in
	atf_check -o inline:\"\$out_${i}_${alg} -\n\" ${alg} -r - <in
	# -q overrides -r regardless of order
	for opt in -q -qr -rq ; do
		atf_check -o inline:\"\$out_${i}_${alg}\n\" ${alg} \${opt} in
	done
	atf_check -o inline:\"\$inp_${i}\$out_${i}_${alg}\n\" ${alg} -p <in
	atf_check -o inline:\"\$out_${i}_${alg}\n\" ${alg} -s \"\$inp_${i}\"
}
"
		eval "
atf_test_case gnu_${alg}_vec${i}
gnu_${alg}_vec${i}_head() {
	atf_set descr \"GNU mode \$name_bsd_${alg} test vector ${i}\"
	atf_set require.progs \"${alg}sum\"
}
gnu_${alg}_vec${i}_body() {
	printf '%s' \"\$inp_${i}\" >in
	atf_check -o inline:\"\$out_${i}_${alg}  -\n\" ${alg}sum <in
	atf_check -o inline:\"\$out_${i}_${alg} *-\n\" ${alg}sum -b <in
	atf_check -o inline:\"\$out_${i}_${alg}  in\n\" ${alg}sum in
	atf_check -o inline:\"\$out_${i}_${alg}  -\n\" ${alg}sum - <in
	atf_check -o inline:\"\$out_${i}_${alg} *in\n\" ${alg}sum -b in
	atf_check -o inline:\"\$out_${i}_${alg} *-\n\" ${alg}sum -b - <in
	atf_check -o inline:\"\$name_bsd_${alg} (in) = \$out_${i}_${alg}\n\" ${alg}sum --tag in
	atf_check -o inline:\"\$name_bsd_${alg} (-) = \$out_${i}_${alg}\n\" ${alg}sum --tag - <in
	atf_check -o inline:\"\$out_${i}_${alg}  in\0\" ${alg}sum -z in
	atf_check -o inline:\"\$out_${i}_${alg}  -\0\" ${alg}sum -z - <in
}
"
		eval "
atf_test_case perl_${alg}_vec${i}
perl_${alg}_vec${i}_head() {
	atf_set descr \"Perl mode \$name_bsd_${alg} test vector ${i}\"
	atf_set require.progs \"shasum\"
}
perl_${alg}_vec${i}_body() {
	[ -n \"\$name_perl_${alg}\" ] || atf_skip \"shasum does not support ${alg}\"
	printf '%s' \"\$inp_${i}\" >in
	atf_check -o inline:\"\$out_${i}_${alg}  -\n\" shasum \$alg_perl_${alg} <in
	atf_check -o inline:\"\$out_${i}_${alg} *-\n\" shasum \$alg_perl_${alg} -b <in
	atf_check -o inline:\"\$out_${i}_${alg} U-\n\" shasum \$alg_perl_${alg} -U <in
	atf_check -o inline:\"\$out_${i}_${alg}  in\n\" shasum \$alg_perl_${alg} in
	atf_check -o inline:\"\$out_${i}_${alg}  -\n\" shasum \$alg_perl_${alg} - <in
	atf_check -o inline:\"\$out_${i}_${alg} *in\n\" shasum \$alg_perl_${alg} -b in
	atf_check -o inline:\"\$out_${i}_${alg} *-\n\" shasum \$alg_perl_${alg} -b - <in
	atf_check -o inline:\"\$out_${i}_${alg} Uin\n\" shasum \$alg_perl_${alg} -U in
	atf_check -o inline:\"\$out_${i}_${alg} U-\n\" shasum \$alg_perl_${alg} -U - <in
	atf_check -o inline:\"\$name_perl_${alg} (in) = \$out_${i}_${alg}\n\" shasum \$alg_perl_${alg} --tag in
	atf_check -o inline:\"\$name_perl_${alg} (-) = \$out_${i}_${alg}\n\" shasum \$alg_perl_${alg} --tag - <in
}
"
	done
	eval "
atf_test_case gnu_check_${alg}
gnu_check_${alg}_head() {
	atf_set descr \"GNU mode check test for \$name_bsd_${alg}\"
	atf_set require.progs \"${alg}sum\"
}
gnu_check_${alg}_body() {
	:>digests
	:>stdout
	:>stderr
	rv=0
	printf '%s' \"\$inp_2\" >inp2
	printf '%s  inp%d\n' \"\$out_2_${alg}\" 2 >>digests
	printf 'inp%d: OK\n' 2 >>stdout
	atf_check -o file:stdout -e file:stderr -s exit:$rv ${alg}sum -c digests
	printf '%s' \"\$inp_3\" >inp3
	printf '%s  inp%d\n' \"malformed\" 3 >>digests
	printf '%ssum: WARNING: 1 line is improperly formatted\n' ${alg} >>stderr
	rv=1
	atf_check -o file:stdout -e file:stderr -s exit:$rv ${alg}sum -c digests
	printf '%s' \"\$inp_4\" >inp4
	printf '%s  inp%d\n' \"\$out_4_${alg}\" 4 | tr abcdef fedcba >>digests
	printf 'inp%d: FAILED\n' 4 >>stdout
	printf '%ssum: WARNING: 1 computed checksum did NOT match\n' ${alg} >>stderr
	atf_check -o file:stdout -e file:stderr -s exit:$rv ${alg}sum -c digests
	grep -v OK stdout >quiet
	atf_check -o file:quiet -e file:stderr -s exit:$rv ${alg}sum --check --quiet digests
	atf_check -s exit:$rv ${alg}sum --check --status digests
}
"
	eval "
atf_test_case perl_check_${alg}
perl_check_${alg}_head() {
	atf_set descr \"Perl mode check test for \$name_bsd_${alg}\"
	atf_set require.progs \"shasum\"
}
perl_check_${alg}_body() {
	[ -n \"\$name_perl_${alg}\" ] || atf_skip \"shasum does not support ${alg}\"
	:>digests
	:>stdout
	:>stderr
	rv=0
	printf '%s' \"\$inp_2\" >inp2
	printf '%s  inp%d\n' \"\$out_2_${alg}\" 2 >>digests
	printf 'inp%d: OK\n' 2 >>stdout
	atf_check -o file:stdout -e file:stderr -s exit:$rv shasum \$alg_perl_${alg} -c digests
	printf '%s' \"\$inp_3\" >inp3
	printf '%s  inp%d\n' \"malformed\" 3 >>digests
	printf 'shasum: WARNING: 1 line is improperly formatted\n' >>stderr
	rv=1
	atf_check -o file:stdout -e file:stderr -s exit:$rv shasum \$alg_perl_${alg} -c digests
	printf '%s' \"\$inp_4\" >inp4
	printf '%s  inp%d\n' \"\$out_4_${alg}\" 4 | tr abcdef fedcba >>digests
	printf 'inp%d: FAILED\n' 4 >>stdout
	printf 'shasum: WARNING: 1 computed checksum did NOT match\n' >>stderr
	atf_check -o file:stdout -e file:stderr -s exit:$rv shasum \$alg_perl_${alg} -c digests
	grep -v OK stdout >quiet
	atf_check -o file:quiet -e file:stderr -s exit:$rv shasum \$alg_perl_${alg} --check --quiet digests
	atf_check -s exit:$rv shasum \$alg_perl_${alg} --check --status digests
}
"
done

atf_test_case gnu_bflag
gnu_bflag_head()
{
	atf_set descr "Verify GNU binary mode"
	atf_set require.progs "sha256sum"
}
gnu_bflag_body()
{
	echo foo >a
	echo bar >b

	(sha256 -q a | tr -d '\n'; echo " *a") > expected
	(sha256 -q b | tr -d '\n'; echo " *b") >> expected

	atf_check -o file:expected sha256sum -b a b
	atf_check -o file:expected sha256sum --binary a b
}

atf_test_case gnu_cflag
gnu_cflag_head()
{
	atf_set descr "Verify handling of missing files in GNU check mode"
	atf_set require.progs "sha256sum"
}
gnu_cflag_body()
{

	# Verify that the *sum -c mode works even if some files are missing.
	# PR 267722 identified that we would never advance past the first record
	# to check against.  As a result, things like checking the published
	# checksums for the install media became a more manual process again if
	# you didn't download all of the images.
	for i in 2 3 4 ; do
		eval "printf '%s  inp%d\n' \"\$out_${i}_sha256\" ${i}"
	done >digests
	for combo in "2 3 4" "3 4" "2 4" "2 3" "2" "3" "4" ""; do
		rm -f inp2 inp3 inp4
		:> expected
		cnt=0
		for i in ${combo}; do
			eval "printf '%s' \"\$inp_${i}\"" > inp${i}
			printf "inp%d: OK\n" ${i} >> expected
			cnt=$((cnt + 1))
		done

		err=0
		[ "$cnt" -eq 3 ] || err=1
		atf_check -o file:expected -e ignore -s exit:${err} \
		    sha256sum -c digests
		atf_check -o file:expected -e ignore -s exit:0 \
		    sha256sum --ignore-missing -c digests
	done

}

atf_test_case gnu_cflag_mode
gnu_cflag_mode_head()
{
	atf_set descr "Verify handling of input modes in GNU check mode"
	atf_set require.progs "sha1sum"
}
gnu_cflag_mode_body()
{
	printf "The Magic Words are 01010011 01001111\r\n" >input
	# The first line is malformed per GNU coreutils but matches
	# what we produce when mode == mode_bsd && output_mode ==
	# output_reverse (i.e. `sha1 -r`) so we want to support it.
	cat >digests <<EOF
53d88300dfb2be42f0ef25e3d9de798e31bb7e69 input
53d88300dfb2be42f0ef25e3d9de798e31bb7e69 *input
53d88300dfb2be42f0ef25e3d9de798e31bb7e69  input
2290cf6ba4ac5387e520088de760b71a523871b0 ^input
c1065e0d2bbc1c67dcecee0187d61316fb9c5582 Uinput
EOF
	atf_check sha1sum --quiet --check digests
}

atf_init_test_cases()
{
	for alg in $algorithms ; do
		atf_add_test_case self_test_${alg}
		for i in $(seq $n) ; do
			atf_add_test_case bsd_${alg}_vec${i}
			atf_add_test_case gnu_${alg}_vec${i}
			atf_add_test_case perl_${alg}_vec${i}
		done
		atf_add_test_case gnu_check_${alg}
		atf_add_test_case perl_check_${alg}
	done
	atf_add_test_case gnu_bflag
	atf_add_test_case gnu_cflag
	atf_add_test_case gnu_cflag_mode
}
