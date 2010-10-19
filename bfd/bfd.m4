dnl This file was derived from acinclude.m4.


dnl Check for existence of a type $1 in sys/procfs.h

AC_DEFUN([BFD_HAVE_SYS_PROCFS_TYPE],
[AC_MSG_CHECKING([for $1 in sys/procfs.h])
 AC_CACHE_VAL(bfd_cv_have_sys_procfs_type_$1,
   [AC_TRY_COMPILE([
#define _SYSCALL32
#include <sys/procfs.h>],
      [$1 avar],
      bfd_cv_have_sys_procfs_type_$1=yes,
      bfd_cv_have_sys_procfs_type_$1=no
   )])
 if test $bfd_cv_have_sys_procfs_type_$1 = yes; then
   AC_DEFINE([HAVE_]translit($1, [a-z], [A-Z]), 1,
	     [Define if <sys/procfs.h> has $1.])
 fi
 AC_MSG_RESULT($bfd_cv_have_sys_procfs_type_$1)
])


dnl Check for existence of member $2 in type $1 in sys/procfs.h

AC_DEFUN([BFD_HAVE_SYS_PROCFS_TYPE_MEMBER],
[AC_MSG_CHECKING([for $1.$2 in sys/procfs.h])
 AC_CACHE_VAL(bfd_cv_have_sys_procfs_type_member_$1_$2,
   [AC_TRY_COMPILE([
#define _SYSCALL32
#include <sys/procfs.h>],
      [$1 avar; void* aref = (void*) &avar.$2],
      bfd_cv_have_sys_procfs_type_member_$1_$2=yes,
      bfd_cv_have_sys_procfs_type_member_$1_$2=no
   )])
 if test $bfd_cv_have_sys_procfs_type_member_$1_$2 = yes; then
   AC_DEFINE([HAVE_]translit($1, [a-z], [A-Z])[_]translit($2, [a-z], [A-Z]), 1,
	     [Define if <sys/procfs.h> has $1.$2.])
 fi
 AC_MSG_RESULT($bfd_cv_have_sys_procfs_type_member_$1_$2)
])

