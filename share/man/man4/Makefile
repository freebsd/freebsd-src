#	@(#)Makefile	8.1 (Berkeley) 6/18/93
# $FreeBSD$

MAN4=	ahc.4 al.4 alpm.4 atkbd.4 atkbdc.4 ax.4 blackhole.4 bpf.4 \
	bridge.4 ccd.4 cd.4 ch.4 da.4 ddb.4 de.4 \
	divert.4 dm.4 drum.4 dummynet.4 fd.4 fdc.4 fpa.4 fxp.4 \
	icmp.4 ifmib.4 iic.4 iicbb.4 iicbus.4 iicsmb.4 \
	inet.4 intpm.4 intro.4 ip.4 ipfirewall.4 keyboard.4 kld.4 \
	lo.4 lp.4 lpbb.4 lpt.4 mem.4 mouse.4 mtio.4 mx.4 natm.4 ncr.4 \
	netintro.4 null.4 pass.4 ppbus.4 ppi.4 ppp.4 pt.4 pty.4 \
	route.4 ohci.4 pcm.4 pcvt.4 pn.4 psm.4 rl.4 sa.4 screen.4 scsi.4 \
	sd.4 sf.4 si.4 sio.4 sis.4 sk.4 sl.4 smb.4 smbus.4 smp.4 snp.4 \
	splash.4 sppp.4 ssc.4 st.4 ste.4 su.4 syscons.4 sysmouse.4 tcp.4 \
	termios.4 ti.4 tl.4 ttcp.4 tty.4 tun.4 udp.4 uhci.4 uk.4 ukbd.4 \
	umass.4 ums.4 unix.4 update.4 usb.4 vga.4 vinum.4 vn.4 \
	vpo.4 vr.4 wb.4 worm.4 wst.4 xl.4 xpt.4 yp.4 zero.4 

MLINKS=	fd.4 stderr.4 fd.4 stdin.4 fd.4 stdout.4
MLINKS+=fpa.4 fea.4
MLINKS+=ipfirewall.4 ipacct.4 ipfirewall.4 ipaccounting.4 ipfirewall.4 ipfw.4
MLINKS+=lp.4 plip.4
MLINKS+=mem.4 kmem.4
MLINKS+=netintro.4 networking.4
MLINKS+=pcvt.4 vt.4
MLINKS+=scsi.4 cam.4
MLINKS+=splash.4 screensaver.4
MLINKS+=smp.4 SMP.4
MLINKS+=syscons.4 sc.4
MLINKS+=vpo.4 imm.4
MLINKS+=yp.4 YP.4 yp.4 NIS.4 yp.4 nis.4

# XXX NOT IMPORTED:	man4.hp300 man4.sparc man4.tahoe man4.vax
SUBDIR=	 man4.i386 
.if make(maninstall)
maninstall:: _SUBDIR
.endif

.include <bsd.prog.mk>
