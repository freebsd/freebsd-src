dnl
dnl $Id: capabilities.m4,v 1.2.20.1 2004/04/01 07:27:32 joda Exp $
dnl

dnl
dnl Test SGI capabilities
dnl

AC_DEFUN([KRB_CAPABILITIES],[

AC_CHECK_HEADERS(capability.h sys/capability.h)

AC_CHECK_FUNCS(sgi_getcapabilitybyname cap_set_proc)
])
