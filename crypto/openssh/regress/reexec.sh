#	$OpenBSD: reexec.sh,v 1.3 2004/06/25 01:32:44 djm Exp $
#	Placed in the Public Domain.

tid="reexec tests"

DATA=/bin/ls
COPY=${OBJ}/copy
SSHD_ORIG=$SSHD
SSHD_COPY=$OBJ/sshd.copy

# Start a sshd and then delete it
start_sshd_copy_zap ()
{
	cp $SSHD_ORIG $SSHD_COPY
	SSHD=$SSHD_COPY
	start_sshd
	rm -f $SSHD_COPY
	SSHD=$SSHD_ORIG
}

verbose "test config passing"
cp $OBJ/sshd_config $OBJ/sshd_config.orig

start_sshd

echo "InvalidXXX=no" >> $OBJ/sshd_config

rm -f ${COPY}
for p in 1 2; do
	verbose "$tid: proto $p"
	${SSH} -nqo "Protocol=$p" -F $OBJ/ssh_config somehost \
	    cat ${DATA} > ${COPY}
	if [ $? -ne 0 ]; then
		fail "ssh cat $DATA failed"
	fi
	cmp ${DATA} ${COPY}		|| fail "corrupted copy"
	rm -f ${COPY}
done

$SUDO kill `cat $PIDFILE`
rm -f $PIDFILE

cp $OBJ/sshd_config.orig $OBJ/sshd_config

verbose "test reexec fallback"

start_sshd_copy_zap

rm -f ${COPY}
for p in 1 2; do
	verbose "$tid: proto $p"
	${SSH} -nqo "Protocol=$p" -F $OBJ/ssh_config somehost \
	    cat ${DATA} > ${COPY}
	if [ $? -ne 0 ]; then
		fail "ssh cat $DATA failed"
	fi
	cmp ${DATA} ${COPY}		|| fail "corrupted copy"
	rm -f ${COPY}
done

$SUDO kill `cat $PIDFILE`
rm -f $PIDFILE

verbose "test reexec fallback without privsep"

cp $OBJ/sshd_config.orig $OBJ/sshd_config
echo "UsePrivilegeSeparation=no" >> $OBJ/sshd_config

start_sshd_copy_zap

rm -f ${COPY}
for p in 1 2; do
	verbose "$tid: proto $p"
	${SSH} -nqo "Protocol=$p" -F $OBJ/ssh_config somehost \
	    cat ${DATA} > ${COPY}
	if [ $? -ne 0 ]; then
		fail "ssh cat $DATA failed"
	fi
	cmp ${DATA} ${COPY}		|| fail "corrupted copy"
	rm -f ${COPY}
done

$SUDO kill `cat $PIDFILE`
rm -f $PIDFILE

cp $OBJ/sshd_config.orig $OBJ/sshd_config

