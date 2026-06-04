# SoC Framework Makefile
# FreeBSD Mobile Project

.PATH: ${SRCDIR}

KERN_DIR = ${.CURDIR}/..

CFLAGS += -I${KERN_DIR} -I${.CURDIR} -Wall -Wformat=2 -Wunused-parameter

LIB = soc

SRCS = soc.c soc_common.c mediatek.c qualcomm.c samsung.c google.c apple.c

# Header files
SRCS += soc.h soc_common.h mediatek.h qualcomm.h samsung.h google.h apple.h

# Compile as kernel module or library
# For kernel module:
# KMOD = soc

# For static library:
# SHLIB_NAME = soc

.include <bsd.kbuild.mk>
.include <bsd.subdir.mk>