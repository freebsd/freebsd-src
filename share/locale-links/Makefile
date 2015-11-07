# $FreeBSD$

LOCALEDIR=	${SHAREDIR}/locale

# We need to keep zh_CN.* around as aliases to zh_Hans_CN.* because some
# of the lang catalogs use zh_CN still (e.g. vi), plus people may expect it
# We won't alias zh_Hans_CN or zh_Hans_UTF8 though
.for CN in GB18030 GB2312 GBK UTF-8 eucCN
SYMLINKS+=	zh_Hans_CN.${CN} ${LOCALEDIR}/zh_CN.${CN}
.endfor

.for TW in Big5 UTF-8
SYMLINKS+=	zh_Hant_TW.${TW} ${LOCALEDIR}/zh_TW.${TW}
.endfor

.include <bsd.prog.mk>
