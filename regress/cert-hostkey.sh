#	$OpenBSD: cert-hostkey.sh,v 1.3 2010/03/04 10:38:23 djm Exp $
#	Placed in the Public Domain.

tid="certified host keys"

rm -f $OBJ/known_hosts-cert $OBJ/host_ca_key* $OBJ/cert_host_key*
cp $OBJ/sshd_proxy $OBJ/sshd_proxy_bak

HOSTS='localhost-with-alias,127.0.0.1,::1'

# Create a CA key and add it to known hosts
${SSHKEYGEN} -q -N '' -t rsa  -f $OBJ/host_ca_key ||\
	fail "ssh-keygen of host_ca_key failed"
(
	echon '@cert-authority '
	echon "$HOSTS "
	cat $OBJ/host_ca_key.pub
) > $OBJ/known_hosts-cert

# Generate and sign host keys
for ktype in rsa dsa ; do 
	verbose "$tid: sign host ${ktype} cert"
	# Generate and sign a host key
	${SSHKEYGEN} -q -N '' -t ${ktype} \
	    -f $OBJ/cert_host_key_${ktype} || \
		fail "ssh-keygen of cert_host_key_${ktype} failed"
	${SSHKEYGEN} -h -q -s $OBJ/host_ca_key \
	    -I "regress host key for $USER" \
	    -n $HOSTS $OBJ/cert_host_key_${ktype} ||
		fail "couldn't sign cert_host_key_${ktype}"
done

# Basic connect tests
for privsep in yes no ; do
	for ktype in rsa dsa ; do 
		verbose "$tid: host ${ktype} cert connect privsep $privsep"
		(
			cat $OBJ/sshd_proxy_bak
			echo HostKey $OBJ/cert_host_key_${ktype}
			echo HostCertificate $OBJ/cert_host_key_${ktype}-cert.pub
			echo UsePrivilegeSeparation $privsep
		) > $OBJ/sshd_proxy

		${SSH} -2 -oUserKnownHostsFile=$OBJ/known_hosts-cert \
		    -oGlobalKnownHostsFile=$OBJ/known_hosts-cert \
			-F $OBJ/ssh_proxy somehost true
		if [ $? -ne 0 ]; then
			fail "ssh cert connect failed"
		fi
	done
done

# Revoked certificates with key present
(
	echon '@cert-authority '
	echon "$HOSTS "
	cat $OBJ/host_ca_key.pub
	echon '@revoked '
	echon "* "
	cat $OBJ/cert_host_key_rsa.pub
	echon '@revoked '
	echon "* "
	cat $OBJ/cert_host_key_dsa.pub
) > $OBJ/known_hosts-cert
for privsep in yes no ; do
	for ktype in rsa dsa ; do 
		verbose "$tid: host ${ktype} revoked cert privsep $privsep"
		(
			cat $OBJ/sshd_proxy_bak
			echo HostKey $OBJ/cert_host_key_${ktype}
			echo HostCertificate $OBJ/cert_host_key_${ktype}-cert.pub
			echo UsePrivilegeSeparation $privsep
		) > $OBJ/sshd_proxy

		${SSH} -2 -oUserKnownHostsFile=$OBJ/known_hosts-cert \
		    -oGlobalKnownHostsFile=$OBJ/known_hosts-cert \
			-F $OBJ/ssh_proxy somehost true >/dev/null 2>&1
		if [ $? -eq 0 ]; then
			fail "ssh cert connect succeeded unexpectedly"
		fi
	done
done

# Revoked CA
(
	echon '@cert-authority '
	echon "$HOSTS "
	cat $OBJ/host_ca_key.pub
	echon '@revoked '
	echon "* "
	cat $OBJ/host_ca_key.pub
) > $OBJ/known_hosts-cert
for ktype in rsa dsa ; do 
	verbose "$tid: host ${ktype} revoked cert"
	(
		cat $OBJ/sshd_proxy_bak
		echo HostKey $OBJ/cert_host_key_${ktype}
		echo HostCertificate $OBJ/cert_host_key_${ktype}-cert.pub
	) > $OBJ/sshd_proxy
	${SSH} -2 -oUserKnownHostsFile=$OBJ/known_hosts-cert \
	    -oGlobalKnownHostsFile=$OBJ/known_hosts-cert \
		-F $OBJ/ssh_proxy somehost true >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		fail "ssh cert connect succeeded unexpectedly"
	fi
done

# Create a CA key and add it to known hosts
(
	echon '@cert-authority '
	echon "$HOSTS "
	cat $OBJ/host_ca_key.pub
) > $OBJ/known_hosts-cert

test_one() {
	ident=$1
	result=$2
	sign_opts=$3
	
	verbose "$tid: test host cert connect $ident expect $result"

	${SSHKEYGEN} -q -s $OBJ/host_ca_key -I "regress host key for $USER" \
	    $sign_opts \
	    $OBJ/cert_host_key_rsa ||
		fail "couldn't sign cert_host_key_rsa"
	(
		cat $OBJ/sshd_proxy_bak
		echo HostKey $OBJ/cert_host_key_rsa
		echo HostCertificate $OBJ/cert_host_key_rsa-cert.pub
	) > $OBJ/sshd_proxy

	${SSH} -2 -oUserKnownHostsFile=$OBJ/known_hosts-cert \
	    -oGlobalKnownHostsFile=$OBJ/known_hosts-cert \
	    -F $OBJ/ssh_proxy somehost true >/dev/null 2>&1
	rc=$?
	if [ "x$result" = "xsuccess" ] ; then
		if [ $rc -ne 0 ]; then
			fail "ssh cert connect $ident failed unexpectedly"
		fi
	else
		if [ $rc -eq 0 ]; then
			fail "ssh cert connect $ident succeeded unexpectedly"
		fi
	fi
}

test_one "user-certificate"	failure "-n $HOSTS"
test_one "empty principals"	success "-h"
test_one "wrong principals"	failure "-h -n foo"
test_one "cert not yet valid"	failure "-h -V20200101:20300101"
test_one "cert expired"		failure "-h -V19800101:19900101"
test_one "cert valid interval"	success "-h -V-1w:+2w"
test_one "cert has constraints"	failure "-h -Oforce-command=false"

# Check downgrade of cert to raw key when no CA found
rm -f $OBJ/known_hosts-cert $OBJ/cert_host_key*
for ktype in rsa dsa ; do 
	verbose "$tid: host ${ktype} cert downgrade to raw key"
	# Generate and sign a host key
	${SSHKEYGEN} -q -N '' -t ${ktype} \
	    -f $OBJ/cert_host_key_${ktype} || \
		fail "ssh-keygen of cert_host_key_${ktype} failed"
	${SSHKEYGEN} -h -q -s $OBJ/host_ca_key -I "regress host key for $USER" \
	    -n $HOSTS $OBJ/cert_host_key_${ktype} ||
		fail "couldn't sign cert_host_key_${ktype}"
	(
		echon "$HOSTS "
		cat $OBJ/cert_host_key_${ktype}.pub
	) > $OBJ/known_hosts-cert
	(
		cat $OBJ/sshd_proxy_bak
		echo HostKey $OBJ/cert_host_key_${ktype}
		echo HostCertificate $OBJ/cert_host_key_${ktype}-cert.pub
	) > $OBJ/sshd_proxy
	
	${SSH} -2 -oUserKnownHostsFile=$OBJ/known_hosts-cert \
	    -oGlobalKnownHostsFile=$OBJ/known_hosts-cert \
		-F $OBJ/ssh_proxy somehost true
	if [ $? -ne 0 ]; then
		fail "ssh cert connect failed"
	fi
done

# Wrong certificate
(
	echon '@cert-authority '
	echon "$HOSTS "
	cat $OBJ/host_ca_key.pub
) > $OBJ/known_hosts-cert
for ktype in rsa dsa ; do 
	# Self-sign key
	${SSHKEYGEN} -h -q -s $OBJ/cert_host_key_${ktype} \
	    -I "regress host key for $USER" \
	    -n $HOSTS $OBJ/cert_host_key_${ktype} ||
		fail "couldn't sign cert_host_key_${ktype}"
	verbose "$tid: host ${ktype} connect wrong cert"
	(
		cat $OBJ/sshd_proxy_bak
		echo HostKey $OBJ/cert_host_key_${ktype}
		echo HostCertificate $OBJ/cert_host_key_${ktype}-cert.pub
	) > $OBJ/sshd_proxy

	${SSH} -2 -oUserKnownHostsFile=$OBJ/known_hosts-cert \
	    -oGlobalKnownHostsFile=$OBJ/known_hosts-cert \
		-F $OBJ/ssh_proxy -q somehost true >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		fail "ssh cert connect $ident succeeded unexpectedly"
	fi
done

rm -f $OBJ/known_hosts-cert $OBJ/host_ca_key* $OBJ/cert_host_key*
