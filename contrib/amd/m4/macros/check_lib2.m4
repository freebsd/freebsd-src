dnl a bug-fixed version of autoconf 2.12.
dnl first try to link library without $5, and only of that failed,
dnl try with $5 if specified.
dnl it adds $5 to $LIBS if it was needed -Erez.
dnl AC_CHECK_LIB2(LIBRARY, FUNCTION [, ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND
dnl              [, OTHER-LIBRARIES]]])
AC_DEFUN(AMU_CHECK_LIB2,
[AC_MSG_CHECKING([for $2 in -l$1])
dnl Use a cache variable name containing both the library and function name,
dnl because the test really is for library $1 defining function $2, not
dnl just for library $1.  Separate tests with the same $1 and different $2s
dnl may have different results.
ac_lib_var=`echo $1['_']$2 | sed 'y%./+-%__p_%'`
AC_CACHE_VAL(ac_cv_lib_$ac_lib_var,
[ac_save_LIBS="$LIBS"

# first try with base library, without auxiliary library
LIBS="-l$1 $LIBS"
AC_TRY_LINK(dnl
ifelse([$2], [main], , dnl Avoid conflicting decl of main.
[/* Override any gcc2 internal prototype to avoid an error.  */
]
[/* We use char because int might match the return type of a gcc2
    builtin and then its argument prototype would still apply.  */
char $2();
]),
	    [$2()],
	    eval "ac_cv_lib_$ac_lib_var=\"$1\"",
	    eval "ac_cv_lib_$ac_lib_var=no")

# if OK, set to no auxiliary library, else try auxiliary library
if eval "test \"`echo '$ac_cv_lib_'$ac_lib_var`\" = no"; then
 LIBS="-l$1 $5 $LIBS"
 AC_TRY_LINK(dnl
 ifelse([$2], [main], , dnl Avoid conflicting decl of main.
 [/* Override any gcc2 internal prototype to avoid an error.  */
 ]
 [/* We use char because int might match the return type of a gcc2
     builtin and then its argument prototype would still apply.  */
 char $2();
 ]),
 	    [$2()],
 	    eval "ac_cv_lib_$ac_lib_var=\"$1 $5\"",
 	    eval "ac_cv_lib_$ac_lib_var=no")
fi

LIBS="$ac_save_LIBS"
])dnl
ac_tmp="`eval echo '$''{ac_cv_lib_'$ac_lib_var'}'`"
if test "${ac_tmp}" != no; then
  AC_MSG_RESULT(-l$ac_tmp)
  ifelse([$3], ,
[
  ac_tr_lib=HAVE_LIB`echo $1 | sed -e 's/[[^a-zA-Z0-9_]]/_/g' \
    -e 'y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/'`

  AC_DEFINE_UNQUOTED($ac_tr_lib)
  LIBS="-l$ac_tmp $LIBS"
], [$3])
else
  AC_MSG_RESULT(no)
ifelse([$4], , , [$4
])dnl
fi

])
