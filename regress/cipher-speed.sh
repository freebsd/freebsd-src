#	$OpenBSD: cipher-speed.sh,v 1.13 2015/03/24 20:22:17 markus Exp $
#	Placed in the Public Domain.

tid="cipher speed"

getbytes ()
{
	sed -n -e '/transferred/s/.*secs (\(.* bytes.sec\).*/\1/p' \
	    -e '/copied/s/.*s, \(.* MB.s\).*/\1/p'
}

tries="1 2"

for c in `${SSH} -Q cipher`; do n=0; for m in `${SSH} -Q mac`; do
	trace "proto 2 cipher $c mac $m"
	for x in $tries; do
		printf "%-60s" "$c/$m:"
		( ${SSH} -o 'compression no' \
			-F $OBJ/ssh_proxy -2 -m $m -c $c somehost \
			exec sh -c \'"dd of=/dev/null obs=32k"\' \
		< ${DATA} ) 2>&1 | getbytes

		if [ $? -ne 0 ]; then
			fail "ssh -2 failed with mac $m cipher $c"
		fi
	done
	# No point trying all MACs for AEAD ciphers since they are ignored.
	if ${SSH} -Q cipher-auth | grep "^${c}\$" >/dev/null 2>&1 ; then
		break
	fi
	n=`expr $n + 1`
done; done

if ssh_version 1; then
	ciphers="3des blowfish"
else
	ciphers=""
fi
for c in $ciphers; do
	trace "proto 1 cipher $c"
	for x in $tries; do
		printf "%-60s" "$c:"
		( ${SSH} -o 'compression no' \
			-F $OBJ/ssh_proxy -1 -c $c somehost \
			exec sh -c \'"dd of=/dev/null obs=32k"\' \
		< ${DATA} ) 2>&1 | getbytes
		if [ $? -ne 0 ]; then
			fail "ssh -1 failed with cipher $c"
		fi
	done
done
