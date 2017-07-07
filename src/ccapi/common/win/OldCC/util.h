/* ccapi/common/win/OldCC/util.h */
/*
 * Copyright 2008 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#ifndef __UTIL_H__
#define __UTIL_H__

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

BOOL isNT();

void*
user_allocate(
    size_t size
    );

void
user_free(
    void* ptr
    );

void
free_alloc_p(
    void* pptr
    );

DWORD
alloc_name(
    LPSTR* pname,
    LPSTR postfix,
    BOOL isNT
    );

DWORD
alloc_own_security_descriptor_NT(
    PSECURITY_DESCRIPTOR* ppsd
    );

DWORD
alloc_module_dir_name(
    char* module,
    char** pname
    );

DWORD
alloc_module_dir_name_with_file(
    char* module,
    char* file,
    char** pname
    );

DWORD alloc_cmdline_2_args(
    char* prog,
    char* arg1,
    char* arg2,
    char** pname);

#ifdef __cplusplus
}
#endif

#endif /* __UTIL_H__ */
