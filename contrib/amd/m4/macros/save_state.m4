dnl ######################################################################
dnl AC_SAVE_STATE: save confdefs.h onto dbgcf.h and write $ac_cv_* cache
dnl variables that are known so far.
define(AMU_SAVE_STATE,
AC_MSG_NOTICE(*** SAVING CONFIGURE STATE ***)
if test -f confdefs.h
then
 cp confdefs.h dbgcf.h
fi
[AC_CACHE_SAVE]
)
dnl ======================================================================
