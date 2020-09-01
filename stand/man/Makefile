# $FreeBSD$

.include <bsd.init.mk>

.if ${MK_EFI} != "no"
MAN+=	boot1.efi.8
.endif
MAN+=	loader.8
.if ${MK_EFI} != "no"
MAN+=	loader.efi.8
.endif

.include <bsd.prog.mk>
