NO_OBJ=t

.include <bsd.init.mk>

SUBDIR.yes+=	libefi
SUBDIR.${MK_LOADER_IA32}+=	libsa32efi libefi32
SUBDIR.${MK_FDT}+=	fdt

.if ${LOADER_DEFAULT_INTERP} == "lua"
SUBDIR.${MK_LOADER_IA32}+=	liblua32efi
.elif ${LOADER_DEFAULT_INTERP} == "4th"
SUBDIR.${MK_LOADER_IA32}+=	ficl32efi
.endif

SUBDIR.yes+=	.WAIT

SUBDIR.yes+=	boot1 gptboot

SUBDIR.${MK_FORTH}+= loader_4th
SUBDIR.${MK_LOADER_LUA}+= loader_lua
SUBDIR.${MK_LOADER_IA32}+= loader_ia32
SUBDIR.yes+=	loader_simp

.include <bsd.subdir.mk>
