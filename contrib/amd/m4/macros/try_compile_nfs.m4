dnl ######################################################################
dnl Compile a program with NFS headers to try and find a feature.
dnl The headers part are fixed.  Only three arguments are allowed:
dnl [$1] is the program to compile (2nd arg to AC_TRY_COMPILE)
dnl [$2] action to take if the program compiled (3rd arg to AC_TRY_COMPILE)
dnl [$3] action to take if program did not compile (4rd arg to AC_TRY_COMPILE)
AC_DEFUN(AC_TRY_COMPILE_NFS,
[# try to compile a program which may have a definition for a structure
AC_TRY_COMPILE(
AMU_MOUNT_HEADERS
, [$1], [$2], [$3])
])
dnl ======================================================================
