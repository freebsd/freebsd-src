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

sortfile() {
	for filename; do
		sort "${filename}" >"${filename}"-
		mv "${filename}"- "${filename}"
	done
}

certctl_setup()
{
	export DESTDIR="$PWD"

	# Create input directories
	mkdir -p ${DESTDIR}${DISTBASE}/usr/share/certs/trusted
	mkdir -p ${DESTDIR}${DISTBASE}/usr/share/certs/untrusted
	mkdir -p ${DESTDIR}/usr/local/share/certs

	# Do not create output directories; certctl will take care of it
	#mkdir -p ${DESTDIR}${DISTBASE}/etc/ssl/certs
	#mkdir -p ${DESTDIR}${DISTBASE}/etc/ssl/untrusted

	# Generate a random key
	keyname="testkey"
	gen_key ${keyname}

	# Generate certificates
	:>metalog.expect
	:>trusted.expect
	:>untrusted.expect
	metalog() {
		echo ".${DISTBASE}$@ type=file" >>metalog.expect
	}
	trusted() {
		local crtname=$1
		local filename=$2
		printf "%s\t%s\n" "${filename}" "${crtname}" >>trusted.expect
		metalog "/etc/ssl/certs/${filename}"
	}
	untrusted() {
		local crtname=$1
		local filename=$2
		printf "%s\t%s\n" "${filename}" "${crtname}" >>untrusted.expect
		metalog "/etc/ssl/untrusted/${filename}"
	}
	set1 | while read crtname hash ; do
		gen_crt ${crtname} ${keyname}
		mv ${crtname}.crt ${DESTDIR}${DISTBASE}/usr/share/certs/trusted
		trusted "${crtname}" "${hash}.0"
	done
	local c=0
	coll | while read crtname hash ; do
		gen_crt ${crtname} ${keyname}
		mv ${crtname}.crt ${DESTDIR}${DISTBASE}/usr/share/certs/trusted
		trusted "${crtname}" "${hash}.${c}"
		c=$((c+1))
	done
	set2 | while read crtname hash ; do
		gen_crt ${crtname} ${keyname}
		openssl x509 -in ${crtname}.crt
		rm ${crtname}.crt
		trusted "${crtname}" "${hash}.0"
	done >usr/local/share/certs/bundle.crt
	set3 | while read crtname hash ; do
		gen_crt ${crtname} ${keyname}
		mv ${crtname}.crt ${DESTDIR}${DISTBASE}/usr/share/certs/untrusted
		untrusted "${crtname}" "${hash}.0"
	done
	metalog "/etc/ssl/cert.pem"
	unset -f untrusted
	unset -f trusted
	unset -f metalog
	sortfile *.expect
}

check_trusted() {
	local crtname=$1
	local subject="$(subject ${crtname})"
	local c=${2:-1}

	atf_check -e ignore -o match:"found: ${c}\$" \
	    openssl storeutl -noout -subject "${subject}" \
	    ${DESTDIR}${DISTBASE}/etc/ssl/certs
	atf_check -e ignore -o not-match:"found: [1-9]"  \
	    openssl storeutl -noout -subject "${subject}" \
	    ${DESTDIR}${DISTBASE}/etc/ssl/untrusted
}

check_untrusted() {
	local crtname=$1
	local subject="$(subject ${crtname})"
	local c=${2:-1}

	atf_check -e ignore -o not-match:"found: [1-9]" \
	    openssl storeutl -noout -subject "${subject}" \
	    ${DESTDIR}/${DISTBASE}/etc/ssl/certs
	atf_check -e ignore -o match:"found: ${c}\$" \
	    openssl storeutl -noout -subject "${subject}" \
	    ${DESTDIR}/${DISTBASE}/etc/ssl/untrusted
}

check_in_bundle() {
	local b=${DISTBASE}${DISTBASE+/}
	local crtfile=$1
	local line

	line=$(tail +5 "${crtfile}" | head -1)
	atf_check grep -q "${line}" ${DESTDIR}${DISTBASE}/etc/ssl/cert.pem
}

check_not_in_bundle() {
	local b=${DISTBASE}${DISTBASE+/}
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
	(set1; set2) >trusted
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

	# Verify bundle
	for f in etc/ssl/certs/*.? ; do
		check_in_bundle "${f}"
	done
	for f in etc/ssl/untrusted/*.? ; do
		check_not_in_bundle "${f}"
	done
}

atf_test_case list
list_head()
{
	atf_set "descr" "Test the list and untrusted commands"
}
list_body()
{
	certctl_setup
	atf_check certctl rehash

	atf_check -o save:trusted.out certctl list
	sortfile trusted.out
	# the ordering of the colliding certificates is partly
	# determined by fields that change every time we regenerate
	# them, so ignore them in the diff
	atf_check diff -u \
	    --ignore-matching-lines $collhash \
	    trusted.expect trusted.out

	atf_check -o save:untrusted.out certctl untrusted
	sortfile untrusted.out
	atf_check diff -u \
	    untrusted.expect untrusted.out
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
	crtname=$(set3 | (read crtname hash ; echo ${crtname}))
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
	crtname=$(set1 | (read crtname hash ; echo ${crtname}))
	crtfile=usr/share/certs/trusted/${crtname}.crt
	check_trusted "${crtname}"
	check_in_bundle ${crtfile}
	atf_check certctl untrust "${crtfile}"
	check_untrusted "${crtname}"
	check_not_in_bundle ${crtfile}
}

atf_test_case metalog
metalog_head()
{
	atf_set "descr" "Verify the metalog"
}
metalog_body()
{
	export DISTBASE=/base
	certctl_setup

	# certctl gets DESTDIR and DISTBASE from environment
	rm -f metalog.orig
	atf_check certctl -U -M metalog.orig rehash
	sed -E 's/(type=file) .*/\1/' metalog.orig | sort >metalog.short
	atf_check diff -u metalog.expect metalog.short

	# certctl gets DESTDIR and DISTBASE from command line
	rm -f metalog.orig
	atf_check env -uDESTDIR -uDISTBASE \
	    certctl -D ${DESTDIR} -d ${DISTBASE} -U -M metalog.orig rehash
	sed -E 's/(type=file) .*/\1/' metalog.orig | sort >metalog.short
	atf_check diff -u metalog.expect metalog.short

	# as above, but intentionally add trailing slashes
	rm -f metalog.orig
	atf_check env -uDESTDIR -uDISTBASE \
	    certctl -D ${DESTDIR}// -d ${DISTBASE}/ -U -M metalog.orig rehash
	sed -E 's/(type=file) .*/\1/' metalog.orig | sort >metalog.short
	atf_check diff -u metalog.expect metalog.short
}

atf_test_case misc
misc_head()
{
	atf_set "descr" "Test miscellaneous edge cases"
}
misc_body()
{
	# certctl rejects DISTBASE that does not begin with a slash
	atf_check -s exit:1 -e match:"begin with a slash" \
	    certctl -d base -n rehash
	atf_check -s exit:1 -e match:"begin with a slash" \
	    env DISTBASE=base certctl -n rehash
}

atf_init_test_cases()
{
	atf_add_test_case rehash
	atf_add_test_case list
	atf_add_test_case trust
	atf_add_test_case untrust
	atf_add_test_case metalog
	atf_add_test_case misc
}
