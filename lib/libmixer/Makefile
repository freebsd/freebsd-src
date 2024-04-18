
PACKAGE=	lib${LIB}
LIB=		mixer
SRCS=		${LIB}.c
INCS=		${LIB}.h
MAN=		${LIB}.3
VERSION_DEF=	${LIBCSRCDIR}/Versions.def
SYMBOL_MAPS=	${.CURDIR}/Symbol.map

MLINKS+=	mixer.3 mixer_open.3
MLINKS+=	mixer.3 mixer_close.3
MLINKS+=	mixer.3 mixer_get_dev.3
MLINKS+=	mixer.3 mixer_get_dev_byname.3
MLINKS+=	mixer.3 mixer_add_ctl.3
MLINKS+=	mixer.3 mixer_add_ctl_s.3
MLINKS+=	mixer.3 mixer_remove_ctl.3
MLINKS+=	mixer.3 mixer_get_ctl.3
MLINKS+=	mixer.3 mixer_get_ctl_byname.3
MLINKS+=	mixer.3 mixer_set_vol.3
MLINKS+=	mixer.3 mixer_set_mute.3
MLINKS+=	mixer.3 mixer_mod_recsrc.3
MLINKS+=	mixer.3 mixer_get_dunit.3
MLINKS+=	mixer.3 mixer_set_dunit.3
MLINKS+=	mixer.3 mixer_get_mode.3
MLINKS+=	mixer.3 mixer_get_nmixers.3
MLINKS+=	mixer.3 MIX_ISDEV.3
MLINKS+=	mixer.3 MIX_ISMUTE.3
MLINKS+=	mixer.3 MIX_ISREC.3
MLINKS+=	mixer.3 MIX_ISRECSRC.3
MLINKS+=	mixer.3 MIX_VOLNORM.3
MLINKS+=	mixer.3 MIX_VOLDENORM.3

.include <bsd.lib.mk>
