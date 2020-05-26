# $FreeBSD$

# Find the highest version DDP package file and build a .ko for it
PKG_FILE != find ${SRCTOP}/sys/contrib/dev/ice -name 'ice-*.pkg' | sort -V | tail -1

.if empty(PKG_FILE)
.error Unable to locate the DDP package binary file
.endif

.info Found ${PKG_FILE}

PKG_NAME != basename ${PKG_FILE}
PKG_VER_STR != basename -s .pkg ${PKG_NAME}
PKG_VER_STR := ${PKG_VER_STR:S/^ice-//}
PKG_VER_STR := ${PKG_VER_STR:S/-signed$//}
PKG_VER_MAJ != echo ${PKG_VER_STR} | cut -d. -f1
PKG_VER_MIN != echo ${PKG_VER_STR} | cut -d. -f2
PKG_VER_UPD != echo ${PKG_VER_STR} | cut -d. -f3
PKG_VER_DFT != echo ${PKG_VER_STR} | cut -d. -f4
PKG_VERSION != printf "0x%02x%02x%02x%02x" "${PKG_VER_MAJ}" "${PKG_VER_MIN}" "${PKG_VER_UPD}" "${PKG_VER_DFT}"

KMOD := ice_ddp
FIRMWS := ${PKG_FILE}:ice_ddp:${PKG_VERSION}
.include <bsd.kmod.mk>
