#!/bin/sh

set -e

prefix=@prefix@
srcdir=@srcdir@

DEFAULT_SYS_PATH="@default_sys_path@"

case "@use_meta@" in
yes) XDEFS="-DUSE_META ${XDEFS}";;
esac

CC="@CC@"
CFLAGS="@CFLAGS@ -I. -I${srcdir} @DEFS@ @CPPFLAGS@ -DMAKE_NATIVE ${XDEFS} -DBMAKE_PATH_MAX=@bmake_path_max@"

MAKE_VERSION=@_MAKE_VERSION@

MDEFS="-DMAKE_VERSION=\"$MAKE_VERSION\" \
-D@force_machine@MACHINE=\"@machine@\" \
-D@force_machine_arch@MACHINE_ARCH=\"@machine_arch@\" \
-D@force_make_os@MAKE_OS=\"@make_os@\" \
-D_PATH_DEFSYSPATH=\"${DEFAULT_SYS_PATH}\""


LDFLAGS="@LDFLAGS@"
LIBS="@LIBS@"

toUpper() {
    ${TR:-tr} abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ
}

do_compile2() {
	obj="$1"; shift
	src="$1"; shift
	echo ${CC} -c ${CFLAGS} "$@" -o "$obj" "$src"
	${CC} -c ${CFLAGS} "$@" -o "$obj" "$src"
}

do_compile() {
	obj="$1"; shift
	case "$1" in
	*.c) src=$1; shift;;
	*) src=`basename "$obj" .o`.c;;
	esac

	for d in "$srcdir" "$srcdir/lst.lib"
	do
		test -s "$d/$src" || continue

		do_compile2 "$obj" "$d/$src" "$@" || exit 1
		return
	done
	echo "Unknown object file '$obj'" >&2
	exit 1
}

do_link() {
	output="$1"; shift
	echo ${CC} ${LDSTATIC} ${LDFLAGS} -o "$output" "$@" ${LIBS}
	${CC} ${LDSTATIC} ${LDFLAGS} -o "$output" "$@" ${LIBS}
}

BASE_OBJECTS="arch.o buf.o compat.o cond.o dir.o for.o hash.o \
lst.o make.o make_malloc.o metachar.o parse.o sigcompat.o str.o \
suff.o targ.o trace.o var.o util.o"

LIB_OBJECTS="@LIBOBJS@"

do_compile main.o ${MDEFS}

for o in ${BASE_OBJECTS} ${LIB_OBJECTS}
do
	do_compile "$o"
done

case "@use_meta@" in
yes)
	case "@use_filemon@" in
	no) MDEFS=;;
	*)
		MDEFS="-DUSE_FILEMON -DUSE_FILEMON_`echo @use_filemon@ | toUpper`"
		case "@use_filemon@,@filemon_h@" in
		dev,*/filemon.h) FDEFS="-DHAVE_FILEMON_H -I`dirname @filemon_h@`";;
		*) FDEFS=;;
		esac
		do_compile filemon_@use_filemon@.o filemon/filemon_@use_filemon@.c ${FDEFS}
		BASE_OBJECTS="filemon_@use_filemon@.o $BASE_OBJECTS"
		;;
	esac
	do_compile meta.o ${MDEFS}
	BASE_OBJECTS="meta.o ${BASE_OBJECTS}"
	;;
esac
do_compile job.o ${MDEFS}

do_link bmake main.o job.o ${BASE_OBJECTS} ${LST_OBJECTS} ${LIB_OBJECTS}
