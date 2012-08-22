# supress optional dependecies
# local.dirdeps.mk will put them in if necessary
GENDIRDEPS_FILTER+= Ngnu/lib/libssp/libssp_nonshared

# gendirdeps.mk will turn _{VAR} into ${VAR} which keeps this simple
GENDIRDEPS_FILTER+= ${CSU_DIR:L:@v@S,/${$v},/_{${v}},@}

# this could easily get confused
GENDIRDEPS_FILTER+= ${MACHINE_CPUARCH MACHINE_CPU MACHINE_ARCH MACHINE:L:@v@S,/${$v}/,/_{${v}}/,@:NS,//,*:u}

