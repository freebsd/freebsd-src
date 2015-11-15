# $FreeBSD$

LOCALEDIR=	${SHAREDIR}/locale
LC_FILES=	LC_COLLATE LC_CTYPE LC_MESSAGES LC_MONETARY LC_NUMERIC \
		LC_TIME

# We need to keep zh_CN.* around as aliases to zh_Hans_CN.* because some
# of the lang catalogs use zh_CN still (e.g. vi), plus people may expect it
# We won't alias zh_Hans_CN or zh_Hans_UTF8 though
.for CN in GB18030 GB2312 GBK UTF-8 eucCN
.for f in ${LC_FILES}
SYMLINKS+=	zh_Hans_CN.${CN}/${f} ${LOCALEDIR}/zh_CN.${CN}/${f}
.endfor
.endfor

.for HK in Big5HKSCS UTF-8
.for f in ${LC_FILES}
SYMLINKS+=	zh_Hant_HK.${HK}/${f} ${LOCALEDIR}/zh_HK.${HK}/${f}
.endfor
.endfor

.for TW in Big5 UTF-8
.for f in ${LC_FILES}
SYMLINKS+=	zh_Hant_TW.${TW}/${f} ${LOCALEDIR}/zh_TW.${TW}/${f}
.endfor
.endfor

.include <bsd.prog.mk>
