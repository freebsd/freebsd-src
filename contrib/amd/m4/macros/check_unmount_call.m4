dnl ######################################################################
dnl check for the correct system call to unmount a filesystem.
AC_DEFUN(AMU_CHECK_UNMOUNT_CALL,
[
dnl make sure this one is called before [AC_CHECK_UNMOUNT_ARGS]
AC_BEFORE([$0], [AC_CHECK_UNMOUNT_ARGS])
AC_CACHE_CHECK(the system call to unmount a filesystem,
ac_cv_unmount_call,
[
# check for various unmount a filesystem calls
if test "$ac_cv_func_uvmount" = yes ; then
  ac_cv_unmount_call=uvmount
elif test "$ac_cv_func_unmount" = yes ; then
  ac_cv_unmount_call=unmount
elif test "$ac_cv_func_umount" = yes ; then
  ac_cv_unmount_call=umount
else
  ac_cv_unmount_call=no
fi
])
if test "$ac_cv_unmount_call" != no
then
  am_utils_unmount_call=$ac_cv_unmount_call
  AC_SUBST(am_utils_unmount_call)
fi
])
dnl ======================================================================
