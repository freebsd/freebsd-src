#	$OpenBSD: dropbear-kex.sh,v 1.1 2023/10/20 06:56:45 dtucker Exp $
#	Placed in the Public Domain.

tid="dropbear kex"

if test "x$REGRESS_INTEROP_DROPBEAR" != "xyes" ; then
	skip "dropbear interop tests not enabled"
fi

cat >>$OBJ/sshd_proxy <<EOD
PubkeyAcceptedAlgorithms +ssh-rsa,ssh-dss
HostkeyAlgorithms +ssh-rsa,ssh-dss
EOD
cp $OBJ/sshd_proxy $OBJ/sshd_proxy.bak

kex="curve25519-sha256 curve25519-sha256@libssh.org
    diffie-hellman-group14-sha256 diffie-hellman-group14-sha1"

for k in $kex; do
	verbose "$tid: kex $k"
	rm -f ${COPY}
	# dbclient doesn't have switch for kex, so force in server
	(cat $OBJ/sshd_proxy.bak; echo "KexAlgorithms $k") >$OBJ/sshd_proxy
	env HOME=$OBJ dbclient -y -i $OBJ/.dropbear/id_rsa 2>$OBJ/dbclient.log \
	    -J "$OBJ/ssh_proxy.sh" somehost cat ${DATA} > ${COPY}
	if [ $? -ne 0 ]; then
		fail "ssh cat $DATA failed"
	fi
	cmp ${DATA} ${COPY}		|| fail "corrupted copy"
done
rm -f ${COPY}
