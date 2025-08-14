#
# Copyright (c) 2025 Dag-Erling Smørgrav <des@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

. $(atf_get_srcdir)/certctl.subr

# Random sets of eight non-colliding names
set1()
{
	cat <<EOF
AVOYKJHSLFHWPVQMKBHENUAHJTEGMCCB 0ca83bbe
UYSYXKDNNJTYOQPBGIKQDHRJYZHTDPKK 0d9a6512
LODHGFXMZYKGOKAYGWTMMYQJYHDATDDM 4e6219f5
NBBTQHJLHKBFFFWJTHHSNKOQYMGLHLPW 5dd76abc
BJFAQZXZHYQLIDDPCAQFPDMNXICUXBXW ad68573d
IOKNTHVEVVIJMNMYAVILMEMQQWLVRESN b577803d
BHGMAJJGNJPIVMHMFCUTJLGFROJICEKN c98a6338
HCRFQMGDQJALMLUQNXMPGLXFLLJRODJW f50c6379
EOF
}

set2()
{
	cat <<EOF
GOHKZTSKIPDSYNLMGYXGLROPTATELXIU 30789c88
YOOTYHEGHZIYFXOBLNKENPSJUDGOPJJU 7fadbc13
ETRINNYBGKIENAVGOKVJYFSSHFZIJZRH 8ed664af
DBFGMFFMRNLPQLQPOLXOEUVLCRXLRSWT 8f34355e
WFOPBQPLQFHDHZOUQFEIDGSYDUOTSNDQ ac0471df
HMNETZMGNIWRGXQCVZXVZGWSGFBRRDQC b32f1472
SHFYBXDVAUACBFPPAIGDAQIAGYOYGMQE baca75fa
PCBGDNVPYCDGNRQSGRSLXFHYKXLAVLHW ddeeae01
EOF
}

set3()
{
	cat <<EOF
NJWIRLPWAIICVJBKXXHFHLCPAERZATRL 000aa2e5
RJAENDPOCZQEVCPFUWOWDXPCSMYJPVYC 021b95a3
PQUQDSWHBNVLBTNBGONYRLGZZVEFXVLO 071e8c50
VZEXRKJUPZSFBDWBOLUZXOGLNTEAPCZM 3af7bb9b
ZXOWOXQTXNZMAMZIWVFDZDJEWOOAGAOH 48d5c7cc
KQSFQYVJMFTMADIHJIWGSQISWKSHRYQO 509f5ba1
AIECYSLWZOIEPJWWUTWSQXCNCIHHZHYI 8cb0c503
RFHWDJZEPOFLMPGXAHVEJFHCDODAPVEV 9ae4e049
EOF
}

# Random set of three colliding names
collhash=f2888ce3
coll()
{
	cat <<EOF
EJFTZEOANQLOYPEHWWXBWEWEFVKHMSNA $collhash
LEMRWZAZLKZLPPSFLNLQZVGKKBEOFYWG $collhash
ZWUPHYWKKTVEFBJOLLPDAIKGRDFVXZID $collhash
EOF
}

certctl_setup()
{
	export DESTDIR="$PWD"

	# Create input directories
	mkdir -p usr/share/certs/trusted
	mkdir -p usr/share/certs/untrusted
	mkdir -p usr/local/share/certs

	# Create output directories
	mkdir -p etc/ssl/certs
	mkdir -p etc/ssl/untrusted

	# Generate a random key
	keyname="testkey"
	gen_key ${keyname}

	# Generate certificates
	set1 | while read crtname hash ; do
		gen_crt ${crtname} ${keyname}
		mv ${crtname}.crt usr/share/certs/trusted
	done
	coll | while read crtname hash ; do
		gen_crt ${crtname} ${keyname}
		mv ${crtname}.crt usr/share/certs/trusted
	done
	set2 | while read crtname hash ; do
		gen_crt ${crtname} ${keyname}
		openssl x509 -in ${crtname}.crt
		rm ${crtname}.crt
	done >usr/local/share/certs/bundle.crt
	set3 | while read crtname hash ; do
		gen_crt ${crtname} ${keyname}
		mv ${crtname}.crt usr/share/certs/untrusted
	done
}

check_trusted() {
	local crtname=$1
	local subject="$(subject ${crtname})"
	local c=${2:-1}

	atf_check -o match:"found: ${c}\$" \
	    openssl storeutl -noout -subject "${subject}" \
	    etc/ssl/certs
	atf_check -o match:"found: 0\$" \
	    openssl storeutl -noout -subject "${subject}" \
	    etc/ssl/untrusted
}

check_untrusted() {
	local crtname=$1
	local subject="$(subject ${crtname})"
	local c=${2:-1}

	atf_check -o match:"found: 0\$" \
	    openssl storeutl -noout -subject "${subject}" \
	    etc/ssl/certs
	atf_check -o match:"found: ${c}\$" \
	    openssl storeutl -noout -subject "${subject}" \
	    etc/ssl/untrusted
}

check_in_bundle() {
	local crtfile=$1
	local line

	line=$(tail +5 "${crtfile}" | head -1)
	atf_check grep -q "${line}" etc/ssl/cert.pem
}

check_not_in_bundle() {
	local crtfile=$1
	local line

	line=$(tail +5 "${crtfile}" | head -1)
	atf_check -s exit:1 grep -q "${line}" etc/ssl/cert.pem
}

atf_test_case rehash
rehash_head()
{
	atf_set "descr" "Test the rehash command"
}
rehash_body()
{
	certctl_setup
	atf_check certctl rehash

	# Verify non-colliding trusted certificates
	(set1 ; set2) > trusted
	while read crtname hash ; do
		check_trusted "${crtname}"
	done <trusted

	# Verify colliding trusted certificates
	coll >coll
	while read crtname hash ; do
		check_trusted "${crtname}" $(wc -l <coll)
	done <coll

	# Verify untrusted certificates
	set3 >untrusted
	while read crtname hash ; do
		check_untrusted "${crtname}"
	done <untrusted

	# Verify bundle; storeutl is no help here
	for f in etc/ssl/certs/*.? ; do
		check_in_bundle "${f}"
	done
	for f in etc/ssl/untrusted/*.? ; do
		check_not_in_bundle "${f}"
	done
}

atf_test_case trust
trust_head()
{
	atf_set "descr" "Test the trust command"
}
trust_body()
{
	certctl_setup
	atf_check certctl rehash
	crtname=NJWIRLPWAIICVJBKXXHFHLCPAERZATRL
	crtfile=usr/share/certs/untrusted/${crtname}.crt
	check_untrusted ${crtname}
	check_not_in_bundle ${crtfile}
	atf_check -e match:"was previously untrusted" \
	    certctl trust ${crtfile}
	check_trusted ${crtname}
	check_in_bundle ${crtfile}
}

atf_test_case untrust
untrust_head()
{
	atf_set "descr" "Test the untrust command"
}
untrust_body()
{
	certctl_setup
	atf_check certctl rehash
	crtname=AVOYKJHSLFHWPVQMKBHENUAHJTEGMCCB
	crtfile=usr/share/certs/trusted/${crtname}.crt
	check_trusted "${crtname}"
	check_in_bundle ${crtfile}
	atf_check certctl untrust "${crtfile}"
	check_untrusted "${crtname}"
	check_not_in_bundle ${crtfile}
}

atf_init_test_cases()
{
	atf_add_test_case rehash
	atf_add_test_case trust
	atf_add_test_case untrust
}
