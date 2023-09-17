
# we need a keyword, this noise is to prevent it being expanded
GENDIRDEPS_HEADER= echo '\# ${FreeBSD:L:@v@$$$v$$ @:M*F*}';

# suppress optional/auto dependencies
# local.dirdeps.mk will put them in if necessary
GENDIRDEPS_FILTER+= \
	Nbin/cat.host \
	Nlib/libssp_nonshared \
	Ncddl/usr.bin/ctf* \
	Nlib/libc_nonshared \
	Nlib/libgcc_eh \
	Nlib/libgcc_s \
	Nstand/libsa/* \
	Nstand/libsa32/* \
	Ntargets/pseudo/stage* \
	Ntools/*

# Clang has nested directories in its OBJDIR.
GENDIRDEPS_FILTER+= C,(lib/clang/lib[^/]*)/.*,\1,

# Exclude toolchain which is handled special.
.if ${RELDIR:Mtargets*} == ""
.if ${RELDIR:Nusr.bin/clang/*:Ngnu/usr.bin/cc/*:Nlib/clang*} != ""
GENDIRDEPS_FILTER.host+= \
	Nusr.bin/clang/* \
	Ngnu/usr.bin/cc/* \

.endif
GENDIRDEPS_FILTER_HOST_TOOLS+= \
	Nlib/clang/headers \
	Nusr.bin/addr2line \
	Nusr.bin/ar \
	Nusr.bin/clang/clang \
	Nusr.bin/elfcopy \
	Nusr.bin/elfdump \
	Nusr.bin/nm \
	Nusr.bin/readelf \
	Nusr.bin/size \
	Nusr.bin/strings \
	Nusr.bin/strip \
	Ngnu/usr.bin/cc* \
	Ngnu/usr.bin/binutils* \

.if ${MACHINE} != "host"
GENDIRDEPS_FILTER+=	${GENDIRDEPS_FILTER_HOST_TOOLS:C,$,.host,}
.else
GENDIRDEPS_FILTER+=	${GENDIRDEPS_FILTER_HOST_TOOLS}
.endif
.endif

GENDIRDEPS_FILTER+= ${GENDIRDEPS_FILTER.${MACHINE}:U}

# gendirdeps.mk will turn _{VAR} into ${VAR} which keeps this simple
# see local.meta.sys.mk for GENDIRDEPS_FILTER_DIR_VARS and
# GENDIRDEPS_FILTER_VARS

# avoid churn for now
LOCAL_DEPENDS_GUARD= _{DEP_RELDIR} == _{_DEP_RELDIR}

.-include <site.gendirdeps.mk>
