dnl ######################################################################
dnl check for type of pte_t (for Irix, usually in <sys/immu.h>)
dnl Note: some gcc's on Irix 6.5 are broken and don't recognize pte_t,
dnl so I'm defining it here to unsigned int, which is not necessarily correct,
dnl but at least it gets am-utils to compile.
AC_DEFUN([AMU_TYPE_PTE_T],
[AC_CHECK_TYPE(pte_t, ,
[AC_DEFINE_UNQUOTED(pte_t, unsigned int,
	 [Check if pte_t is defined in <sys/immu.h>])],
[
#ifdef HAVE_SYS_IMMU_H
# include <sys/immu.h>
#endif /* HAVE_SYS_IMMU_H */
])])
dnl ======================================================================
