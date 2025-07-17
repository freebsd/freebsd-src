#	$OpenBSD
#	Placed in the Public Domain.

tid="penalties"

grep -vi PerSourcePenalties $OBJ/sshd_config > $OBJ/sshd_config.bak
cp $OBJ/authorized_keys_${USER} $OBJ/authorized_keys_${USER}.bak

conf() {
	test -z "$PIDFILE" || stop_sshd
	(cat $OBJ/sshd_config.bak ;
	 echo "PerSourcePenalties $@") > $OBJ/sshd_config
	cp $OBJ/authorized_keys_${USER}.bak $OBJ/authorized_keys_${USER}
	start_sshd
}

conf "noauth:10s authfail:10s max:20s min:1s"

verbose "test connect"
${SSH} -F $OBJ/ssh_config somehost true || fatal "basic connect failed"

verbose "penalty expiry"

# Incur a penalty
cat /dev/null > $OBJ/authorized_keys_${USER}
${SSH} -F $OBJ/ssh_config somehost true && fatal "authfail connect succeeded"
sleep 2

# Check denied
cp $OBJ/authorized_keys_${USER}.bak $OBJ/authorized_keys_${USER}
${SSH} -F $OBJ/ssh_config somehost true && fatal "authfail not rejected"

# Let it expire and try again.
sleep 11
${SSH} -F $OBJ/ssh_config somehost true || fail "authfail not expired"
