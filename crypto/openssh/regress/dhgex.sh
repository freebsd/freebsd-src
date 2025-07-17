#	$OpenBSD: dhgex.sh,v 1.8 2023/03/02 08:14:52 dtucker Exp $
#	Placed in the Public Domain.

tid="dhgex"

LOG=${TEST_SSH_LOGFILE}
rm -f ${LOG}
cp $OBJ/sshd_proxy $OBJ/sshd_proxy_bak

kexs=`${SSH} -Q kex | grep diffie-hellman-group-exchange`

ssh_test_dhgex()
{
	bits="$1"; shift
	cipher="$1"; shift
	kex="$1"; shift

	cp $OBJ/sshd_proxy_bak $OBJ/sshd_proxy
	echo "KexAlgorithms=$kex" >> $OBJ/sshd_proxy
	echo "Ciphers=$cipher" >> $OBJ/sshd_proxy
	rm -f ${LOG}
	opts="-oKexAlgorithms=$kex -oCiphers=$cipher"
	min=2048
	max=8192
	groupsz="$min<$bits<$max"
	verbose "$tid bits $bits $kex $cipher"
	${SSH} ${opts} $@ -vvv -F ${OBJ}/ssh_proxy somehost true
	if [ $? -ne 0 ]; then
		fail "ssh failed ($@)"
	fi
	# check what we request
	grep "SSH2_MSG_KEX_DH_GEX_REQUEST($groupsz) sent" ${LOG} >/dev/null
	if [ $? != 0 ]; then
		got="`egrep 'SSH2_MSG_KEX_DH_GEX_REQUEST(.*) sent' ${LOG}`"
		fail "$tid unexpected GEX sizes, expected $groupsz, got '$got'"
	fi
	# check what we got.
	gotbits="`awk 'BEGIN{FS="/"}/bits set:/{print $2}' ${LOG} |
	    head -1 | tr -d '\r\n'`"
	trace "expected '$bits' got '$gotbits'"
	if [ -z "$gotbits" ] || [ "$gotbits" -lt "$bits" ]; then
		fatal "$tid expected $bits bit group, got $gotbits"
	fi
}

check()
{
	bits="$1"; shift

	for c in $@; do
		for k in $kexs; do
			ssh_test_dhgex $bits $c $k
		done
	done
}

check 3072 3des-cbc  # 112 bits.
check 3072 `${SSH} -Q cipher | grep 128`
check 7680 `${SSH} -Q cipher | grep 192`
check 8192 `${SSH} -Q cipher | grep 256`
check 8192 chacha20-poly1305@openssh.com
