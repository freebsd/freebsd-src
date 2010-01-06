# $FreeBSD$

PROG=	ac
MAN=	ac.8

# Temporary, while tracking down problem wrt 64-bit time_t's on sparc64
.if ${MACHINE_ARCH} == "sparc64"
CFLAGS+=-DDEBUG
.endif

# If "CONSOLE_TTY" is not defined, this program is compatible with the
# traditional implementation (using SunOS 4.x as the sample traditional
# implementation).  This is the default.
#
# If "CONSOLE_TTY" is defined, it must be defined to the appropriate
# console name, e.g. "vga".  Additionally, the various commented-out
# sections of the man page should be uncommented.  This is not the
# default because of the inability to detect the proper console name
# easily, especially on m68k systems, which can share binaries.
#
#CFLAGS+=-DCONSOLE_TTY=\"vga\"

.include <bsd.prog.mk>
