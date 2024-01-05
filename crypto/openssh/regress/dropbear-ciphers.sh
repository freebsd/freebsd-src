#	$OpenBSD: dropbear-ciphers.sh,v 1.1 2023/10/20 06:56:45 dtucker Exp $
#	Placed in the Public Domain.

tid="dropbear ciphers"

if test "x$REGRESS_INTEROP_DROPBEAR" != "xyes" ; then
	skip "dropbear interop tests not enabled"
fi

cat >>$OBJ/sshd_proxy <<EOD
PubkeyAcceptedAlgorithms +ssh-rsa,ssh-dss
HostkeyAlgorithms +ssh-rsa,ssh-dss
EOD

ciphers=`$DBCLIENT -c help 2>&1 | awk '/ ciphers: /{print $4}' | tr ',' ' '`
macs=`$DBCLIENT -m help 2>&1 | awk '/ MACs: /{print $4}' | tr ',' ' '`
keytype=`(cd $OBJ/.dropbear && ls id_*)`

for c in $ciphers ; do
  for m in $macs; do
    for kt in $keytype; do
	verbose "$tid: cipher $c mac $m kt $kt"
	rm -f ${COPY}
	env HOME=$OBJ dbclient -y -i $OBJ/.dropbear/$kt 2>$OBJ/dbclient.log \
	    -c $c -m $m -J "$OBJ/ssh_proxy.sh" somehost cat ${DATA} > ${COPY}
	if [ $? -ne 0 ]; then
		fail "ssh cat $DATA failed"
	fi
	cmp ${DATA} ${COPY}		|| fail "corrupted copy"
    done
  done
done
rm -f ${COPY}
