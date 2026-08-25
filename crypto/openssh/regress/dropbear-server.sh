#	$OpenBSD: dropbear-server.sh,v 1.3 2026/05/27 23:04:36 dtucker Exp $
#	Placed in the Public Domain.

tid="dropbear server"

authkeydir=/var/run/dropbear-regress

if [ -z "$SUDO" -a ! -w /var/run ]; then
	skip "need SUDO to create dir in /var/run, test won't work without it"
fi

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

# Dropbear versions 2026.91 and earlier only support 4 hostkeys in total,
# however this was increased shortly after that release.  Test for this.
$SUDO $DROPBEARCONVERT openssh dropbear "$OBJ/host.ed25519" "$OBJ/db.25519" >/dev/null 2>&1
$SUDO chown $USER $OBJ/$dbkey
k="-r $OBJ/db.ed25519"
if $DROPBEAR $k $k $k $k $k -V >/dev/null 2>&1; then
	limit_4_hostkeys=no
else
	trace "dropbear supports only 4 host keys"
	limit_4_hostkeys=yes
fi

#
# Determine the set of algos supported by the Dropbear we're testing against.
#
if $DROPBEAR -Q help >/dev/null 2>&1; then
	# We can directly query the server for supported algos.
	dbciphers=`$DROPBEAR -Q cipher`
	dbmacs=`$DROPBEAR -Q mac`
	dbkexs=`$DROPBEAR -Q kex`
	dbhkalgs=`$DROPBEAR -Q sig`
	dbpktypes=`$DROPBEAR -Q sig`
else
	# We infer ciphers and macs from dbclient and hard code the rest.
	# Since this test only supports back to Dropbear 2025.07 (due to the
	# need for '-D') we have a pretty good idea what to hard code.
	dbciphers=`$DBCLIENT -c help hst 2>&1 | awk '/ ciphers: /{print $4}' | tr ',' ' '`
	dbmacs=`$DBCLIENT -m help hst 2>&1 | awk '/ MACs: /{print $4}' | tr ',' ' '`
	dbkexs="curve25519-sha256 curve25519-sha256@libssh.org"
	dbkexs="$dbkexs diffie-hellman-group14-sha256"
	dbkexs="$dbkexs ecdh-sha2-nistp256 ecdh-sha2-nistp384 ecdh-sha2-nistp521"
	dbkexs="$dbkexs sntrup761x25519-sha512 mlkem768x25519-sha256"
	dbhkalgs="ssh-ed25519 ecdsa-sha2-nistp256 ecdsa-sha2-nistp521 rsa-sha2-256"
	dbpktypes="ecdsa-sha2-nistp256 ecdsa-sha2-nistp384 ecdsa-sha2-nistp521"
	dbpktypes="$dbpktypes ssh-ed25519 rsa-sha2-256"
fi

if [ -z "$dbmacs" ] || [ -z "$dbciphers" ] || [ -z "$dbkexs" ] || \
    [ -z "$dbhkalgs" ] || [ -z "$dbpktypes" ]; then
	fail "query ciphers '$dbciphers' macs '$dbmacs' kexs '$dbkexs' " \
	   "dbhkalgs '$dbhkalgs' or bpktypes '$bpktypes' failed"
fi

#
# Filter out ciphers, macs and kexes not supported by the OpenSSH we're testing
# and put the ones we want into ciphers, macs and kexes.
#
ciphers=""
for c in $dbciphers; do
	if $SSH -Q Ciphers | grep -E "^$c\$" >/dev/null; then
		ciphers="$ciphers $c"
	else
		trace "ssh does not support cipher '$c'"
	fi
done

macs=""
for m in $dbmacs; do
	if $SSH -Q MACs | grep -E "^$m\$" >/dev/null; then
		macs="$macs $m"
	else
		trace "ssh does not support mac '$m'"
	fi
done

kexs=""
for k in $dbkexs; do
	if $SSH -Q KexAlgorithms | grep -E "^$k\$" >/dev/null; then
		kexs="$kexs $k"
	else
		trace "ssh does not support kex '$k'"
	fi
done

#
# Now filter by supported HostKeyAlgorithms.  The key types are not a 1:1
# correlation with the algos, so we first check that the algo is supported,
# and if so put it in hkalgs add the appropriate key type to keytypes for
# later deduplication and processing.
#
hkalgs=""
keytypes=""
for alg in $dbhkalgs; do
	if ! $SSH -Q HostKeyAlgorithms | grep -E "^$alg\$" >/dev/null; then
		trace "ssh does not support $alg"
		alg=""
	fi

	kt="$alg"
	case "$alg" in
	sk-*)
		trace "omitting sk alg $alg"
		alg=""
		;;
	ecdsa-sha2-nistp384)
		if [ "$limit_4_hostkeys" = "yes" ]; then
			trace "dropbear host key limit=4, omitting $alg"
			alg=""
		fi
		;;
	rsa-sha2*)
		kt=ssh-rsa
		;;
	esac

	if [ "$alg" != "" ]; then
		hkalgs="$hkalgs $alg"
		keytypes="$keytypes $kt"
	fi
done

#
# Deduplicate key types (because the various RSA hostkey algos use the same
# type and Dropbear has a limit on the number of hostkeys it'll load) and
# construct hkeyopts to be passed to dropbear command line.
#
hkeyopts=""
for kt in `for i in $keytypes; do echo $i; done | sort -u`; do
	key="host.$kt"
	dbkey="db.$kt"
	trace "convert hostkey '$key' to '$dbkey'"
	if $SUDO $DROPBEARCONVERT openssh dropbear "$OBJ/$key" \
	     "$OBJ/$dbkey" >/dev/null 2>&1; then
		if [ ! -f "$OBJ/$dbkey" ]; then
			fail "convert $key to $dbkey"
		fi
		$SUDO chown $USER $OBJ/$dbkey
	fi
	trace "hkeyopts add -r $OBJ/db.$kt"
	hkeyopts="$hkeyopts -r $OBJ/db.$kt"
done

pktypes=""
for pk in $dbpktypes; do
	if $SSH -Q PubkeyAcceptedAlgorithms | grep -E "^$pk\$" >/dev/null; then
		case "$pk" in
		sk-*)	;;
		*)	pktypes="$pktypes $pk" ;;
		esac
	else
		trace "ssh does not support pubkey type '$pk'"
	fi
done

# Set up authorized_keys for dropbear.
umask 077
$SUDO mkdir -p $authkeydir
$SUDO chown -R $USER $authkeydir
cp $OBJ/authorized_keys_$USER $authkeydir/authorized_keys

rm -f $OBJ/dropbear.pid
$DROPBEAR -E -D $authkeydir -p $PORT -P $OBJ/dropbear.pid $hkeyopts 2>>$OBJ/sshd.log
if [ $? -ne 0 ]; then
	fatal "starting dropbear server failed"
fi
while [ ! -f $OBJ/dropbear.pid ]; do
	sleep 1
done

pid=`cat $OBJ/dropbear.pid`
trap "kill $pid; $SUDO rm -rf $authkeydir" 0

trace ciphers $ciphers
trace macs $macs
trace kexs $kexs
trace hkalgs $hkalgs
trace pktypes $pktypes

for c in $ciphers; do
  case "$c" in
  chacha20-poly1305@openssh.com|aes*-gcm@openssh.com)
    tmpmacs="<implicit>" ;;
  *)
    tmpmacs="$macs" ;;
  esac

  for m in $tmpmacs; do
   for k in $kexs; do
    for hk in $hkalgs; do
     for pk in $pktypes; do
	verbose "$tid: cipher $c mac $m kex $k hkalg $hk pk $pk"
	rm -f ${COPY}
	if [ "$m" = "<implicit>" ]; then
		macopts=""
	else
		macopts="-oMacs=$m"
	fi
	${SSH} -F $OBJ/ssh_config -oCiphers=$c $macopts -oKexAlgorithms=$k \
	    -oHostKeyAlgorithms=$hk -oPubkeyAcceptedAlgorithms=$pk \
	    somehost cat ${DATA} > ${COPY}
	if [ $? -ne 0 ]; then
		fail "connect dropbear server failed"
	fi
	cmp ${DATA} ${COPY}		|| fail "corrupted copy"
     done
    done
   done
  done
done
