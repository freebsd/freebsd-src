# $FreeBSD$

.PATH:		${ZFSSRC}
.PATH:		${SYSDIR}/crypto/skein
.PATH:		${ZFSOSSRC}/spl
.PATH:		${OZFS}/module/zstd/lib/common
.PATH:		${OZFS}/module/zstd/lib/compress
.PATH:		${OZFS}/module/zstd/lib/decompress
ZFSSRC=		zfs.c nvlist.c skein.c skein_block.c list.c
ZFSSRC+=	zstd_shim.c
ZFSSRC+=	entropy_common.c error_private.c
ZFSSRC+=	fse_compress.c fse_decompress.c hist.c
ZFSSRC+=	huf_compress.c huf_decompress.c pool.c xxhash.c
ZFSSRC+=	zstd_common.c zstd_compress.c zstd_compress_literals.c
ZFSSRC+=	zstd_compress_sequences.c zstd_compress_superblock.c
ZFSSRC+=	zstd_ddict.c zstd_decompress.c zstd_decompress_block.c
ZFSSRC+=	zstd_double_fast.c zstd_fast.c zstd_lazy.c zstd_ldm.c
ZFSSRC+=	zstd_opt.c

CFLAGS+=	-DHAS_ZSTD_ZFS
SRCS+=		${ZFSSRC}

CFLAGS+=	-I${LDRSRC}
CFLAGS+=	-I${SYSDIR}/cddl/boot/zfs
CFLAGS+=	-I${SYSDIR}/crypto/skein

ZFS_EARLY=	-I${ZFSOSINC}					\
		-I${ZFSOSINC}/spl				\
		-I${ZFSOSINC}/zfs

.for i in ${ZFSSRC}
CFLAGS.$i+=	-include ${ZFSOSINC}/spl/sys/ccompile.h
.endfor

CFLAGS_EARLY.list.c+= ${ZFS_EARLY}
CFLAGS_EARLY.zstd_shim.c+= ${ZFS_EARLY}

# Can't use the early flags because there's two conflicting definitions of boolean_t in
# the zfs code that need to be unified.
CFLAGS.nvlist.c+= -I${ZFSOSINC}/spl
CFLAGS.zfs.c+=	-I${ZFSOSINC}/spl 				\
		-I${SRCTOP}/sys/cddl/contrib/opensolaris/common/lz4
CFLAGS.entropy_common.c=	-U__BMI__
CFLAGS.entropy_common.c+=	${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.error_private.c=		-U__BMI__
CFLAGS.error_private.c+=	${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.fse_compress.c=		-U__BMI__
CFLAGS.fse_compress.c+=		${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.fse_decompress.c=	-U__BMI__
CFLAGS.fse_decompress.c+=	${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.hist.c=			-U__BMI__
CFLAGS.hist.c+=			${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.huf_compress.c=		-U__BMI__
CFLAGS.huf_compress.c+=		${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.huf_decompress.c=	-U__BMI__
CFLAGS.huf_decompress.c+=	${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.pool.c=			-U__BMI__
CFLAGS.pool.c+=			${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.xxhash.c=		-U__BMI__
CFLAGS.xxhash.c+=		${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_common.c=		-U__BMI__
CFLAGS.zstd_common.c+=		${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_compress.c=		 -U__BMI__
CFLAGS.zstd_compress.c+=	${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_compress_literals.c=	-U__BMI__
CFLAGS.zstd_compress_literals.c+=	${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_compress_sequences.c=	-U__BMI__
CFLAGS.zstd_compress_sequences.c+=	${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_compress_superblock.c=	-U__BMI__
CFLAGS.zstd_compress_superblock.c+=	${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_double_fast.c=	-U__BMI__
CFLAGS.zstd_double_fast.c+=	${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_fast.c=		-U__BMI__
CFLAGS.zstd_fast.c+=		${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_lazy.c=		-U__BMI__
CFLAGS.zstd_lazy.c+=		${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_ldm.c=		-U__BMI__
CFLAGS.zstd_ldm.c+=		${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_opt.c=		-U__BMI__
CFLAGS.zstd_opt.c+=		${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_ddict.c=		-U__BMI__
CFLAGS.zstd_ddict.c+=		${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_decompress.c=	-U__BMI__
CFLAGS.zstd_decompress.c+=	${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_decompress_block.c=	-U__BMI__
CFLAGS.zstd_decompress_block.c+=	${NO_WBITWISE_INSTEAD_OF_LOGICAL}
CFLAGS.zstd_shim.c+= -DIN_BASE -I${OZFS}/include

# Do not unroll skein loops, reduce code size
CFLAGS.skein_block.c+=	-DSKEIN_LOOP=111

CFLAGS+=	-I${SYSDIR}/contrib/openzfs/include
CFLAGS+=	-I${SYSDIR}/contrib/openzfs/include/os/freebsd/zfs
CFLAGS.zfs.c+=	-I${SYSDIR}/cddl/contrib/opensolaris/common/lz4

CFLAGS+=	-Wformat -Wall
