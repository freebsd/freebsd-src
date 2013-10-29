#	$OpenBSD: sftp-chroot.sh,v 1.2 2013/05/17 04:29:14 dtucker Exp $
#	Placed in the Public Domain.

tid="sftp in chroot"

CHROOT=/var/run
FILENAME=testdata_${USER}
PRIVDATA=${CHROOT}/${FILENAME}

if [ -z "$SUDO" ]; then
  echo "skipped: need SUDO to create file in /var/run, test won't work without"
  exit 0
fi

$SUDO sh -c "echo mekmitastdigoat > $PRIVDATA" || \
	fatal "create $PRIVDATA failed"

start_sshd -oChrootDirectory=$CHROOT -oForceCommand="internal-sftp -d /"

verbose "test $tid: get"
${SFTP} -qS "$SSH" -F $OBJ/ssh_config host:/${FILENAME} $COPY || \
	fatal "Fetch ${FILENAME} failed"
cmp $PRIVDATA $COPY || fail "$PRIVDATA $COPY differ"

$SUDO rm $PRIVDATA
