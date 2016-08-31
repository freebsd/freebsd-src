dnl ######################################################################
dnl check for type of rpcvers_t (usually in <rpc/types.h>)
AC_DEFUN([AMU_TYPE_RPCVERS_T],
[AC_CHECK_TYPE(rpcvers_t, ,
[AC_DEFINE_UNQUOTED(rpcvers_t, unsigned long, [Check if rpcvers_t is defined in <rpc/types.h>])],
[
#ifdef HAVE_RPC_TYPES_H
# include <rpc/types.h>
#endif /* HAVE_RPC_TYPES_H */
])])
dnl ======================================================================
