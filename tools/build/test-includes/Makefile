#
# A number of header files in sys/* have, going back to 7th Edition Unix in
# 1979, reqiured other files (like sys/types.h) to compile. Likewise the 4BSD
# networking code has had prerequisites. However, going back to around the turn
# of the century, other systems have made them be independently includable
# (wide-spread header include protection post-dates 7th edition Unix by maybe 4
# or 5 years judging from netnews sources). Start down the path of making them
# all independently includable by creating this test that fails buildworld when
# they are not.
#
# The _foo.h files are excluded as they are internal-only headers and/or have
# special requirements. Since the purpose of this is to allow autoconfig
# programs to detect FreeBSD has different headers, omitting internal
# headers cuts down on the noise w/o causing problems for the goal.
#
LIB=	test-includes
INTERNALLIB=	This is a compile-only test
MAN=
HDRS!=	(cd ${SRCTOP}/sys; ls sys/[!_]*.h | sort ; ls net*/[!_]*.h | sort)
NO_PIC=	yes

# Some files have to be clean for extra defines too...
CFLAGS.event.c=	-D_WANT_KEVENT32 -D_WANT_FREEBSD11_KEVENT

# We currently support C89 and newer compilers. Catch the odd little fussy
# details like __inline that the header files must follow to support C89.
CFLAGS+=-ansi

.include "badfiles.inc"

.for h c in ${HDRS:@x@$x ${x:S,/,_,g:R}.c@}
.if !${BADHDRS:M${h}}
SRCS+= $c
CLEANFILES+=$c
$c:
	echo "#include <$h>" > ${.TARGET}
.endif
.endfor

test-includes: lib${LIB}.a .PHONY

#
# Target to make the current known bad list. In general, this list should only
# ever shrink and never grow.
#
.PHONY:	badfiles.inc
badfiles.inc:
	@(cd ${SRCTOP}/sys;							\
	echo "# DO NOT EDIT-- this file is automatically @""generated.";	\
	echo "BADHDRS= \\";							\
	for i in ${HDRS}; do						\
		echo "#include <$$i>" |						\
			${CC} ${CFLAGS} -xc - -c -o /dev/null 2> /dev/null ||	\
			echo "	$$i \\";					\
	done;									\
	echo) > ${.CURDIR}/badfiles.inc

.include <bsd.lib.mk>
