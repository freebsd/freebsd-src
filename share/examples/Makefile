# $FreeBSD$
#
# Doing a make install builds /usr/share/examples

.include <src.opts.mk>

LDIRS=	BSD_daemon \
	FreeBSD_version \
	IPv6 \
	bootforth \
	csh \
	diskless \
	drivers \
	etc \
	find_interface \
	ibcs2 \
	indent \
	ipfw \
	jails \
	kld \
	libvgl \
	mdoc \
	netgraph \
	perfmon \
	ppi \
	ppp \
	printing \
	ses \
	scsi_target \
	sunrpc \
	uefisign

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
	csh/dot.cshrc \
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
	indent/indent.pro \
	ipfw/change_rules.sh \
	jails/README \
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
	kld/firmware/Makefile \
	kld/firmware/README \
	kld/firmware/fwconsumer/Makefile \
	kld/firmware/fwconsumer/fw_consumer.c \
	kld/firmware/fwimage/Makefile \
	kld/firmware/fwimage/firmware.img.uu \
	kld/khelp/Makefile \
	kld/khelp/README \
	kld/khelp/h_example.c \
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
	netgraph/virtual.chain \
	netgraph/virtual.lan \
	perfmon/Makefile \
	perfmon/README \
	perfmon/perfmon.c \
	ppi/Makefile \
	ppi/ppilcd.c \
	ppp/chap-auth \
	ppp/login-auth \
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
	ses/srcs/eltsub.h \
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
	sunrpc/sort/sort_proc.c \
	uefisign/uefikeys

BINDIR= ${SHAREDIR}/examples

.if ${MK_HAST} != "no"
LDIRS+=		hast
XFILES+=	hast/ucarp.sh \
		hast/ucarp_down.sh \
		hast/ucarp_up.sh \
		hast/vip-down.sh \
		hast/vip-up.sh
.endif

.if ${MK_USB} != "no"
LDIRS+=		libusb20
XFILES+=	libusb20/Makefile \
		libusb20/README \
		libusb20/util.c \
		libusb20/util.h \
		libusb20/bulk.c \
		libusb20/control.c
.endif

.if ${MACHINE_CPUARCH} == "amd64"
.if ${MK_BHYVE} != "no"
LDIRS+=		bhyve
XFILES+=	bhyve/vmrun.sh
.endif
.endif

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

.if ${MK_TESTS} != "no"
SUBDIR+=tests
.endif

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
