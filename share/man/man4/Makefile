#	@(#)Makefile	8.1 (Berkeley) 6/18/93

MAN4=	bpf.4 ccd.4 cd.4 ch.4 ddb.4 divert.4 drum.4 fd.4 fpa.4 \
	icmp.4 ifmib.4 inet.4 intro.4 ip.4 ipfirewall.4 \
	lkm.4 lo.4 netintro.4 \
	null.4 od.4 ppp.4 pt.4 pty.4 route.4 \
	scsi.4 sd.4 sl.4 smp.4 snp.4 sppp.4 ssc.4 st.4 su.4 tcp.4 \
	ttcp.4 termios.4 tty.4 tun.4  udp.4 uk.4 update.4 unix.4 vn.4 \
	worm.4 yp.4 zero.4

MLINKS+=fd.4 stderr.4 fd.4 stdin.4 fd.4 stdout.4
MLINKS+=netintro.4 networking.4
MLINKS+=ipfirewall.4 ipacct.4 ipfirewall.4 ipfw.4 ipfirewall.4 ipaccounting.4
MLINKS+=fpa.4 fea.4
MLINKS+=yp.4 YP.4 yp.4 nis.4 yp.4 NIS.4
MLINKS+=smp.4 SMP.4

# XXX NOT IMPORTED:	man4.hp300 man4.sparc man4.tahoe man4.vax
SUBDIR=	 man4.i386 
.if make(maninstall)
maninstall:: _SUBDIR
.endif

.include <bsd.prog.mk>


