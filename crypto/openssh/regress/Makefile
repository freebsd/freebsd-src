#	$OpenBSD: Makefile,v 1.13 2002/04/01 22:15:08 markus Exp $

REGRESSTARGETS=	t1 t2 t3 t4 t5 t6 t7

CLEANFILES+=	t2.out t6.out1 t6.out2 t7.out t7.out.pub 

LTESTS= 	connect \
		proxy-connect \
		connect-privsep \
		proto-version \
		proto-mismatch \
		exit-status \
		transfer \
		stderr-data \
		stderr-after-eof \
		broken-pipe \
		try-ciphers \
		yes-head \
		agent \
		keyscan \
		sftp \
		forwarding

USER!=		id -un
CLEANFILES+=	authorized_keys_${USER} known_hosts pidfile \
		ssh_config ssh_proxy sshd_config sshd_proxy \
		rsa.pub rsa rsa1.pub rsa1 host.rsa host.rsa1 \
		rsa-agent rsa-agent.pub rsa1-agent rsa1-agent.pub \
		ls.copy

#LTESTS+=	ssh-com ssh-com-client ssh-com-keygen ssh-com-sftp

t1:
	ssh-keygen -if ${.CURDIR}/rsa_ssh2.prv | diff - ${.CURDIR}/rsa_openssh.prv

t2:
	cat ${.CURDIR}/rsa_openssh.prv > t2.out
	chmod 600 t2.out
	ssh-keygen -yf t2.out | diff - ${.CURDIR}/rsa_openssh.pub

t3:
	ssh-keygen -ef ${.CURDIR}/rsa_openssh.pub |\
		ssh-keygen -if /dev/stdin |\
		diff - ${.CURDIR}/rsa_openssh.pub

t4:
	ssh-keygen -lf ${.CURDIR}/rsa_openssh.pub |\
		awk '{print $$2}' | diff - ${.CURDIR}/t4.ok

t5:
	ssh-keygen -Bf ${.CURDIR}/rsa_openssh.pub |\
		awk '{print $$2}' | diff - ${.CURDIR}/t5.ok

t6:
	ssh-keygen -if ${.CURDIR}/dsa_ssh2.prv > t6.out1
	ssh-keygen -if ${.CURDIR}/dsa_ssh2.pub > t6.out2
	chmod 600 t6.out1
	ssh-keygen -yf t6.out1 | diff - t6.out2

t7.out:
	ssh-keygen -q -t rsa -N '' -f $@

t7: t7.out
	ssh-keygen -lf t7.out > /dev/null
	ssh-keygen -Bf t7.out > /dev/null

.for t in ${LTESTS}
REGRESSTARGETS+=t-${t}
t-${t}:
	sh ${.CURDIR}/test-exec.sh ${.OBJDIR} ${.CURDIR}/${t}.sh
.endfor

.include "bsd.regress.mk"
