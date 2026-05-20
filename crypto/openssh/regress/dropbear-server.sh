#	$OpenBSD: dropbear-server.sh,v 1.2 2025/06/29 05:35:00 dtucker Exp $
#	Placed in the Public Domain.

tid="dropbear server"

if test "x$REGRESS_INTEROP_DROPBEAR" != "xyes" ; then
	skip "dropbear interop tests not enabled"
fi

ver="`$DROPBEAR -V 2>&1 | sed 's/Dropbear v//'`"
if [ -z "$ver" ]; then
	skip "can't determine dropbear version"
fi

major=`echo $ver | cut -f1 -d.`
minor=`echo $ver | cut -f2 -d.`

if [ "$major" -lt "2025" ] || [ "$minor" -lt "87" ]; then
	skip "dropbear version $ver (${major}.${minor}) does not support '-D'"
else
	trace "dropbear version $ver (${major}.${minor}) ok"
fi

if [ -z "$SUDO" -a ! -w /var/run ]; then
	skip "need SUDO to create dir in /var/run, test won't work without"
fi
authkeydir=/var/run/dropbear-regress

ciphers=`$DBCLIENT -c help hst 2>&1 | awk '/ ciphers: /{print $4}' | tr ',' ' '`
macs=`$DBCLIENT -m help hst 2>&1 | awk '/ MACs: /{print $4}' | tr ',' ' '`
if [ -z "$macs" ] || [ -z "$ciphers" ]; then
	skip "dbclient query ciphers '$ciphers' or macs '$macs' failed"
fi

# Set up authorized_keys for dropbear.
umask 077
$SUDO mkdir -p $authkeydir
$SUDO chown -R $USER $authkeydir
cp $OBJ/authorized_keys_$USER $authkeydir/authorized_keys

for i in `$SUDO $SSHD -f $OBJ/sshd_config -T | grep -v sk- | \
    awk '$1=="hostkey" {print $2}'`; do
	file=`basename "$i"`
	file=`echo "$file" | sed s/^host\./db\./g`
	if $SUDO $DROPBEARCONVERT openssh dropbear "$i" "$OBJ/$file" \
	    >/dev/null 2>&1; then
		$SUDO chown $USER $OBJ/$file
		hkeys="-r $OBJ/$file"
	fi
done

rm -f $OBJ/dropbear.pid
$DROPBEAR -D $authkeydir -p $PORT -P $OBJ/dropbear.pid $hkeys -E \
    2>$OBJ/sshd.log
if [ $? -ne 0 ]; then
	fatal "starting dropbear server failed"
fi
while [ ! -f $OBJ/dropbear.pid ]; do
	sleep 1
done

pid=`cat $OBJ/dropbear.pid`
trap "kill $pid; $SUDO rm -rf $authkeydir" 0

for c in $ciphers; do
  for m in $macs; do
	trace "$tid: cipher $c mac $m hk $hk"
	rm -f ${COPY}
	${SSH} -F $OBJ/ssh_config -oCiphers=$c -oMacs=$m \
	   somehost cat ${DATA} > ${COPY}
	if [ $? -ne 0 ]; then
		fail "connect dropbear server failed"
	fi
	cmp ${DATA} ${COPY}		|| fail "corrupted copy"
  done
done
