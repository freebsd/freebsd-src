#	@(#)Makefile	5.4.1.1 (Berkeley) 5/7/91
#

SUBDIR=	XNSrouted badsect comcontrol clri disklabel dmesg dump dumpfs fastboot fdisk \
	fsck halt ifconfig mknod mount mount_isofs mount_pcfs mountd \
	newfs nfsd nfsiod ping quotacheck reboot restore route routed \
	savecore shutdown slattach st swapon tunefs umount

.if defined(INIT_BSDI)
SUBDIR+= init.bsdi
.elif defined(INIT_CHMR)
SUBDIR+= init.chmr
.else
SUBDIR+= init
.endif


# Not ported: enpload
# Missing: icheck ncheck

.include <bsd.subdir.mk>
