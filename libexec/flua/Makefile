.include <src.lua.mk>

PACKAGE=	flua

# New flua modules should be added here rather than to SUBDIR so that we can do
# the right thing for both bootstrap flua and target flua.  The former does not
# do any shared libs, so we just build them all straight into flua itself rather
# than mucking about with the infrastructure to make them linkable -- thus, why
# these are all structured to have a Makefile that describes what we want
# *installed*, and a Makefile.inc that describes what we need to *build*.
FLUA_MODULES+=	lfbsd
FLUA_MODULES+=	lfs
FLUA_MODULES+=	libhash
.ifndef BOOTSTRAPPING
# Bootstrap flua can't usefully do anything with libjail anyways, because it
# can't assume it's being run on a system that even supports jails.
FLUA_MODULES+=	libjail
.endif
FLUA_MODULES+=	libucl
FLUA_MODULES+=	liblyaml

.ifdef BOOTSTRAPPING
# libfreebsd is generally omitted from the bootstrap flua because its
# functionality largely assumes a FreeBSD kernel/system headers, so it doesn't
# really offer functionality that we can use in bootstrap.
CFLAGS+=	-I${.CURDIR} -DBOOTSTRAPPING

SHAREDIR=	${WORLDTMP}/legacy/usr/share/flua
FLUA_PATH=	${SHAREDIR}/?.lua;${SHAREDIR}/?/init.lua
CFLAGS+=	-DBOOTSTRAP_FLUA_PATH=\"${FLUA_PATH:Q}\"

.for mod in ${FLUA_MODULES}
.include "${mod}/Makefile.inc"
.endfor

.else

FLUA_MODULES+=	libfreebsd
SUBDIR+=	${FLUA_MODULES}

.endif

LUASRC?=	${SRCTOP}/contrib/lua/src
.PATH: ${LUASRC}

PROG=	flua
WARNS?=	3

CWARNFLAGS.gcc+=	-Wno-format-nonliteral

LIBADD+=	lua

# Entry point
SRCS+=	lua.c

# FreeBSD Extensions
.PATH: ${.CURDIR}/modules
SRCS+=	linit_flua.c
SRCS+=	lposix.c

CFLAGS+=	-I${SRCTOP}/lib/liblua -I${.CURDIR}/modules -I${LUASRC}
CFLAGS+=	-DLUA_PROGNAME="\"${PROG}\""

# readline bits; these aren't needed if we're building a bootstrap flua, as we
# don't expect that one to see any REPL usage.
.if !defined(BOOTSTRAPPING)
CFLAGS+=	-DLUA_USE_READLINE
CFLAGS+=	-I${SRCTOP}/lib/libedit -I${SRCTOP}/contrib/libedit
LIBADD+=	edit
LDFLAGS+=	-Wl,-E
.endif

.include <bsd.prog.mk>
