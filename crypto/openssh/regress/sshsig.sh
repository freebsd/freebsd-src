#	$OpenBSD: sshsig.sh,v 1.15 2023/10/12 03:51:08 djm Exp $
#	Placed in the Public Domain.

tid="sshsig"

DATA2=$OBJ/${DATANAME}.2
cat ${DATA} ${DATA} > ${DATA2}

rm -f $OBJ/sshsig-*.sig $OBJ/wrong-key* $OBJ/sigca-key*

sig_namespace="test-$$"
sig_principal="user-$$@example.com"

# Make a "wrong key"
${SSHKEYGEN} -q -t ed25519 -f $OBJ/wrong-key \
	-C "wrong trousers, Grommit" -N '' \
	|| fatal "couldn't generate key"
WRONG=$OBJ/wrong-key.pub

# Make a CA key.
${SSHKEYGEN} -q -t ed25519 -f $OBJ/sigca-key -C "CA" -N '' \
	|| fatal "couldn't generate key"
CA_PRIV=$OBJ/sigca-key
CA_PUB=$OBJ/sigca-key.pub

trace "start agent"
eval `${SSHAGENT} ${EXTRA_AGENT_ARGS} -s` > /dev/null
r=$?
if [ $r -ne 0 ]; then
	fatal "could not start ssh-agent: exit code $r"
fi

SIGNKEYS="$SSH_KEYTYPES"
verbose "$tid: make certificates"
for t in $SSH_KEYTYPES ; do
	${SSHKEYGEN} -q -s $CA_PRIV -z $$ \
	    -I "regress signature key for $USER" \
		-V "19840101:19860101" \
	    -n $sig_principal $OBJ/${t} || \
		fatal "couldn't sign ${t}"
	SIGNKEYS="$SIGNKEYS ${t}-cert.pub"
done

for t in $SIGNKEYS; do
	verbose "$tid: check signature for $t"
	keybase=`basename $t .pub`
	privkey=${OBJ}/`basename $t -cert.pub`
	sigfile=${OBJ}/sshsig-${keybase}.sig
	sigfile_agent=${OBJ}/sshsig-agent-${keybase}.sig
	pubkey=${OBJ}/${keybase}.pub
	cert=${OBJ}/${keybase}-cert.pub
	sigfile_cert=${OBJ}/sshsig-${keybase}-cert.sig

	trace "$tid: key type $t check bad hashlg"
	${SSHKEYGEN} -vvv -Y sign -f ${OBJ}/$t -n $sig_namespace \
	    -Ohashalg=sha1 < $DATA > $sigfile 2>/dev/null && \
		fail "sign using $t with bad hash algorithm succeeded"

	for h in default sha256 sha512 ; do
		case "$h" in
		default) hashalg_arg="" ;;
		*) hashalg_arg="-Ohashalg=$h" ;;
		esac
		trace "$tid: key type $t sign with hash $h"
		${SSHKEYGEN} -vvv -Y sign -f ${OBJ}/$t -n $sig_namespace \
		    $hashalg_arg < $DATA > $sigfile 2>/dev/null || \
			fail "sign using $t / $h failed"
		(printf "$sig_principal " ; cat $pubkey) > $OBJ/allowed_signers
		trace "$tid: key type $t verify with hash $h"
		${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		    -I $sig_principal -f $OBJ/allowed_signers \
		    < $DATA >/dev/null 2>&1 || \
			fail "failed signature for $t / $h key"
	done

	trace "$tid: key type $t verify with limited namespace"
	(printf "$sig_principal namespaces=\"$sig_namespace,whatever\" ";
	 cat $pubkey) > $OBJ/allowed_signers
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-I $sig_principal -f $OBJ/allowed_signers \
		< $DATA >/dev/null 2>&1 || \
		fail "failed signature for $t key w/ limited namespace"

	trace "$tid: key type $t print-pubkey"
	(printf "$sig_principal namespaces=\"$sig_namespace,whatever\" ";
	 cat $pubkey) > $OBJ/allowed_signers
	${SSHKEYGEN} -q -Y verify -s $sigfile -n $sig_namespace \
		-I $sig_principal -f $OBJ/allowed_signers \
		-O print-pubkey \
		< $DATA | cut -d' ' -f1-2 > ${OBJ}/${keybase}-fromsig.pub || \
		fail "failed signature for $t key w/ print-pubkey"
	cut -d' ' -f1-2 ${OBJ}/${keybase}.pub > ${OBJ}/${keybase}-strip.pub
	diff -r ${OBJ}/${keybase}-strip.pub ${OBJ}/${keybase}-fromsig.pub || \
		fail "print-pubkey differs from signature key"

	# Invalid option
	trace "$tid: key type $t verify with bad signers"
	(printf "$sig_principal octopus " ; cat $pubkey) > $OBJ/allowed_signers
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-I $sig_principal -f $OBJ/allowed_signers \
		< $DATA >/dev/null 2>&1 && \
		fail "accepted signature for $t key with bad signers option"

	# Wrong key trusted.
	trace "$tid: key type $t verify with wrong key"
	(printf "$sig_principal " ; cat $WRONG) > $OBJ/allowed_signers
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-I $sig_principal -f $OBJ/allowed_signers \
		< $DATA >/dev/null 2>&1 && \
		fail "accepted signature for $t key with wrong key trusted"

	# incorrect data
	trace "$tid: key type $t verify with wrong data"
	(printf "$sig_principal " ; cat $pubkey) > $OBJ/allowed_signers
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-I $sig_principal -f $OBJ/allowed_signers \
		< $DATA2 >/dev/null 2>&1 && \
		fail "passed signature for wrong data with $t key"

	# wrong principal in signers
	trace "$tid: key type $t verify with wrong principal"
	(printf "josef.k@example.com " ; cat $pubkey) > $OBJ/allowed_signers
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-I $sig_principal -f $OBJ/allowed_signers \
		< $DATA >/dev/null 2>&1 && \
		fail "accepted signature for $t key with wrong principal"

	# wrong namespace
	trace "$tid: key type $t verify with wrong namespace"
	(printf "$sig_principal " ; cat $pubkey) > $OBJ/allowed_signers
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n COWS_COWS_COWS \
		-I $sig_principal -f $OBJ/allowed_signers \
		< $DATA >/dev/null 2>&1 && \
		fail "accepted signature for $t key with wrong namespace"

	# namespace excluded by option
	trace "$tid: key type $t verify with excluded namespace"
	(printf "$sig_principal namespaces=\"whatever\" " ;
	 cat $pubkey) > $OBJ/allowed_signers
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-I $sig_principal -f $OBJ/allowed_signers \
		< $DATA >/dev/null 2>&1 && \
		fail "accepted signature for $t key with excluded namespace"

	( printf "$sig_principal " ;
	  printf "valid-after=\"19800101\",valid-before=\"19900101\" " ;
	  cat $pubkey) > $OBJ/allowed_signers

	# key lifespan valid
	trace "$tid: key type $t verify with valid lifespan"
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-I $sig_principal -f $OBJ/allowed_signers \
		-Overify-time=19850101 \
		< $DATA >/dev/null 2>&1 || \
		fail "failed signature for $t key with valid expiry interval"
	# key not yet valid
	trace "$tid: key type $t verify with not-yet-valid lifespan"
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-I $sig_principal -f $OBJ/allowed_signers \
		-Overify-time=19790101 \
		< $DATA >/dev/null 2>&1 && \
		fail "failed signature for $t not-yet-valid key"
	# key expired
	trace "$tid: key type $t verify with expired lifespan"
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-I $sig_principal -f $OBJ/allowed_signers \
		-Overify-time=19910101 \
		< $DATA >/dev/null 2>&1 && \
		fail "failed signature for $t with expired key"
	# NB. assumes we're not running this test in the 1980s
	trace "$tid: key type $t verify with expired lifespan (now)"
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-I $sig_principal -f $OBJ/allowed_signers \
		< $DATA >/dev/null 2>&1 && \
		fail "failed signature for $t with expired key"

	# key lifespan valid
	trace "$tid: key type $t find-principals with valid lifespan"
	${SSHKEYGEN} -vvv -Y find-principals -s $sigfile \
		-Overify-time="19850101" \
		-f $OBJ/allowed_signers >/dev/null 2>&1 || \
		fail "failed find-principals for $t key with valid expiry interval"
	# key not yet valid
	trace "$tid: key type $t find principals with not-yet-valid lifespan"
	${SSHKEYGEN} -vvv -Y find-principals -s $sigfile \
		-Overify-time="19790101" \
		-f $OBJ/allowed_signers >/dev/null 2>&1 && \
		fail "failed find-principals for $t not-yet-valid key"
	# key expired
	trace "$tid: key type $t find-principals with expired lifespan"
	${SSHKEYGEN} -vvv -Y find-principals -s $sigfile \
		-Overify-time="19990101" \
		-f $OBJ/allowed_signers >/dev/null 2>&1 && \
		fail "failed find-principals for $t with expired key"
	# NB. assumes we're not running this test in the 1980s
	trace "$tid: key type $t find-principals with expired lifespan (now)"
	${SSHKEYGEN} -vvv -Y find-principals -s $sigfile \
		-f $OBJ/allowed_signers >/dev/null 2>&1 && \
		fail "failed find-principals for $t with expired key"

	# public key in revoked keys file
	trace "$tid: key type $t verify with revoked key"
	cat $pubkey > $OBJ/revoked_keys
	(printf "$sig_principal namespaces=\"whatever\" " ;
	 cat $pubkey) > $OBJ/allowed_signers
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-I $sig_principal -f $OBJ/allowed_signers \
		-r $OBJ/revoked_keys \
		< $DATA >/dev/null 2>&1 && \
		fail "accepted signature for $t key, but key is in revoked_keys"

	# public key not revoked, but others are present in revoked_keysfile
	trace "$tid: key type $t verify with unrevoked key"
	cat $WRONG > $OBJ/revoked_keys
	(printf "$sig_principal " ; cat $pubkey) > $OBJ/allowed_signers
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-I $sig_principal -f $OBJ/allowed_signers \
		-r $OBJ/revoked_keys \
		< $DATA >/dev/null 2>&1 || \
		fail "couldn't verify signature for $t key, but key not in revoked_keys"

	# check-novalidate with valid data
	trace "$tid: key type $t check-novalidate with valid data"
	${SSHKEYGEN} -vvv -Y check-novalidate -s $sigfile -n $sig_namespace \
		< $DATA >/dev/null 2>&1 || \
		fail "failed to check valid signature for $t key"

	# check-novalidate with invalid data
	trace "$tid: key type $t check-novalidate with invalid data"
	${SSHKEYGEN} -vvv -Y check-novalidate -s $sigfile -n $sig_namespace \
		< $DATA2 >/dev/null 2>&1 && \
		fail "succeeded checking signature for $t key with invalid data"

	# find-principals with valid public key
	trace "$tid: key type $t find-principals with valid key"
	(printf "$sig_principal " ; cat $pubkey) > $OBJ/allowed_signers
	${SSHKEYGEN} -vvv -Y find-principals -s $sigfile -f $OBJ/allowed_signers >/dev/null 2>&1 || \
		fail "failed to find valid principals in allowed_signers"

	# find-principals with wrong key not in allowed_signers
	trace "$tid: key type $t find-principals with wrong key"
	(printf "$sig_principal " ; cat $WRONG) > $OBJ/allowed_signers
	${SSHKEYGEN} -vvv -Y find-principals -s $sigfile -f $OBJ/allowed_signers >/dev/null 2>&1 && \
		fail "succeeded finding principal with invalid signers file"

	# find-principals with a configured namespace but none on command-line
	trace "$tid: key type $t find-principals with missing namespace"
	(printf "$sig_principal " ;
	 printf "namespaces=\"test1,test2\" ";
	 cat $pubkey) > $OBJ/allowed_signers
	${SSHKEYGEN} -vvv -Y find-principals -s $sigfile \
	    -f $OBJ/allowed_signers >/dev/null 2>&1 || \
		fail "failed finding principal when namespaces are configured"

	# Check signing keys using ssh-agent.
	trace "$tid: key type $t prepare agent"
	${SSHADD} -D >/dev/null 2>&1 # Remove all previously-loaded keys.
	${SSHADD} ${privkey} > /dev/null 2>&1 || fail "ssh-add failed"

	# Move private key to ensure agent key is used
	mv ${privkey} ${privkey}.tmp

	trace "$tid: key type $t sign with agent"
	${SSHKEYGEN} -vvv -Y sign -f $pubkey -n $sig_namespace \
		< $DATA > $sigfile_agent 2>/dev/null || \
		fail "ssh-agent based sign using $pubkey failed"
	trace "$tid: key type $t check signature w/ agent"
	${SSHKEYGEN} -vvv -Y check-novalidate -s $sigfile_agent \
		-n $sig_namespace < $DATA >/dev/null 2>&1 || \
		fail "failed to check valid signature for $t key"
	(printf "$sig_principal namespaces=\"$sig_namespace,whatever\" ";
	 cat $pubkey) > $OBJ/allowed_signers
	${SSHKEYGEN} -vvv -Y verify -s $sigfile_agent -n $sig_namespace \
		-I $sig_principal -f $OBJ/allowed_signers \
		< $DATA >/dev/null 2>&1 || \
		fail "failed signature for $t key w/ limited namespace"

	# Move private key back
	mv ${privkey}.tmp ${privkey}

	# Duplicate principals & keys in allowed_signers but with different validities
	( printf "$sig_principal " ;
	  printf "valid-after=\"19800101\",valid-before=\"19900101\" " ;
	  cat $pubkey;
	  printf "${sig_principal} " ;
	  printf "valid-after=\"19850101\",valid-before=\"20000101\" " ;
	  cat $pubkey) > $OBJ/allowed_signers

	# find-principals outside of any validity lifespan
	trace "$tid: key type $t find principals outside multiple validities"
	${SSHKEYGEN} -vvv -Y find-principals -s $sigfile \
		-Overify-time="20100101" \
		-f $OBJ/allowed_signers >/dev/null 2>&1 && \
		fail "succeeded find-principals for $t verify-time outside of validity"
	# find-principals matching only the first lifespan
	trace "$tid: key type $t find principals matching one validity (1st)"
	${SSHKEYGEN} -vvv -Y find-principals -s $sigfile \
		-Overify-time="19830101" \
		-f $OBJ/allowed_signers >/dev/null 2>&1 || \
		fail "failed find-principals for $t verify-time within first span"
	# find-principals matching both lifespans
	trace "$tid: key type $t find principals matching two validities"
	${SSHKEYGEN} -vvv -Y find-principals -s $sigfile \
		-Overify-time="19880101" \
		-f $OBJ/allowed_signers >/dev/null 2>&1 || \
		fail "failed find-principals for $t verify-time within both spans"
	# find-principals matching only the second lifespan
	trace "$tid: key type $t find principals matching one validity (2nd)"
	${SSHKEYGEN} -vvv -Y find-principals -s $sigfile \
		-Overify-time="19950101" \
		-f $OBJ/allowed_signers >/dev/null 2>&1 || \
		fail "failed find-principals for $t verify-time within second span"

	# verify outside of any validity lifespan
	trace "$tid: key type $t verify outside multiple validities"
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-Overify-time="20100101" -I $sig_principal \
		-r $OBJ/revoked_keys -f $OBJ/allowed_signers \
		< $DATA >/dev/null 2>&1 && \
		fail "succeeded verify for $t verify-time outside of validity"
	# verify matching only the first lifespan
	trace "$tid: key type $t verify matching one validity (1st)"
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-Overify-time="19830101" -I $sig_principal \
		-r $OBJ/revoked_keys -f $OBJ/allowed_signers \
		< $DATA >/dev/null 2>&1 || \
		fail "failed verify for $t verify-time within first span"
	# verify matching both lifespans
	trace "$tid: key type $t verify matching two validities"
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-Overify-time="19880101" -I $sig_principal \
		-r $OBJ/revoked_keys -f $OBJ/allowed_signers \
		< $DATA >/dev/null 2>&1 || \
		fail "failed verify for $t verify-time within both spans"
	# verify matching only the second lifespan
	trace "$tid: key type $t verify matching one validity (2nd)"
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-Overify-time="19950101" -I $sig_principal \
		-r $OBJ/revoked_keys -f $OBJ/allowed_signers \
		< $DATA >/dev/null 2>&1 || \
		fail "failed verify for $t verify-time within second span"

	# Remaining tests are for certificates only.
	case "$keybase" in
		*-cert) ;;
		*) continue ;;
	esac

	# Check key lifespan on find-principals when using the CA
	( printf "$sig_principal " ;
	  printf "cert-authority,valid-after=\"19800101\",valid-before=\"19900101\" ";
	  cat $CA_PUB) > $OBJ/allowed_signers
	# key lifespan valid
	trace "$tid: key type $t find-principals cert lifetime valid"
	${SSHKEYGEN} -vvv -Y find-principals -s $sigfile \
		-Overify-time="19850101" \
		-f $OBJ/allowed_signers >/dev/null 2>&1 || \
		fail "failed find-principals for $t key with valid expiry interval"
	# key not yet valid
	trace "$tid: key type $t find-principals cert lifetime not-yet-valid"
	${SSHKEYGEN} -vvv -Y find-principals -s $sigfile \
		-Overify-time="19790101" \
		-f $OBJ/allowed_signers >/dev/null 2>&1 && \
		fail "failed find-principals for $t not-yet-valid key"
	# key expired
	trace "$tid: key type $t find-principals cert lifetime expired"
	${SSHKEYGEN} -vvv -Y find-principals -s $sigfile \
		-Overify-time="19990101" \
		-f $OBJ/allowed_signers >/dev/null 2>&1 && \
		fail "failed find-principals for $t with expired key"
	# NB. assumes we're not running this test in the 1980s
	trace "$tid: key type $t find-principals cert lifetime expired (now)"
	${SSHKEYGEN} -vvv -Y find-principals -s $sigfile \
		-f $OBJ/allowed_signers >/dev/null 2>&1 && \
		fail "failed find-principals for $t with expired key"

	# correct CA key
	trace "$tid: key type $t verify cert good CA"
	(printf "$sig_principal cert-authority " ;
	 cat $CA_PUB) > $OBJ/allowed_signers
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-I $sig_principal -f $OBJ/allowed_signers \
		-Overify-time=19850101 \
		< $DATA >/dev/null 2>&1 || \
		fail "failed signature for $t cert"

	# find-principals
	trace "$tid: key type $t find-principals cert good CA"
	${SSHKEYGEN} -vvv -Y find-principals -s $sigfile \
		-Overify-time=19850101 \
		-f $OBJ/allowed_signers >/dev/null 2>&1 || \
		fail "failed find-principals for $t with ca key"

	# CA with wildcard principal
	trace "$tid: key type $t find-principals cert good wildcard CA"
	(printf "*@example.com cert-authority " ;
	 cat $CA_PUB) > $OBJ/allowed_signers
	# find-principals CA with wildcard principal
	${SSHKEYGEN} -vvv -Y find-principals -s $sigfile \
		-Overify-time=19850101 \
		-f $OBJ/allowed_signers 2>/dev/null | \
		fgrep "$sig_principal" >/dev/null || \
		fail "failed find-principals for $t with ca key using wildcard principal"

	# verify CA with wildcard principal
	trace "$tid: key type $t verify cert good wildcard CA"
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-I $sig_principal -f $OBJ/allowed_signers \
		-Overify-time=19850101 \
		< $DATA >/dev/null 2>&1 || \
		fail "failed signature for $t cert using wildcard principal"

	# signing key listed as cert-authority
	trace "$tid: key type $t verify signing key listed as CA"
	(printf "$sig_principal cert-authority " ;
	 cat $pubkey) > $OBJ/allowed_signers
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-I $sig_principal -f $OBJ/allowed_signers \
		< $DATA >/dev/null 2>&1 && \
		fail "accepted signature with $t key listed as CA"

	# CA key not flagged cert-authority
	trace "$tid: key type $t verify key not marked as CA"
	(printf "$sig_principal " ; cat $CA_PUB) > $OBJ/allowed_signers
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-I $sig_principal -f $OBJ/allowed_signers \
		< $DATA >/dev/null 2>&1 && \
		fail "accepted signature for $t cert with CA not marked"

	# mismatch between cert principal and file
	trace "$tid: key type $t verify cert with wrong principal"
	(printf "josef.k@example.com cert-authority " ;
	 cat $CA_PUB) > $OBJ/allowed_signers
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-I $sig_principal -f $OBJ/allowed_signers \
		< $DATA >/dev/null 2>&1 && \
		fail "accepted signature for $t cert with wrong principal"

	# Cert valid but CA revoked
	trace "$tid: key type $t verify cert with revoked CA"
	cat $CA_PUB > $OBJ/revoked_keys
	(printf "$sig_principal " ; cat $pubkey) > $OBJ/allowed_signers
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-I $sig_principal -f $OBJ/allowed_signers \
		-r $OBJ/revoked_keys \
		< $DATA >/dev/null 2>&1 && \
		fail "accepted signature for $t key, but CA key in revoked_keys"

	# Set lifespan of CA key and verify signed user certs behave accordingly
	( printf "$sig_principal " ;
	  printf "cert-authority,valid-after=\"19800101\",valid-before=\"19900101\" " ;
	  cat $CA_PUB) > $OBJ/allowed_signers

	# CA key lifespan valid
	trace "$tid: key type $t verify cert valid CA lifespan"
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-I $sig_principal -f $OBJ/allowed_signers \
		-Overify-time=19850101 \
		< $DATA >/dev/null 2>&1 >/dev/null 2>&1 || \
		fail "failed signature for $t key with valid CA expiry interval"
	# CA lifespan is valid but user key not yet valid
	trace "$tid: key type $t verify cert valid CA lifespan, not-yet-valid cert"
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-I $sig_principal -f $OBJ/allowed_signers \
		-Overify-time=19810101 \
		< $DATA >/dev/null 2>&1 && \
		fail "accepted signature for $t key with valid CA expiry interval but not yet valid cert"
	# CA lifespan is valid but user key expired
	trace "$tid: key type $t verify cert valid CA lifespan, expired cert"
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-I $sig_principal -f $OBJ/allowed_signers \
		-Overify-time=19890101 \
		< $DATA >/dev/null 2>&1 && \
		fail "accepted signature for $t key with valid CA expiry interval but expired cert"
	# CA key not yet valid
	trace "$tid: key type $t verify cert CA not-yet-valid"
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-I $sig_principal -f $OBJ/allowed_signers \
		-Overify-time=19790101 \
		< $DATA >/dev/null 2>&1 && \
		fail "accepted signature for $t not-yet-valid CA key"
	# CA key expired
	trace "$tid: key type $t verify cert CA expired"
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-I $sig_principal -f $OBJ/allowed_signers \
		-Overify-time=19910101 \
		< $DATA >/dev/null 2>&1 && \
		fail "accepted signature for $t with expired CA key"
	# NB. assumes we're not running this test in the 1980s
	trace "$tid: key type $t verify cert CA expired (now)"
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-I $sig_principal -f $OBJ/allowed_signers \
		< $DATA >/dev/null 2>&1 && \
		fail "accepted signature for $t with expired CA key"

	# Set lifespan of CA outside of the cert validity
	trace "$tid: key type $t verify CA/cert lifespan mismatch"
	( printf "$sig_principal " ;
	  printf "cert-authority,valid-after=\"19800101\",valid-before=\"19820101\" " ;
	  cat $CA_PUB) > $OBJ/allowed_signers
	# valid cert validity but expired CA
	${SSHKEYGEN} -vvv -Y verify -s $sigfile -n $sig_namespace \
		-I $sig_principal -f $OBJ/allowed_signers \
		-Overify-time=19840101 \
		< $DATA >/dev/null 2>&1 && \
		fail "accepted signature for $t key with expired CA but valid cert"

done

# Test key independant match-principals
(
	printf "principal1 " ; cat $pubkey;
	printf "princi* " ; cat $pubkey;
	printf "unique " ; cat $pubkey;
) > $OBJ/allowed_signers

verbose "$tid: match principals"
${SSHKEYGEN} -Y match-principals -f $OBJ/allowed_signers -I "unique" | \
    fgrep "unique" >/dev/null || \
	fail "failed to match static principal"

trace "$tid: match principals wildcard"
${SSHKEYGEN} -Y match-principals -f $OBJ/allowed_signers -I "princip" | \
    fgrep "princi*" >/dev/null || \
	fail "failed to match wildcard principal"

trace "$tid: match principals static/wildcard"
${SSHKEYGEN} -Y match-principals -f $OBJ/allowed_signers -I "principal1" | \
    fgrep -e "principal1" -e "princi*" >/dev/null || \
	fail "failed to match static and wildcard principal"
verbose "$tid: nomatch principals"
for x in princ prince unknown ; do
	${SSHKEYGEN} -Y match-principals -f $OBJ/allowed_signers \
	    -I $x >/dev/null 2>&1 && \
		fail "succeeded to match unknown principal \"$x\""
done

trace "kill agent"
${SSHAGENT} -k > /dev/null

