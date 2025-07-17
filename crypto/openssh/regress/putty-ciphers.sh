#	$OpenBSD: putty-ciphers.sh,v 1.13 2024/02/09 08:56:59 dtucker Exp $
#	Placed in the Public Domain.

tid="putty ciphers"

puttysetup

cp ${OBJ}/sshd_proxy ${OBJ}/sshd_proxy_bak

# Since there doesn't seem to be a way to set MACs on the PuTTY client side,
# we force each in turn on the server side, omitting the ones PuTTY doesn't
# support.  Grepping the binary is pretty janky, but AFAIK there's no way to
# query for supported algos.
macs=""
for m in `${SSH} -Q MACs`; do
	if strings "${PLINK}" | grep -E "^${m}$" >/dev/null; then
		macs="${macs} ${m}"
	else
		trace "omitting unsupported MAC ${m}"
	fi
done

ciphers=""
for c in `${SSH} -Q Ciphers`; do
	if strings "${PLINK}" | grep -E "^${c}$" >/dev/null; then
		ciphers="${ciphers} ${c}"
	else
		trace "omitting unsupported cipher ${c}"
	fi
done

for c in default $ciphers; do
    for m in default ${macs}; do
	verbose "$tid: cipher $c mac $m"
	cp ${OBJ}/.putty/sessions/localhost_proxy \
	    ${OBJ}/.putty/sessions/cipher_$c
	if [ "${c}" != "default" ]; then
		echo "Cipher=$c" >> ${OBJ}/.putty/sessions/cipher_$c
	fi

	cp ${OBJ}/sshd_proxy_bak ${OBJ}/sshd_proxy
	if [ "${m}" != "default" ]; then
		echo "MACs $m" >> ${OBJ}/sshd_proxy
	fi

	rm -f ${COPY}
	env HOME=$PWD ${PLINK} -load cipher_$c -batch -i ${OBJ}/putty.rsa2 \
	    cat ${DATA} > ${COPY}
	if [ $? -ne 0 ]; then
		fail "ssh cat $DATA failed"
	fi
	cmp ${DATA} ${COPY}		|| fail "corrupted copy"
    done
done
rm -f ${COPY}
