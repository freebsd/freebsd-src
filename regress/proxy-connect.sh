#	$OpenBSD: proxy-connect.sh,v 1.10 2017/04/30 23:34:55 djm Exp $
#	Placed in the Public Domain.

tid="proxy connect"

mv $OBJ/sshd_proxy $OBJ/sshd_proxy.orig

for ps in no yes; do
	cp $OBJ/sshd_proxy.orig $OBJ/sshd_proxy
	echo "UsePrivilegeSeparation $ps" >> $OBJ/sshd_proxy
	for c in no yes; do
		verbose "plain username privsep=$ps comp=$c"
		opts="-oCompression=$c -F $OBJ/ssh_proxy"
		SSH_CONNECTION=`${SSH} $opts 999.999.999.999 'echo $SSH_CONNECTION'`
		if [ $? -ne 0 ]; then
			fail "ssh proxyconnect privsep=$ps comp=$c failed"
		fi
		if [ "$SSH_CONNECTION" != "UNKNOWN 65535 UNKNOWN 65535" ]; then
			fail "bad SSH_CONNECTION privsep=$ps comp=$c: " \
			    "$SSH_CONNECTION"
		fi
	done
done

verbose "username with style"
${SSH} -F $OBJ/ssh_proxy ${USER}:style@999.999.999.999 true || \
	fail "ssh proxyconnect failed"
