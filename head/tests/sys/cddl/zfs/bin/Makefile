# $FreeBSD$

.include <bsd.own.mk>

MAN=
BINDIR=	${TESTSBASE}/sys/cddl/zfs/bin
SCRIPTSDIR=	${TESTSBASE}/sys/cddl/zfs/bin

SCRIPTS+=	bsddisks.ksh df.ksh dircmp.ksh dumpadm.ksh ff.ksh fmadm.ksh
SCRIPTS+=	fmdump.ksh format.ksh fstyp.ksh groupadd.ksh groupdel.ksh
SCRIPTS+=	groupmod.ksh groupshow.ksh svcs.ksh swap.ksh useradd.ksh
SCRIPTS+=	userdel.ksh usermod.ksh zfs.ksh zfs_crypto.ksh
SCRIPTS+=	zfs_version.ksh zlogin.ksh zoneadm.ksh zonecfg.ksh zpool.ksh
SCRIPTS+=	zpool_bsd.ksh zpool_smi.ksh zpool_version.ksh

PROGS+=	chg_usr_exec
# Not ported to FreeBSD
# PROGRS+= devname2devid
PROGS += dir_rd_update file_check file_trunc
PROGS+=	file_write largest_file mktree mmapwrite randfree_file readmmap
PROGS+=	rename_dir rm_lnkcnt_zero_file

.for p in ${PROGS}
SRCS.$p=	$p.c
.endfor

LDADD.mmapwrite+=	-lpthread
LDADD.rm_lnkcnt_zero_file+=	-lpthread

.include <bsd.progs.mk>
