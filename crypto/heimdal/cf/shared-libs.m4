dnl
dnl $Id: shared-libs.m4,v 1.6 2000/11/17 02:59:27 assar Exp $
dnl
dnl Shared library stuff has to be different everywhere
dnl

AC_DEFUN(AC_SHARED_LIBS, [

dnl Check if we want to use shared libraries
AC_ARG_ENABLE(shared,
[  --enable-shared         create shared libraries for Kerberos])

AC_SUBST(CFLAGS)dnl
AC_SUBST(LDFLAGS)dnl

case ${enable_shared} in
  yes ) enable_shared=yes;;
  no  ) enable_shared=no;;
  *   ) enable_shared=no;;
esac

# NOTE: Building shared libraries may not work if you do not use gcc!
#
# OS		$SHLIBEXT
# HP-UX		sl
# Linux		so
# NetBSD	so
# FreeBSD	so
# OSF		so
# SunOS5	so
# SunOS4	so.0.5
# Irix		so
#
# LIBEXT is the extension we should build (.a or $SHLIBEXT)
LINK='$(CC)'
AC_SUBST(LINK)
lib_deps=yes
REAL_PICFLAGS="-fpic"
LDSHARED='$(CC) $(PICFLAGS) -shared'
LIBPREFIX=lib
build_symlink_command=@true
install_symlink_command=@true
install_symlink_command2=@true
REAL_SHLIBEXT=so
changequote({,})dnl
SHLIB_VERSION=`echo $VERSION | sed 's/\([0-9.]*\).*/\1/'`
SHLIB_SONAME=`echo $VERSION | sed 's/\([0-9]*\).*/\1/'`
changequote([,])dnl
case "${host}" in
*-*-hpux*)
	REAL_SHLIBEXT=sl
	REAL_LD_FLAGS='-Wl,+b$(libdir)'
	if test -z "$GCC"; then
		LDSHARED="ld -b"
		REAL_PICFLAGS="+z"
	fi
	lib_deps=no
	;;
*-*-linux*)
	LDSHARED='$(CC) -shared -Wl,-soname,$(LIBNAME).so.'"${SHLIB_SONAME}"
	REAL_LD_FLAGS='-Wl,-rpath,$(libdir)'
	REAL_SHLIBEXT=so.$SHLIB_VERSION
	build_symlink_command='$(LN_S) -f [$][@] $(LIBNAME).so'
	install_symlink_command='$(LN_S) -f $(LIB) $(DESTDIR)$(libdir)/$(LIBNAME).so.'"${SHLIB_SONAME}"';$(LN_S) -f $(LIB) $(DESTDIR)$(libdir)/$(LIBNAME).so'
	install_symlink_command2='$(LN_S) -f $(LIB2) $(DESTDIR)$(libdir)/$(LIBNAME2).so.'"${SHLIB_SONAME}"';$(LN_S) -f $(LIB2) $(DESTDIR)$(libdir)/$(LIBNAME2).so'
	;;
changequote(,)dnl
*-*-freebsd[345]* | *-*-freebsdelf[345]*)
changequote([,])dnl
	REAL_SHLIBEXT=so.$SHLIB_VERSION
	REAL_LD_FLAGS='-Wl,-R$(libdir)'
	build_symlink_command='$(LN_S) -f [$][@] $(LIBNAME).so'
	install_symlink_command='$(LN_S) -f $(LIB) $(DESTDIR)$(libdir)/$(LIBNAME).so'
	install_symlink_command2='$(LN_S) -f $(LIB2) $(DESTDIR)$(libdir)/$(LIBNAME2).so'
	;;
*-*-*bsd*)
	REAL_SHLIBEXT=so.$SHLIB_VERSION
	LDSHARED='ld -Bshareable'
	REAL_LD_FLAGS='-Wl,-R$(libdir)'
	;;
*-*-osf*)
	REAL_LD_FLAGS='-Wl,-rpath,$(libdir)'
	REAL_PICFLAGS=
	LDSHARED='ld -shared -expect_unresolved \*'
	;;
*-*-solaris2*)
	LDSHARED='$(CC) -shared -Wl,-soname,$(LIBNAME).so.'"${SHLIB_SONAME}"
	REAL_SHLIBEXT=so.$SHLIB_VERSION
	build_symlink_command='$(LN_S) [$][@] $(LIBNAME).so'
	install_symlink_command='$(LN_S) $(LIB) $(DESTDIR)$(libdir)/$(LIBNAME).so.'"${SHLIB_SONAME}"';$(LN_S) $(LIB) $(DESTDIR)$(libdir)/$(LIBNAME).so'
	install_symlink_command2='$(LN_S) $(LIB2) $(DESTDIR)$(libdir)/$(LIBNAME2).so.'"${SHLIB_SONAME}"';$(LN_S) $(LIB2) $(DESTDIR)$(libdir)/$(LIBNAME2).so'
	REAL_LD_FLAGS='-Wl,-R$(libdir)'
	if test -z "$GCC"; then
		LDSHARED='$(CC) -G -h$(LIBNAME).so.'"${SHLIB_SONAME}"
		REAL_PICFLAGS="-Kpic"
	fi
	;;
*-fujitsu-uxpv*)
	REAL_LD_FLAGS='' # really: LD_RUN_PATH=$(libdir) cc -o ...
	REAL_LINK='LD_RUN_PATH=$(libdir) $(CC)'
	LDSHARED='$(CC) -G'
	REAL_PICFLAGS="-Kpic"
	lib_deps=no # fails in mysterious ways
	;;
*-*-sunos*)
	REAL_SHLIBEXT=so.$SHLIB_VERSION
	REAL_LD_FLAGS='-Wl,-L$(libdir)'
	lib_deps=no
	;;
*-*-irix*)
        libdir="${libdir}${abilibdirext}"
        REAL_LD_FLAGS="${abi} -Wl,-rpath,\$(libdir)"
        LD_FLAGS="${abi} -Wl,-rpath,\$(libdir)"
	LDSHARED="\$(CC) -shared ${abi}"
        REAL_PICFLAGS=
        CFLAGS="${abi} ${CFLAGS}"
	;;
*-*-os2*)
	LIBPREFIX=
	EXECSUFFIX='.exe'
	RANLIB=EMXOMF
	LD_FLAGS=-Zcrtdll
	REAL_SHLIBEXT=nobuild
	;;
*-*-cygwin32*)
	EXECSUFFIX='.exe'
	REAL_SHLIBEXT=nobuild
	;;
*)	REAL_SHLIBEXT=nobuild
	REAL_PICFLAGS= 
	;;
esac

if test "${enable_shared}" != "yes" ; then 
 PICFLAGS=""
 SHLIBEXT="nobuild"
 LIBEXT="a"
 build_symlink_command=@true
 install_symlink_command=@true
 install_symlink_command2=@true
else
 PICFLAGS="$REAL_PICFLAGS"
 SHLIBEXT="$REAL_SHLIBEXT"
 LIBEXT="$SHLIBEXT"
 AC_MSG_CHECKING(whether to use -rpath)
 case "$libdir" in
   /lib | /usr/lib | /usr/local/lib)
     AC_MSG_RESULT(no)
     REAL_LD_FLAGS=
     LD_FLAGS=
     ;;
   *)
     LD_FLAGS="$REAL_LD_FLAGS"
     test "$REAL_LINK" && LINK="$REAL_LINK"
     AC_MSG_RESULT($LD_FLAGS)
     ;;
   esac
fi

if test "$lib_deps" = yes; then
	lib_deps_yes=""
	lib_deps_no="# "
else
	lib_deps_yes="# "
	lib_deps_no=""
fi
AC_SUBST(lib_deps_yes)
AC_SUBST(lib_deps_no)

# use supplied ld-flags, or none if `no'
if test "$with_ld_flags" = no; then
	LD_FLAGS=
elif test -n "$with_ld_flags"; then
	LD_FLAGS="$with_ld_flags"
fi

AC_SUBST(REAL_PICFLAGS) dnl
AC_SUBST(REAL_SHLIBEXT) dnl
AC_SUBST(REAL_LD_FLAGS) dnl

AC_SUBST(PICFLAGS) dnl
AC_SUBST(SHLIBEXT) dnl
AC_SUBST(LDSHARED) dnl
AC_SUBST(LD_FLAGS) dnl
AC_SUBST(LIBEXT) dnl
AC_SUBST(LIBPREFIX) dnl
AC_SUBST(EXECSUFFIX) dnl

AC_SUBST(build_symlink_command)dnl
AC_SUBST(install_symlink_command)dnl
AC_SUBST(install_symlink_command2)dnl
])
