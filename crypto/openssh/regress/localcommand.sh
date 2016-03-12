#	$OpenBSD: localcommand.sh,v 1.3 2015/03/03 22:35:19 markus Exp $
#	Placed in the Public Domain.

tid="localcommand"

echo 'PermitLocalCommand yes' >> $OBJ/ssh_proxy
echo 'LocalCommand echo foo' >> $OBJ/ssh_proxy

for p in ${SSH_PROTOCOLS}; do
	verbose "test $tid: proto $p localcommand"
	a=`${SSH} -F $OBJ/ssh_proxy -$p somehost true`
	if [ "$a" != "foo" ] ; then
		fail "$tid proto $p"
	fi
done
