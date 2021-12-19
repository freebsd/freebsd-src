#	$OpenBSD: sftp-chroot.sh,v 1.8 2021/09/01 00:50:27 dtucker Exp $
#	Placed in the Public Domain.

tid="sftp in chroot"

CHROOT=/var/run
FILENAME=testdata_${USER}.$$
PRIVDATA=${CHROOT}/${FILENAME}
trap "${SUDO} rm -f ${PRIVDATA}" 0

if [ -z "$SUDO" -a ! -w /var/run ]; then
	skip "need SUDO to create file in /var/run, test won't work without"
fi

if ! $OBJ/check-perm -m chroot "$CHROOT" ; then
  skip "$CHROOT is unsuitable as ChrootDirectory"
fi

$SUDO sh -c "echo mekmitastdigoat > $PRIVDATA" || \
	fatal "create $PRIVDATA failed"

start_sshd -oChrootDirectory=$CHROOT -oForceCommand="internal-sftp -d /"

verbose "test $tid: get"
${SFTP} -S "$SSH" -F $OBJ/ssh_config host:/${FILENAME} $COPY \
    >>$TEST_REGRESS_LOGFILE 2>&1 || \
	fatal "Fetch ${FILENAME} failed"
cmp $PRIVDATA $COPY || fail "$PRIVDATA $COPY differ"
