# $FreeBSD$

.include <src.opts.mk>

SUBDIR+=		libsa
.if ${MK_FORTH} != "no"
# Build the add-in FORTH interpreter.
SUBDIR+=		ficl
SUBDIR+=		forth
.endif
.if ${MK_LOADER_LUA} != "no"
SUBDIR+=		liblua
SUBDIR+=		lua
.endif

SUBDIR+=		defaults
SUBDIR+=		man

.include <bsd.arch.inc.mk>

.if ${MK_EFI} != "no" && ${MK_LOADER_EFI} != "no"
SUBDIR+=		efi
.endif

.if exists(${.CURDIR}/${MACHINE}/.)
SUBDIR+=		${MACHINE}
.endif

.include <bsd.subdir.mk>
