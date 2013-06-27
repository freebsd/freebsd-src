#!/bin/sh

ORIGDIR=clean-head
WORKDIR=head

DIFFFILENAME=$(date '+vps-%Y%m%d_%s.diff')

diff -Naupr -x .svn -x rsync_vps.\* -I '$Id.*$' -I '$FreeBSD.*$' ${ORIGDIR} ${WORKDIR} > ${DIFFFILENAME} 

ls -lh ${DIFFFILENAME}

# EOF
