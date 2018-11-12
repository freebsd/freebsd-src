#	$OpenBSD: forcecommand.sh,v 1.3 2015/03/03 22:35:19 markus Exp $
#	Placed in the Public Domain.

tid="forced command"

cp $OBJ/sshd_proxy $OBJ/sshd_proxy_bak

cp /dev/null $OBJ/authorized_keys_$USER
for t in ${SSH_KEYTYPES}; do
	printf 'command="true" ' >>$OBJ/authorized_keys_$USER
	cat $OBJ/$t.pub >> $OBJ/authorized_keys_$USER
done

for p in ${SSH_PROTOCOLS}; do
	trace "forced command in key option proto $p"
	${SSH} -$p -F $OBJ/ssh_proxy somehost false \ ||
	    fail "forced command in key proto $p"
done

cp /dev/null $OBJ/authorized_keys_$USER
for t in ${SSH_KEYTYPES}; do
	printf 'command="false" ' >> $OBJ/authorized_keys_$USER
	cat $OBJ/$t.pub >> $OBJ/authorized_keys_$USER
done

cp $OBJ/sshd_proxy_bak $OBJ/sshd_proxy
echo "ForceCommand true" >> $OBJ/sshd_proxy

for p in ${SSH_PROTOCOLS}; do
	trace "forced command in sshd_config overrides key option proto $p"
	${SSH} -$p -F $OBJ/ssh_proxy somehost false \ ||
	    fail "forced command in key proto $p"
done

cp $OBJ/sshd_proxy_bak $OBJ/sshd_proxy
echo "ForceCommand false" >> $OBJ/sshd_proxy
echo "Match User $USER" >> $OBJ/sshd_proxy
echo "    ForceCommand true" >> $OBJ/sshd_proxy

for p in ${SSH_PROTOCOLS}; do
	trace "forced command with match proto $p"
	${SSH} -$p -F $OBJ/ssh_proxy somehost false \ ||
	    fail "forced command in key proto $p"
done
