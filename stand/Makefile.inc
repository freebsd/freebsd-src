SUBDIR_PARALLEL=	yes

PACKAGE=		bootloader

# Firmware may not be able to handle branch protection failures
MK_BRANCH_PROTECTION=	no

.include "defs.mk"
