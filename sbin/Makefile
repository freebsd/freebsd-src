#	@(#)Makefile	5.4.1.1 (Berkeley) 5/7/91
#	$Id: Makefile,v 1.10 1994/02/07 05:23:06 alm Exp $
#

SUBDIR=	XNSrouted adjkerntz badsect chkconfig clri comcontrol disklabel dmesg \
	dump dumpfs fastboot fdisk fsck ft halt ifconfig mknod mount \
	mount_isofs mount_pcfs mount_procfs mountd newfs nfsd nfsiod ping \
	quotacheck reboot restore route routed savecore scsi shutdown \
	slattach st swapon tunefs umount 

.if defined(INIT_ORIG)
SUBDIR+= init
.elif defined(INIT_CHMR)
SUBDIR+= init.chmr
.else
SUBDIR+= init.bsdi
.endif


# Not ported: enpload
# Missing: icheck ncheck

.include <bsd.subdir.mk>
