#	$OpenBSD: cert-userkey.sh,v 1.3 2010/03/04 10:38:23 djm Exp $
#	Placed in the Public Domain.

tid="certified user keys"

rm -f $OBJ/authorized_keys_$USER $OBJ/user_ca_key* $OBJ/cert_user_key*
cp $OBJ/sshd_proxy $OBJ/sshd_proxy_bak

# Create a CA key
${SSHKEYGEN} -q -N '' -t rsa  -f $OBJ/user_ca_key ||\
	fail "ssh-keygen of user_ca_key failed"

# Generate and sign user keys
for ktype in rsa dsa ; do 
	verbose "$tid: sign user ${ktype} cert"
	${SSHKEYGEN} -q -N '' -t ${ktype} \
	    -f $OBJ/cert_user_key_${ktype} || \
		fail "ssh-keygen of cert_user_key_${ktype} failed"
	${SSHKEYGEN} -q -s $OBJ/user_ca_key -I \
	    "regress user key for $USER" \
	    -n $USER $OBJ/cert_user_key_${ktype} ||
		fail "couldn't sign cert_user_key_${ktype}"
done

basic_tests() {
	auth=$1
	if test "x$auth" = "xauthorized_keys" ; then
		# Add CA to authorized_keys
		(
			echon 'cert-authority '
			cat $OBJ/user_ca_key.pub
		) > $OBJ/authorized_keys_$USER
	else
		echo > $OBJ/authorized_keys_$USER
		extra_sshd="TrustedUserCAKeys $OBJ/user_ca_key.pub"
	fi
	
	for ktype in rsa dsa ; do 
		for privsep in yes no ; do
			_prefix="${ktype} privsep $privsep $auth"
			# Simple connect
			verbose "$tid: ${_prefix} connect"
			(
				cat $OBJ/sshd_proxy_bak
				echo "UsePrivilegeSeparation $privsep"
				echo "$extra_sshd"
			) > $OBJ/sshd_proxy
	
			${SSH} -2i $OBJ/cert_user_key_${ktype} \
			    -F $OBJ/ssh_proxy somehost true
			if [ $? -ne 0 ]; then
				fail "ssh cert connect failed"
			fi

			# Revoked keys
			verbose "$tid: ${_prefix} revoked key"
			(
				cat $OBJ/sshd_proxy_bak
				echo "UsePrivilegeSeparation $privsep"
				echo "RevokedKeys $OBJ/cert_user_key_${ktype}.pub"
				echo "$extra_sshd"
			) > $OBJ/sshd_proxy
			${SSH} -2i $OBJ/cert_user_key_${ktype} \
			    -F $OBJ/ssh_proxy somehost true >/dev/null 2>&1
			if [ $? -eq 0 ]; then
				fail "ssh cert connect succeeded unexpecedly"
			fi
		done
	
		# Revoked CA
		verbose "$tid: ${ktype} $auth revoked CA key"
		(
			cat $OBJ/sshd_proxy_bak
			echo "RevokedKeys $OBJ/user_ca_key.pub"
			echo "$extra_sshd"
		) > $OBJ/sshd_proxy
		${SSH} -2i $OBJ/cert_user_key_${ktype} -F $OBJ/ssh_proxy \
		    somehost true >/dev/null 2>&1
		if [ $? -eq 0 ]; then
			fail "ssh cert connect succeeded unexpecedly"
		fi
	done
	
	verbose "$tid: $auth CA does not authenticate"
	(
		cat $OBJ/sshd_proxy_bak
		echo "$extra_sshd"
	) > $OBJ/sshd_proxy
	verbose "$tid: ensure CA key does not authenticate user"
	${SSH} -2i $OBJ/user_ca_key \
	    -F $OBJ/ssh_proxy somehost true >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		fail "ssh cert connect with CA key succeeded unexpectedly"
	fi
}

basic_tests authorized_keys
basic_tests TrustedUserCAKeys

test_one() {
	ident=$1
	result=$2
	sign_opts=$3
	auth_choice=$4

	if test "x$auth_choice" = "x" ; then
		auth_choice="authorized_keys TrustedUserCAKeys"
	fi

	for auth in $auth_choice ; do
		cat $OBJ/sshd_proxy_bak > $OBJ/sshd_proxy
		if test "x$auth" = "xauthorized_keys" ; then
			# Add CA to authorized_keys
			(
				echon 'cert-authority '
				cat $OBJ/user_ca_key.pub
			) > $OBJ/authorized_keys_$USER
		else
			echo > $OBJ/authorized_keys_$USER
			echo "TrustedUserCAKeys $OBJ/user_ca_key.pub" >> \
			    $OBJ/sshd_proxy

		fi
		
		verbose "$tid: $ident auth $auth expect $result"
		${SSHKEYGEN} -q -s $OBJ/user_ca_key \
		    -I "regress user key for $USER" \
		    $sign_opts \
		    $OBJ/cert_user_key_rsa ||
			fail "couldn't sign cert_user_key_rsa"
	
		${SSH} -2i $OBJ/cert_user_key_rsa -F $OBJ/ssh_proxy \
		    somehost true >/dev/null 2>&1
		rc=$?
		if [ "x$result" = "xsuccess" ] ; then
			if [ $rc -ne 0 ]; then
				fail "$ident failed unexpectedly"
			fi
		else
			if [ $rc -eq 0 ]; then
				fail "$ident succeeded unexpectedly"
			fi
		fi
	done
}

test_one "correct principal"	success "-n ${USER}"
test_one "host-certificate"	failure "-n ${USER} -h"
test_one "wrong principals"	failure "-n foo"
test_one "cert not yet valid"	failure "-n ${USER} -V20200101:20300101"
test_one "cert expired"		failure "-n ${USER} -V19800101:19900101"
test_one "cert valid interval"	success "-n ${USER} -V-1w:+2w"
test_one "wrong source-address"	failure "-n ${USER} -Osource-address=10.0.0.0/8"
test_one "force-command"	failure "-n ${USER} -Oforce-command=false"

# Behaviour is different here: TrustedUserCAKeys doesn't allow empty principals
test_one "empty principals"	success "" authorized_keys
test_one "empty principals"	failure "" TrustedUserCAKeys

# Wrong certificate
for ktype in rsa dsa ; do 
	# Self-sign
	${SSHKEYGEN} -q -s $OBJ/cert_user_key_${ktype} -I \
	    "regress user key for $USER" \
	    -n $USER $OBJ/cert_user_key_${ktype} ||
		fail "couldn't sign cert_user_key_${ktype}"
	verbose "$tid: user ${ktype} connect wrong cert"
	${SSH} -2i $OBJ/cert_user_key_${ktype} -F $OBJ/ssh_proxy \
	    somehost true >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		fail "ssh cert connect $ident succeeded unexpectedly"
	fi
done

rm -f $OBJ/authorized_keys_$USER $OBJ/user_ca_key* $OBJ/cert_user_key*

