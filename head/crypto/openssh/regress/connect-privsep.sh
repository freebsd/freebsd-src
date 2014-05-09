#	$OpenBSD: connect-privsep.sh,v 1.4 2012/07/02 14:37:06 dtucker Exp $
#	Placed in the Public Domain.

tid="proxy connect with privsep"

cp $OBJ/sshd_proxy $OBJ/sshd_proxy.orig
echo 'UsePrivilegeSeparation yes' >> $OBJ/sshd_proxy

for p in 1 2; do
	${SSH} -$p -F $OBJ/ssh_proxy 999.999.999.999 true
	if [ $? -ne 0 ]; then
		fail "ssh privsep+proxyconnect protocol $p failed"
	fi
done

cp $OBJ/sshd_proxy.orig $OBJ/sshd_proxy
echo 'UsePrivilegeSeparation sandbox' >> $OBJ/sshd_proxy

for p in 1 2; do
	${SSH} -$p -F $OBJ/ssh_proxy 999.999.999.999 true
	if [ $? -ne 0 ]; then
		# XXX replace this with fail once sandbox has stabilised
		warn "ssh privsep/sandbox+proxyconnect protocol $p failed"
	fi
done

# Because sandbox is sensitive to changes in libc, especially malloc, retest
# with every malloc.conf option (and none).
for m in '' A F G H J P R S X Z '<' '>'; do
    for p in 1 2; do
	env MALLOC_OPTIONS="$m" ${SSH} -$p -F $OBJ/ssh_proxy 999.999.999.999 true
	if [ $? -ne 0 ]; then
		fail "ssh privsep/sandbox+proxyconnect protocol $p mopt '$m' failed"
	fi
    done
done
