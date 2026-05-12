#       $OpenBSD: connect-bigconf.sh,v 1.1 2025/07/04 07:52:17 djm Exp $
#       Placed in the Public Domain.

tid="simple connect"

for x in `jot 10000 1` ; do
	echo "Match version NONEXIST" >> $OBJ/sshd_config
	echo "ChrootDirectory /some/path/for/group/NONEXIST" >> $OBJ/sshd_config
done
#cat $OBJ/sshd_config
start_sshd

trace "direct connect with large sshd_config"
${SSH} -F $OBJ/ssh_config somehost true
if [ $? -ne 0 ]; then
        fail "ssh direct connect with large sshd_config failed"
fi
