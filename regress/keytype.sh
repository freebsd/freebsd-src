#	$OpenBSD: keytype.sh,v 1.1 2010/09/02 16:12:55 markus Exp $
#	Placed in the Public Domain.

tid="login with different key types"

TIME=`which time` 2>/dev/null
if test ! -x "$TIME"; then
	TIME=""
fi

cp $OBJ/sshd_proxy $OBJ/sshd_proxy_bak
cp $OBJ/ssh_proxy $OBJ/ssh_proxy_bak

ktypes="dsa-1024 rsa-2048 rsa-3072"
if test "$TEST_SSH_ECC" = "yes"; then
	ktypes="$ktypes ecdsa-256 ecdsa-384 ecdsa-521"
fi

for kt in $ktypes; do 
	rm -f $OBJ/key.$kt
	bits=`echo ${kt} | awk -F- '{print $2}'`
	type=`echo ${kt}  | awk -F- '{print $1}'`
	printf "keygen $type, $bits bits:\t"
	${TIME} ${SSHKEYGEN} -b $bits -q -N '' -t $type  -f $OBJ/key.$kt ||\
		fail "ssh-keygen for type $type, $bits bits failed"
done

tries="1 2 3"
for ut in $ktypes; do 
	htypes=$ut
	#htypes=$ktypes
	for ht in $htypes; do 
		trace "ssh connect, userkey $ut, hostkey $ht"
		(
			grep -v HostKey $OBJ/sshd_proxy_bak
			echo HostKey $OBJ/key.$ht 
		) > $OBJ/sshd_proxy
		(
			grep -v IdentityFile $OBJ/ssh_proxy_bak
			echo IdentityFile $OBJ/key.$ut 
		) > $OBJ/ssh_proxy
		(
			echon 'localhost-with-alias,127.0.0.1,::1 '
			cat $OBJ/key.$ht.pub
		) > $OBJ/known_hosts
		cat $OBJ/key.$ut.pub > $OBJ/authorized_keys_$USER
		for i in $tries; do
			printf "userkey $ut, hostkey ${ht}:\t"
			${TIME} ${SSH} -F $OBJ/ssh_proxy 999.999.999.999 true
			if [ $? -ne 0 ]; then
				fail "ssh userkey $ut, hostkey $ht failed"
			fi
		done
	done
done
