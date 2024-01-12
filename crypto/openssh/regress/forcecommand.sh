#	$OpenBSD: forcecommand.sh,v 1.7 2023/11/01 02:08:38 dtucker Exp $
#	Placed in the Public Domain.

tid="forced command"

cp $OBJ/sshd_proxy $OBJ/sshd_proxy_bak

authorized_keys() {
	cmd=$1
	cp /dev/null $OBJ/authorized_keys_$USER
	for t in ${SSH_KEYTYPES}; do
		test -z "$cmd" || \
			printf "command=\"$cmd\" " >>$OBJ/authorized_keys_$USER
		cat $OBJ/$t.pub >> $OBJ/authorized_keys_$USER
	done
}

trace "test config with sftp"
authorized_keys
rm -f $OBJ/ssh_proxy.tmp
echo "@get $OBJ/ssh_proxy $OBJ/ssh_proxy.tmp" | \
	${SFTP} -S ${SSH} -b - -qF $OBJ/ssh_proxy somehost 2>/dev/null || \
	fail "sftp failed"
test -f "$OBJ/ssh_proxy.tmp" || fail "sftp did not download file"
rm -f $OBJ/ssh_proxy.tmp

trace "forced command in key option"
authorized_keys true
${SSH} -F $OBJ/ssh_proxy somehost false || fail "forced command in key option"

authorized_keys false
cp $OBJ/sshd_proxy_bak $OBJ/sshd_proxy
echo "ForceCommand true" >> $OBJ/sshd_proxy

trace "forced command in sshd_config overrides key option"
${SSH} -F $OBJ/ssh_proxy somehost false || fail "forced command config"

authorized_keys
cp $OBJ/sshd_proxy_bak $OBJ/sshd_proxy
echo "ForceCommand false" >> $OBJ/sshd_proxy

trace "force command overriding subsystem"
echo "@get $OBJ/ssh_proxy $OBJ/ssh_proxy.tmp" | \
	${SFTP} -S ${SSH} -F $OBJ/ssh_proxy -oLoglevel=quiet somehost && \
	fail "sftp succeeded"

echo "Match User $USER" >> $OBJ/sshd_proxy
echo "    ForceCommand true" >> $OBJ/sshd_proxy

trace "forced command with match"
${SSH} -F $OBJ/ssh_proxy somehost false || fail "forced command match"

trace "force command in match overriding subsystem"
echo "@get $OBJ/ssh_proxy $OBJ/ssh_proxy.tmp" | \
	${SFTP} -S ${SSH} -F $OBJ/ssh_proxy -oLoglevel=quiet somehost && \
	fail "sftp succeeded"

trace "force command to sftpserver"
grep -vi subsystem $OBJ/sshd_proxy_bak > $OBJ/sshd_proxy
echo "Subsystem sftp /bin/false" >> $OBJ/sshd_proxy
echo "ForceCommand ${SFTPSERVER}" >> $OBJ/sshd_proxy
rm -f $OBJ/ssh_proxy.tmp
echo "@get $OBJ/ssh_proxy $OBJ/ssh_proxy.tmp" | \
	${SFTP} -S ${SSH} -b - -qF $OBJ/ssh_proxy somehost 2>/dev/null || \
	fail "sftp failed"
test -f "$OBJ/ssh_proxy.tmp" || fail "sftp did not download file"
rm -f $OBJ/ssh_proxy.tmp
