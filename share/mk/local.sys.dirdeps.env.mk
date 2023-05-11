
# For universe we want to potentially
# build for multiple MACHINE_ARCH per MACHINE
# so we need more than MACHINE in TARGET_SPEC
TARGET_SPEC_VARS?= MACHINE MACHINE_ARCH
 
# this is sufficient for most of the tree.
.MAKE.DEPENDFILE_DEFAULT = ${.MAKE.DEPENDFILE_PREFIX}

# but if we have a machine qualified file it should be used in preference
.MAKE.DEPENDFILE_PREFERENCE = \
	${.MAKE.DEPENDFILE_PREFIX}.${MACHINE} \
	${.MAKE.DEPENDFILE_PREFIX}

