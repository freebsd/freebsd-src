# We want to build some host tools (eg makefs, mkimg) for Linux
# This only gets included during DIRDEPS_BUILD when MACHINE is "host"
# or "host32"

CFLAGS+= -I${SRCTOP}/tools/build/cross-build/include/linux

WARNS= 0

.ifdef PROG
LOCAL_LIBRARIES+= bsd egacy
LIBADD+= egacy m
.endif

# Bring in the full GNU namespace
CFLAGS+= -D_GNU_SOURCE

# for sane staging behavior
LN= ln -L
