NO_OBJ=t

.include <bsd.init.mk>

SUBDIR.yes+=	libefi
SUBDIR.${MK_LOADER_IA32}+=	libefi32
SUBDIR.${MK_FDT}+=	fdt
SUBDIR.yes+=	.WAIT

SUBDIR.yes+=	boot1 gptboot

SUBDIR.${MK_FORTH}+= loader_4th
SUBDIR.${MK_LOADER_LUA}+= loader_lua
SUBDIR.${MK_LOADER_IA32}+= loader_ia32
SUBDIR.yes+=	loader_simp

.include <bsd.subdir.mk>
