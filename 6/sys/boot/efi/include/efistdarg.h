/* $FreeBSD$ */
#ifndef _EFISTDARG_H_
#define _EFISTDARG_H_

/*++

Copyright (c) 1998  Intel Corporation

Module Name:

    devpath.h

Abstract:

    Defines for parsing the EFI Device Path structures



Revision History

--*/

#define _INTSIZEOF(n)   ( (sizeof(n) + sizeof(UINTN) - 1) & ~(sizeof(UINTN) - 1) )

typedef CHAR8 * va_list;

#define va_start(ap,v)  ( ap = (va_list)&v + _INTSIZEOF(v) )
#define va_arg(ap,t)    ( *(t *)((ap += _INTSIZEOF(t)) - _INTSIZEOF(t)) )
#define va_end(ap)  ( ap = (va_list)0 )


#endif  /* _INC_STDARG */
