# $FreeBSD$

NO_OBJ=t

.include <bsd.init.mk>

SUBDIR.yes=	mbr pmbr boot0 boot0sio btx boot2 cdboot gptboot \
		isoboot libi386

SUBDIR.${MK_LOADER_FIREWIRE}+=	libfirewire

SUBDIR.${MK_FORTH}+= loader_4th
SUBDIR.${MK_LOADER_LUA}+= loader_lua
SUBDIR.yes+=	loader_simp

# special boot programs, 'self-extracting boot2+loader'
SUBDIR.yes+=	pxeldr

SUBDIR.${MK_LOADER_ZFS}+=	zfsboot gptzfsboot

.include <bsd.subdir.mk>
