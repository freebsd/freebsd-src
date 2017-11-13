# $FreeBSD$

SUBDIR_A_DEPEND= ${_libthr}
SUBDIR_A= \
include

SUBDIR_B_DEPEND= ${SUBDIR_A}
SUBDIR_B= \
libibverbs

SUBDIR_C_DEPEND= ${SUBDIR_B}
SUBDIR_C= \
libibcm \
libibumad

SUBDIR_D_DEPEND= ${SUBDIR_C}
SUBDIR_D= \
librdmacm \
libibmad \
opensm/complib \
opensm/libvendor \
libcxgb4 \
libmlx4 \
libmlx5

SUBDIR_E_DEPEND= ${SUBDIR_D}
SUBDIR_E= \
libibverbs/examples/build \
librdmacm/examples/build \
libibnetdisc \
opensm/libopensm

SUBDIR_F_DEPEND= ${SUBDIR_E}
SUBDIR_F= \
opensm/opensm \
infiniband-diags/build

.for L in A B C D E F
.for S in ${SUBDIR_${L}}
SUBDIR_DEPEND_${S}+= ${SUBDIR_${L}_DEPEND}
.endfor
SUBDIR+= ${SUBDIR_${L}}
.endfor

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
