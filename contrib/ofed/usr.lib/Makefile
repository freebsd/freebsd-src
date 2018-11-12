SUBDIR=	libibcommon libibmad libibumad libibverbs libmlx4 libmthca \
	libopensm libosmcomp libosmvendor libibcm librdmacm libsdp libcxgb4

SUBDIR_DEPEND_libcxgb4=	libibverbs
SUBDIR_DEPEND_libibcm=	libibverbs
SUBDIR_DEPEND_libibmad= libibcommon libibumad
SUBDIR_DEPEND_libibumad= libibcommon
SUBDIR_DEPEND_libmlx4=	libibverbs
SUBDIR_DEPEND_libmthca=	libibverbs
SUBDIR_DEPEND_libosmvendor=	libibumad libopensm libosmcomp
SUBDIR_DEPEND_librdmacm=	libibverbs
SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
