NO_OBJ=t

.include <bsd.init.mk>

# Almost everything else here relies on btxldr, so we must make sure it's built
# before everything else proceeds so we don't end up building against a stale
# btxldr and ending up with a build-during-install scenario.
SUBDIR.yes+=	btx libi386
SUBDIR.yes+=	.WAIT

SUBDIR.yes+=	mbr pmbr boot0 boot0sio boot2 cdboot gptboot \
		isoboot

SUBDIR.${MK_FORTH}+= loader_4th
SUBDIR.${MK_LOADER_LUA}+= loader_lua
SUBDIR.yes+=	loader_simp

# special boot programs, 'self-extracting boot2+loader'
SUBDIR.${MK_LOADER_PXEBOOT}+=	pxeldr

SUBDIR.${MK_LOADER_ZFS}+=	gptzfsboot

.if defined(PXEBOOT_DEFAULT_INTERP)
L=${PXEBOOT_DEFAULT_INTERP}
.else
L=${LOADER_DEFAULT_INTERP}
.endif
SUBDIR_DEPEND_pxeldr+=	loader_${L}

.include <bsd.subdir.mk>
