# $FreeBSD$
#
# Doing a make install builds /usr/share/examples

.include <bsd.own.mk>

LDIRS=	BSD_daemon \
	FreeBSD_version \
	IPv6 \
	bootforth \
	cvsup \
	diskless \
	drivers \
	etc \
	find_interface \
	ibcs2 \
	ipfw \
	isdn \
	kld \
	libvgl \
	mdoc \
	netgraph \
	nwclient \
	perfmon \
	portal \
	ppi \
	ppp \
	pppd \
	printing \
	ses \
	scsi_target \
	slattach \
	sliplogin \
	startslip \
	sunrpc

# Disabled in 7.0 as netatm is not MPSAFE.
#LDIRS+=	atm

XFILES=	BSD_daemon/FreeBSD.pfa \
	BSD_daemon/README \
	BSD_daemon/beastie.eps \
	BSD_daemon/beastie.fig \
	BSD_daemon/eps.patch \
	BSD_daemon/poster.sh \
	FreeBSD_version/FreeBSD_version.c \
	FreeBSD_version/Makefile \
	FreeBSD_version/README \
	IPv6/USAGE \
	bootforth/README \
	bootforth/boot.4th \
	bootforth/frames.4th \
	bootforth/loader.rc \
	bootforth/menu.4th \
	bootforth/menuconf.4th \
	bootforth/screen.4th \
	cvsup/README \
	cvsup/cvs-supfile \
	cvsup/doc-supfile \
	cvsup/gnats-supfile \
	cvsup/ports-supfile \
	cvsup/refuse \
	cvsup/refuse.README \
	cvsup/stable-supfile \
	cvsup/standard-supfile \
	cvsup/www-supfile \
	diskless/ME \
	diskless/README.BOOTP \
	diskless/README.TEMPLATING \
	diskless/clone_root \
	drivers/README \
	drivers/make_device_driver.sh \
	drivers/make_pseudo_driver.sh \
	etc/README.examples \
	etc/bsd-style-copyright \
	etc/make.conf \
	find_interface/Makefile \
	find_interface/README \
	find_interface/find_interface.c \
	ibcs2/README \
	ibcs2/hello.uu \
	ipfw/change_rules.sh \
	isdn/FAQ \
	isdn/KERNEL \
	isdn/Overview \
	isdn/README \
	isdn/ROADMAP \
	isdn/ReleaseNotes \
	isdn/Resources \
	isdn/SupportedCards \
	isdn/ThankYou \
	isdn/contrib/README \
	isdn/contrib/anleitung.ppp \
	isdn/contrib/answer.c \
	isdn/contrib/answer.sh \
	isdn/contrib/convert.sh \
	isdn/contrib/hplay.c \
	isdn/contrib/i4b-ppp-newbie.txt \
	isdn/contrib/isdnctl \
	isdn/contrib/isdnd_acct \
	isdn/contrib/isdnd_acct.pl \
	isdn/contrib/isdntelmux.c \
	isdn/contrib/mrtg-isp0.sh \
	isdn/i4brunppp/Makefile \
	isdn/i4brunppp/README \
	isdn/i4brunppp/i4brunppp-isdnd.rc \
	isdn/i4brunppp/i4brunppp.8 \
	isdn/i4brunppp/i4brunppp.c \
	isdn/v21/Makefile \
	isdn/v21/README \
	isdn/v21/v21modem.c \
	kld/Makefile \
	kld/cdev/Makefile \
	kld/cdev/README \
	kld/cdev/module/Makefile \
	kld/cdev/module/cdev.c \
	kld/cdev/module/cdev.h \
	kld/cdev/module/cdevmod.c \
	kld/cdev/test/Makefile \
	kld/cdev/test/testcdev.c \
	kld/dyn_sysctl/Makefile \
	kld/dyn_sysctl/README \
	kld/dyn_sysctl/dyn_sysctl.c \
	kld/syscall/Makefile \
	kld/syscall/module/Makefile \
	kld/syscall/module/syscall.c \
	kld/syscall/test/Makefile \
	kld/syscall/test/call.c \
	libvgl/Makefile \
	libvgl/demo.c \
	mdoc/POSIX-copyright \
	mdoc/deshallify.sh \
	mdoc/example.1 \
	mdoc/example.3 \
	mdoc/example.4 \
	mdoc/example.9 \
	netgraph/ether.bridge \
	netgraph/frame_relay \
	netgraph/ngctl \
	netgraph/raw \
	netgraph/udp.tunnel \
	nwclient/dot.nwfsrc \
	nwclient/nwfs.sh.sample \
	perfmon/Makefile \
	perfmon/README \
	perfmon/perfmon.c \
	portal/README \
	portal/portal.conf \
	ppi/Makefile \
	ppi/ppilcd.c \
	ppp/chap-auth \
	ppp/isdnd.rc \
	ppp/login-auth \
	ppp/ppp.conf.isdn \
	ppp/ppp.conf.sample \
	ppp/ppp.conf.span-isp \
	ppp/ppp.conf.span-isp.working \
	ppp/ppp.linkdown.sample \
	ppp/ppp.linkdown.span-isp \
	ppp/ppp.linkdown.span-isp.working \
	ppp/ppp.linkup.sample \
	ppp/ppp.linkup.span-isp \
	ppp/ppp.linkup.span-isp.working \
	ppp/ppp.secret.sample \
	ppp/ppp.secret.span-isp \
	ppp/ppp.secret.span-isp.working \
	pppd/auth-down.sample \
	pppd/auth-up.sample \
	pppd/chap-secrets.sample \
	pppd/chat.sh.sample \
	pppd/ip-down.sample \
	pppd/ip-up.sample \
	pppd/options.sample \
	pppd/pap-secrets.sample \
	pppd/ppp.deny.sample \
	pppd/ppp.shells.sample \
	printing/README \
	printing/diablo-if-net \
	printing/hpdf \
	printing/hpif \
	printing/hpof \
	printing/hprf \
	printing/hpvf \
	printing/if-simple \
	printing/if-simpleX \
	printing/ifhp \
	printing/make-ps-header \
	printing/netprint \
	printing/psdf \
	printing/psdfX \
	printing/psif \
	printing/pstf \
	printing/pstfX \
	ses/Makefile \
	ses/Makefile.inc \
	ses/getencstat/Makefile \
	ses/getencstat/getencstat.0 \
	ses/sesd/Makefile \
	ses/sesd/sesd.0 \
	ses/setencstat/Makefile \
	ses/setencstat/setencstat.0 \
	ses/setobjstat/Makefile \
	ses/setobjstat/setobjstat.0 \
	ses/srcs/chpmon.c \
	ses/srcs/eltsub.c \
	ses/srcs/getencstat.c \
	ses/srcs/getnobj.c \
	ses/srcs/getobjmap.c \
	ses/srcs/getobjstat.c \
	ses/srcs/inienc.c \
	ses/srcs/sesd.c \
	ses/srcs/setencstat.c \
	ses/srcs/setobjstat.c \
	scsi_target/Makefile \
	scsi_target/scsi_target.c \
	scsi_target/scsi_target.h \
	scsi_target/scsi_target.8 \
	scsi_target/scsi_cmds.c \
	slattach/unit-command.sh \
	sliplogin/slip.hosts \
	sliplogin/slip.login \
	sliplogin/slip.logout \
	sliplogin/slip.slparms \
	startslip/sldown.sh \
	startslip/slip.sh \
	startslip/slup.sh \
	sunrpc/Makefile \
	sunrpc/dir/Makefile \
	sunrpc/dir/dir.x \
	sunrpc/dir/dir_proc.c \
	sunrpc/dir/rls.c \
	sunrpc/msg/Makefile \
	sunrpc/msg/msg.x \
	sunrpc/msg/msg_proc.c \
	sunrpc/msg/printmsg.c \
	sunrpc/msg/rprintmsg.c \
	sunrpc/sort/Makefile \
	sunrpc/sort/rsort.c \
	sunrpc/sort/sort.x \
	sunrpc/sort/sort_proc.c

# Disabled in 7.0 as netatm is not MPSAFE.
#XFILES+=	atm/NOTES \
#	atm/README \
#	atm/Startup \
#	atm/atm-config.sh \
#	atm/atm-sockets.txt \
#	atm/cpcs-design.txt \
#	atm/fore-microcode.txt \
#	atm/sscf-design.txt \
#	atm/sscop-design.txt

BINDIR= ${SHAREDIR}/examples

NO_OBJ=

# Define SHARED to indicate whether you want symbolic links to the system
# source (``symlinks''), or a separate copy (``copies''); (latter useful
# in environments where it's not possible to keep /sys publicly readable)
SHARED?=	copies

beforeinstall: ${SHARED} etc-examples
.ORDER: ${SHARED} etc-examples

copies:
.for i in ${LDIRS}
	if [ -L ${DESTDIR}${BINDIR}/$i ]; then \
		rm -f ${DESTDIR}${BINDIR}/$i; \
	fi
.endfor
	mtree -deU ${MTREE_FOLLOWS_SYMLINKS} \
	    -f ${.CURDIR}/../../etc/mtree/BSD.usr.dist -p ${DESTDIR}/usr
.for file in ${XFILES}
	${INSTALL} -o ${SHAREOWN} -g ${SHAREGRP} -m ${SHAREMODE} \
		${.CURDIR}/${file} ${DESTDIR}${BINDIR}/${file}
.endfor

symlinks:
.for i in ${LDIRS}
	rm -rf ${DESTDIR}${BINDIR}/$i
	ln -s ${.CURDIR}/$i ${DESTDIR}${BINDIR}/$i
.endfor

etc-examples:
.if ${SHARED} != "symlinks"
	(cd ${.CURDIR}/../../etc; ${MAKE} etc-examples)
.endif

.if ${SHARED} != "symlinks"
SUBDIR=	smbfs
.if ${MK_IPFILTER} != "no"
SUBDIR+=ipfilter
.endif
.if ${MK_PF} != "no"
SUBDIR+=pf
.endif
.endif

.include <bsd.subdir.mk>
