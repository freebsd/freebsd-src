#	$OpenBSD: dropbear-ciphers.sh,v 1.3 2024/06/20 08:23:18 dtucker Exp $
#	Placed in the Public Domain.

tid="dropbear ciphers"

if test "x$REGRESS_INTEROP_DROPBEAR" != "xyes" ; then
	skip "dropbear interop tests not enabled"
fi

# Enable all support algorithms
algs=`$SSH -Q key-sig | tr '\n' ,`
cat >>$OBJ/sshd_proxy <<EOD
PubkeyAcceptedAlgorithms $algs
HostkeyAlgorithms $algs
EOD

ciphers=`$DBCLIENT -c help hst 2>&1 | awk '/ ciphers: /{print $4}' | tr ',' ' '`
macs=`$DBCLIENT -m help hst 2>&1 | awk '/ MACs: /{print $4}' | tr ',' ' '`
if [ -z "$macs" ] || [ -z "$ciphers" ]; then
	skip "dbclient query ciphers '$ciphers' or macs '$macs' failed"
fi
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
