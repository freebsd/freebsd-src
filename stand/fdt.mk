
.if ${MK_FDT} == "yes"
CFLAGS+=	-I${FDTSRC}
CFLAGS+=	-I${BOOTOBJ}/fdt
CFLAGS+=	-I${SYSDIR}/contrib/libfdt
CFLAGS+=	-DLOADER_FDT_SUPPORT
LIBSAFDT=	${BOOTOBJ}/fdt/libfdt.a
.endif
