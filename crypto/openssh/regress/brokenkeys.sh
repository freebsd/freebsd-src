#	$OpenBSD: brokenkeys.sh,v 1.1 2004/10/29 23:59:22 djm Exp $
#	Placed in the Public Domain.

tid="broken keys"

KEYS="$OBJ/authorized_keys_${USER}"

start_sshd

mv ${KEYS} ${KEYS}.bak

# Truncated key
echo "ssh-rsa AAAAB3NzaC1yc2EAAAABIwAAAIEABTM= bad key" > $KEYS
cat ${KEYS}.bak >> ${KEYS}
cat $OBJ/$t.pub >> $OBJ/authorized_keys_$USER

${SSH} -2 -F $OBJ/ssh_config somehost true
if [ $? -ne 0 ]; then
	fail "ssh connect with protocol $p failed"
fi

mv ${KEYS}.bak ${KEYS}

