#ifndef __LOADFUNCS_COM_ERR_H__
#define __LOADFUNCS_COM_ERR_H__

#include "loadfuncs.h"
#include <com_err.h>

#if defined(_WIN64)
#define COMERR_DLL      "comerr64.dll"
#else
#define COMERR_DLL      "comerr32.dll"
#endif

TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV_C,
    com_err,
    (const char FAR *, errcode_t, const char FAR *, ...)
    );
TYPEDEF_FUNC(
    void,
    KRB5_CALLCONV,
    com_err_va,
    (const char FAR *whoami, errcode_t code, const char FAR *fmt, va_list ap)
    );
TYPEDEF_FUNC(
    const char FAR *,
    KRB5_CALLCONV,
    error_message,
    (errcode_t)
    );
TYPEDEF_FUNC(
    errcode_t,
    KRB5_CALLCONV,
    add_error_table,
    (const struct error_table FAR *)
    );
TYPEDEF_FUNC(
    errcode_t,
    KRB5_CALLCONV,
    remove_error_table,
    (const struct error_table FAR *)
    );

#endif /* __LOADFUNCS_COM_ERR_H__ */
