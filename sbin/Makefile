#	@(#)Makefile	5.4.1.1 (Berkeley) 5/7/91
#	$Id: Makefile,v 1.13 1994/05/18 16:40:00 jkh Exp $
#

SUBDIR=	adjkerntz badsect chkconfig clri comcontrol disklabel dmesg \
	dump dumpfs fastboot fdisk fsck ft halt ifconfig md5 mknod mount \
	mount_isofs mount_pcfs mount_procfs mountd newfs nfsd nfsiod \
	ping quotacheck reboot restore route savecore scsi shutdown \
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
