# $FreeBSD$

LOCALEDIR=	${SHAREDIR}/locale
LC_FILES=	LC_COLLATE LC_CTYPE LC_MESSAGES LC_MONETARY LC_NUMERIC \
		LC_TIME

ALIASES=	zh_Hans_CN.GB18030 zh_CN.GB18030 \
		zh_Hans_CN.GB2312 zh_CN.GB2312 \
		zh_Hans_CN.GBK zh_CN.GBK \
		zh_Hans_CN.eucCN zh_CN.eucCN \
		zh_Hans_CN.UTF-8 zh_CN.UTF-8 \
		zh_Hant_HK.Big5HKSCS zh_HK.Big5HKSCS \
		zh_Hant_HK.UTF-8 zh_HK.UTF-8 \
		zh_Hant_TW.Big5 zh_TW.Big5 \
		zh_Hant_TW.UTF-8 zh_TW.UTF-8

.for from to in ${ALIASES}
.for f in ${LC_FILES}
SYMLINKS+=	../${from}/${f} ${LOCALEDIR}/${to}/${f}
.endfor
.endfor

.include <bsd.prog.mk>
