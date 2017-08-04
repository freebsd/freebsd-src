dnl ######################################################################
dnl @synopsis NTP_HARDEN([SCRIPTSDIRPATH])
dnl Build (compile/link) hardening information:
dnl - NTP_HARD_CFLAGS
dnl - NTP_HARD_CPPFLAGS
dnl - NTP_HARD_LDFLAGS

AC_DEFUN([NTP_HARDEN], [

AC_MSG_CHECKING([for compile/link hardening flags])

AC_ARG_WITH(
    [locfile],
    [AS_HELP_STRING(
	[--with-locfile=XXX],
	[os-specific or "legacy"]
    )],
    [],
    [with_locfile=no]
)

(									\
    SENTINEL_DIR="$PWD" &&						\
    cd $srcdir/$1 &&							\
    case "$with_locfile" in						\
     yes|no|'')								\
	scripts/genHardFlags -d "$SENTINEL_DIR"				\
	;;								\
     *)									\
	scripts/genHardFlags -d "$SENTINEL_DIR" -f "$with_locfile"	\
	;;								\
    esac								\
) > genHardFlags.i 2> genHardFlags.err
. ./genHardFlags.i

case "$GENHARDFLAG" in
 OK)
    AC_MSG_RESULT([in file $GENHARDFLAGFILE])
    rm genHardFlags.err genHardFlags.i
    ;;
 *)
    AC_MSG_RESULT([failed.])
    AC_MSG_ERROR([Problem with genHardFlags!])
    ;;
esac

AC_SUBST([NTP_HARD_CFLAGS])
AC_SUBST([NTP_HARD_CPPFLAGS])
AC_SUBST([NTP_HARD_LDFLAGS])

])dnl
dnl ======================================================================
