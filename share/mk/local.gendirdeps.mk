# supress optional/auto dependecies
# local.dirdeps.mk will put them in if necessary
GENDIRDEPS_FILTER+= \
	Ngnu/lib/libssp/libssp_nonshared \
	Ncddl/usr.bin/ctf* \
	Nlib/clang/include \
	Nlib/libc_nonshared \
	Ntargets/pseudo/stage* \
	Ntools/*

.if ${RELDIR:Mtargets*} == ""
.if ${RELDIR:Nusr.bin/clang/*:Ngnu/usr.bin/cc/*:Nlib/clang*} != ""
GENDIRDEPS_FILTER.host+= \
	Nusr.bin/clang/* \
	Ngnu/usr.bin/cc/* \

.endif
GENDIRDEPS_FILTER+= \
	Nusr.bin/clang/clang.host \
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

# handle the non-standard way that gnu/usr.bin/groff/tmac is staged
GENDIRDEPS_FILTER+= C,.*usr/share/tmac.*stage,gnu/usr.bin/groff/tmac,

