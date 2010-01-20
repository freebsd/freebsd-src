#	$OpenBSD: forcecommand.sh,v 1.1 2006/07/19 13:09:28 dtucker Exp $
#	Placed in the Public Domain.

tid="forced command"

cp $OBJ/sshd_proxy $OBJ/sshd_proxy_bak

echon 'command="true" ' >$OBJ/authorized_keys_$USER
cat $OBJ/rsa.pub >> $OBJ/authorized_keys_$USER
echon 'command="true" ' >>$OBJ/authorized_keys_$USER
cat $OBJ/rsa1.pub >> $OBJ/authorized_keys_$USER

for p in 1 2; do
	trace "forced command in key option proto $p"
	${SSH} -$p -F $OBJ/ssh_proxy somehost false \ ||
	    fail "forced command in key proto $p"
done

echon 'command="false" ' >$OBJ/authorized_keys_$USER
cat $OBJ/rsa.pub >> $OBJ/authorized_keys_$USER
echon 'command="false" ' >>$OBJ/authorized_keys_$USER
cat $OBJ/rsa1.pub >> $OBJ/authorized_keys_$USER

cp $OBJ/sshd_proxy_bak $OBJ/sshd_proxy
echo "ForceCommand true" >> $OBJ/sshd_proxy

for p in 1 2; do
	trace "forced command in sshd_config overrides key option proto $p"
	${SSH} -$p -F $OBJ/ssh_proxy somehost false \ ||
	    fail "forced command in key proto $p"
done

cp $OBJ/sshd_proxy_bak $OBJ/sshd_proxy
echo "ForceCommand false" >> $OBJ/sshd_proxy
echo "Match User $USER" >> $OBJ/sshd_proxy
echo "    ForceCommand true" >> $OBJ/sshd_proxy

for p in 1 2; do
	trace "forced command with match proto $p"
	${SSH} -$p -F $OBJ/ssh_proxy somehost false \ ||
	    fail "forced command in key proto $p"
done
