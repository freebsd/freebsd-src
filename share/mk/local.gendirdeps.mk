# $FreeBSD$

# we need a keyword, this noise is to prevent it being expanded
GENDIRDEPS_HEADER= echo '\# ${FreeBSD:L:@v@$$$v$$ @:M*F*}';

# supress optional/auto dependecies
# local.dirdeps.mk will put them in if necessary
GENDIRDEPS_FILTER+= \
	Nbin/cat.host \
	Ngnu/lib/libssp/libssp_nonshared \
	Ncddl/usr.bin/ctf* \
	Nlib/libc_nonshared \
	Ntargets/pseudo/stage* \
	Ntools/*

# Exclude toolchain which is handled special.
.if ${RELDIR:Mtargets*} == ""
.if ${RELDIR:Nusr.bin/clang/*:Ngnu/usr.bin/cc/*:Nlib/clang*} != ""
GENDIRDEPS_FILTER.host+= \
	Nusr.bin/clang/* \
	Ngnu/usr.bin/cc/* \

.endif
GENDIRDEPS_FILTER+= \
	Nlib/clang/include.host \
	Nusr.bin/addr2line.host \
	Nusr.bin/ar.host \
	Nusr.bin/clang/clang.host \
	Nusr.bin/elfcopy.host \
	Nusr.bin/elfdump.host \
	Nusr.bin/nm.host \
	Nusr.bin/readelf.host \
	Nusr.bin/size.host \
	Nusr.bin/strings.host \
	Nusr.bin/strip.host \
	Ngnu/usr.bin/cc* \
	Ngnu/usr.bin/binutils*.host \

.endif

GENDIRDEPS_FILTER+= ${GENDIRDEPS_FILTER.${MACHINE}:U}

# gendirdeps.mk will turn _{VAR} into ${VAR} which keeps this simple
# order of this list matters!
GENDIRDEPS_FILTER_DIR_VARS+= \
       CSU_DIR \
       BOOT_MACHINE_DIR

# order of this list matters!
GENDIRDEPS_FILTER_VARS+= \
       KERNEL_NAME \
       MACHINE_CPUARCH \
       MACHINE_ARCH \
       MACHINE

GENDIRDEPS_FILTER+= ${GENDIRDEPS_FILTER_DIR_VARS:@v@S,${$v},_{${v}},@}
GENDIRDEPS_FILTER+= ${GENDIRDEPS_FILTER_VARS:@v@S,/${$v}/,/_{${v}}/,@:NS,//,*:u}
