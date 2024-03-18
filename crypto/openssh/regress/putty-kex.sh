#	$OpenBSD: putty-kex.sh,v 1.11 2024/02/09 08:56:59 dtucker Exp $
#	Placed in the Public Domain.

tid="putty KEX"

puttysetup

cp ${OBJ}/sshd_proxy ${OBJ}/sshd_proxy_bak

# Enable group1, which PuTTY now disables by default
echo "KEX=dh-group1-sha1" >>${OBJ}/.putty/sessions/localhost_proxy

# Grepping algos out of the binary is pretty janky, but AFAIK there's no way
# to query supported algos.
kex=""
for k in `$SSH -Q kex`; do
	if strings "${PLINK}" | grep -E "^${k}$" >/dev/null; then
		kex="${kex} ${k}"
	else
		trace "omitting unsupported KEX ${k}"
	fi
done

for k in ${kex}; do
	verbose "$tid: kex $k"
	cp ${OBJ}/sshd_proxy_bak ${OBJ}/sshd_proxy
	echo "KexAlgorithms ${k}" >>${OBJ}/sshd_proxy

	env HOME=$PWD ${PLINK} -v -load localhost_proxy -batch -i ${OBJ}/putty.rsa2 true \
	    2>${OBJ}/log/putty-kex-$k.log
	if [ $? -ne 0 ]; then
		fail "KEX $k failed"
	fi
	kexmsg=`grep -E '^Doing.* key exchange' ${OBJ}/log/putty-kex-$k.log`
	trace putty: ${kexmsg}
done
