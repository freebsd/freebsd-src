.include <bsd.compat.pre.mk>

SCRIPTS=auto \
	adduser \
	bootconfig \
	checksum \
	config \
	docsinstall \
	entropy \
	fetchmissingdists \
	finalconfig \
	firmware \
	hardening \
	hostname \
	jail \
	keymap \
	mirrorselect \
	mount \
	netconfig \
	netconfig_ipv4 \
	netconfig_ipv6 \
	pkgbase \
	rootpass \
	script \
	services \
	time \
	umount \
	wlanconfig \
	zfsboot

BINDIR= ${LIBEXECDIR}/bsdinstall

MAN=

pkgbase: pkgbase.in
	sed "s|%%_ALL_libcompats%%|${_ALL_libcompats}|" < ${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>
