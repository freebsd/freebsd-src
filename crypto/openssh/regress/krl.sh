#	$OpenBSD: krl.sh,v 1.11 2019/12/16 02:39:05 djm Exp $
#	Placed in the Public Domain.

tid="key revocation lists"

# Use ed25519 by default since it's fast and it's supported when building
# w/out OpenSSL.  Populate ktype[2-4] with the other types if supported.
ktype1=ed25519; ktype2=ed25519; ktype3=ed25519;
ktype4=ed25519; ktype5=ed25519; ktype6=ed25519;
for t in $SSH_KEYTYPES; do
	case "$t" in
		ecdsa*)		ktype2=ecdsa ;;
		ssh-rsa)	ktype3=rsa ;;
		ssh-dss)	ktype4=dsa ;;
		sk-ssh-ed25519@openssh.com)		ktype5=ed25519-sk ;;
		sk-ecdsa-sha2-nistp256@openssh.com)	ktype6=ecdsa-sk ;;
	esac
done

# Do most testing with ssh-keygen; it uses the same verification code as sshd.

# Old keys will interfere with ssh-keygen.
rm -f $OBJ/revoked-* $OBJ/krl-*

# Generate a CA key
$SSHKEYGEN -t $ktype1 -f $OBJ/revoked-ca  -C "" -N "" > /dev/null ||
	fatal "$SSHKEYGEN CA failed"
$SSHKEYGEN -t $ktype2 -f $OBJ/revoked-ca2  -C "" -N "" > /dev/null ||
	fatal "$SSHKEYGEN CA2 failed"

# A specification that revokes some certificates by serial numbers
# The serial pattern is chosen to ensure the KRL includes list, range and
# bitmap sections.
cat << EOF >> $OBJ/revoked-serials
serial: 1-4
serial: 10
serial: 15
serial: 30
serial: 50
serial: 90
serial: 999
# The following sum to 500-799
serial: 500
serial: 501
serial: 502
serial: 503-600
serial: 700-797
serial: 798
serial: 799
serial: 599-701
# Some multiple consecutive serial number ranges
serial: 10000-20000
serial: 30000-40000
EOF

# A specification that revokes some certificated by key ID.
touch $OBJ/revoked-keyid
for n in 1 2 3 4 10 15 30 50 90 `jot 500 300` 999 1000 1001 1002; do
	test "x$n" = "x499" && continue
	# Fill in by-ID revocation spec.
	echo "id: revoked $n" >> $OBJ/revoked-keyid
done

keygen() {
	N=$1
	f=$OBJ/revoked-`printf "%04d" $N`
	# Vary the keytype. We use mostly ed25519 since this is fast and well
	# supported.
	keytype=$ktype1
	case $N in
	2  | 10 | 510 | 1001)	keytype=$ktype2 ;;
	4  | 30 | 520 | 1002)	keytype=$ktype3 ;;
	8  | 50 | 530 | 1003)	keytype=$ktype4 ;;
	16 | 70 | 540 | 1004)	keytype=$ktype5 ;;
	32 | 90 | 550 | 1005)	keytype=$ktype6 ;;
	esac
	$SSHKEYGEN -t $keytype -f $f -C "" -N "" > /dev/null \
		|| fatal "$SSHKEYGEN failed"
	# Sign cert
	$SSHKEYGEN -s $OBJ/revoked-ca -z $n -I "revoked $N" $f >/dev/null 2>&1 \
		|| fatal "$SSHKEYGEN sign failed"
	echo $f
}

# Generate some keys.
verbose "$tid: generating test keys"
REVOKED_SERIALS="1 4 10 50 90 500 510 520 550 799 999"
for n in $REVOKED_SERIALS ; do
	f=`keygen $n`
	RKEYS="$RKEYS ${f}.pub"
	RCERTS="$RCERTS ${f}-cert.pub"
done
UNREVOKED_SERIALS="5 9 14 16 29 49 51 499 800 1010 1011"
UNREVOKED=""
for n in $UNREVOKED_SERIALS ; do
	f=`keygen $n`
	UKEYS="$UKEYS ${f}.pub"
	UCERTS="$UCERTS ${f}-cert.pub"
done

# Specifications that revoke keys by hash.
touch $OBJ/revoked-sha1 $OBJ/revoked-sha256 $OBJ/revoked-hash
for rkey in $RKEYS; do
	(printf "sha1: "; cat $rkey) >> $OBJ/revoked-sha1
	(printf "sha256: "; cat $rkey) >> $OBJ/revoked-sha256
	(printf "hash: "; $SSHKEYGEN -lf $rkey | \
		awk '{ print $2 }') >> $OBJ/revoked-hash
done

genkrls() {
	OPTS=$1
$SSHKEYGEN $OPTS -kf $OBJ/krl-empty - </dev/null \
	>/dev/null || fatal "$SSHKEYGEN KRL failed"
$SSHKEYGEN $OPTS -kf $OBJ/krl-keys $RKEYS \
	>/dev/null || fatal "$SSHKEYGEN KRL failed"
$SSHKEYGEN $OPTS -kf $OBJ/krl-cert $RCERTS \
	>/dev/null || fatal "$SSHKEYGEN KRL failed"
$SSHKEYGEN $OPTS -kf $OBJ/krl-all $RKEYS $RCERTS \
	>/dev/null || fatal "$SSHKEYGEN KRL failed"
$SSHKEYGEN $OPTS -kf $OBJ/krl-ca $OBJ/revoked-ca.pub \
	>/dev/null || fatal "$SSHKEYGEN KRL failed"
$SSHKEYGEN $OPTS -kf $OBJ/krl-sha1 $OBJ/revoked-sha1 \
	>/dev/null 2>&1 || fatal "$SSHKEYGEN KRL failed"
$SSHKEYGEN $OPTS -kf $OBJ/krl-sha256 $OBJ/revoked-sha256 \
	>/dev/null 2>&1 || fatal "$SSHKEYGEN KRL failed"
$SSHKEYGEN $OPTS -kf $OBJ/krl-hash $OBJ/revoked-hash \
	>/dev/null 2>&1 || fatal "$SSHKEYGEN KRL failed"
# This should fail as KRLs from serial/key-id spec need the CA specified.
$SSHKEYGEN $OPTS -kf $OBJ/krl-serial $OBJ/revoked-serials \
	>/dev/null 2>&1 && fatal "$SSHKEYGEN KRL succeeded unexpectedly"
$SSHKEYGEN $OPTS -kf $OBJ/krl-keyid $OBJ/revoked-keyid \
	>/dev/null 2>&1 && fatal "$SSHKEYGEN KRL succeeded unexpectedly"
# These should succeed; they specify an explicit CA key.
$SSHKEYGEN $OPTS -kf $OBJ/krl-serial -s $OBJ/revoked-ca \
	$OBJ/revoked-serials >/dev/null || fatal "$SSHKEYGEN KRL failed"
$SSHKEYGEN $OPTS -kf $OBJ/krl-keyid -s $OBJ/revoked-ca.pub \
	$OBJ/revoked-keyid >/dev/null || fatal "$SSHKEYGEN KRL failed"
# These should succeed; they specify an wildcard CA key.
$SSHKEYGEN $OPTS -kf $OBJ/krl-serial-wild -s NONE $OBJ/revoked-serials \
	>/dev/null || fatal "$SSHKEYGEN KRL failed"
$SSHKEYGEN $OPTS -kf $OBJ/krl-keyid-wild -s NONE $OBJ/revoked-keyid \
	>/dev/null || fatal "$SSHKEYGEN KRL failed"
# Revoke the same serials with the second CA key to ensure a multi-CA
# KRL is generated.
$SSHKEYGEN $OPTS -kf $OBJ/krl-serial -u -s $OBJ/revoked-ca2 \
	$OBJ/revoked-serials >/dev/null || fatal "$SSHKEYGEN KRL failed"
}

## XXX dump with trace and grep for set cert serials
## XXX test ranges near (u64)-1, etc.

verbose "$tid: generating KRLs"
genkrls

check_krl() {
	KEY=$1
	KRL=$2
	EXPECT_REVOKED=$3
	TAG=$4
	$SSHKEYGEN -Qf $KRL $KEY >/dev/null
	result=$?
	if test "x$EXPECT_REVOKED" = "xy" -a $result -eq 0 ; then
		fatal "key $KEY not revoked by KRL $KRL: $TAG"
	elif test "x$EXPECT_REVOKED" = "xn" -a $result -ne 0 ; then
		fatal "key $KEY unexpectedly revoked by KRL $KRL: $TAG"
	fi
}
test_rev() {
	FILES=$1
	TAG=$2
	KEYS_RESULT=$3
	ALL_RESULT=$4
	HASH_RESULT=$5
	SERIAL_RESULT=$6
	KEYID_RESULT=$7
	CERTS_RESULT=$8
	CA_RESULT=$9
	SERIAL_WRESULT=$10
	KEYID_WRESULT=$11
	verbose "$tid: checking revocations for $TAG"
	for f in $FILES ; do
		check_krl $f $OBJ/krl-empty		no		"$TAG"
		check_krl $f $OBJ/krl-keys		$KEYS_RESULT	"$TAG"
		check_krl $f $OBJ/krl-all		$ALL_RESULT	"$TAG"
		check_krl $f $OBJ/krl-sha1		$HASH_RESULT	"$TAG"
		check_krl $f $OBJ/krl-sha256		$HASH_RESULT	"$TAG"
		check_krl $f $OBJ/krl-hash		$HASH_RESULT	"$TAG"
		check_krl $f $OBJ/krl-serial		$SERIAL_RESULT	"$TAG"
		check_krl $f $OBJ/krl-keyid		$KEYID_RESULT	"$TAG"
		check_krl $f $OBJ/krl-cert		$CERTS_RESULT	"$TAG"
		check_krl $f $OBJ/krl-ca		$CA_RESULT	"$TAG"
		check_krl $f $OBJ/krl-serial-wild	$SERIAL_WRESULT	"$TAG"
		check_krl $f $OBJ/krl-keyid-wild	$KEYID_WRESULT	"$TAG"
	done
}

test_all() {
	#                                                           wildcard
	#                                 keys all hash sr# ID cert  CA srl ID
	test_rev "$RKEYS"     "revoked keys" y   y    y   n  n    n   n   n  n
	test_rev "$UKEYS"   "unrevoked keys" n   n    n   n  n    n   n   n  n
	test_rev "$RCERTS"   "revoked certs" y   y    y   y  y    y   y   y  y
	test_rev "$UCERTS" "unrevoked certs" n   n    n   n  n    n   y   n  n
}

test_all

# Check update. Results should be identical.
verbose "$tid: testing KRL update"
for f in $OBJ/krl-keys $OBJ/krl-cert $OBJ/krl-all \
    $OBJ/krl-ca $OBJ/krl-serial $OBJ/krl-keyid \
    $OBJ/krl-serial-wild $OBJ/krl-keyid-wild; do
	cp -f $OBJ/krl-empty $f
	genkrls -u
done

test_all
