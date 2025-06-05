/* ccapi/common/cci_common.h */
/*
 * Copyright 2006, 2007 Massachusetts Institute of Technology.
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

#ifndef CCI_COMMON_H
#define CCI_COMMON_H

#include <CredentialsCache.h>

#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <com_err.h>

#if TARGET_OS_MAC
#include <unistd.h>
#define VECTOR_FUNCTIONS_INITIALIZER ,NULL
#else
#include "win-mac.h"
#define VECTOR_FUNCTIONS_INITIALIZER
#endif

#define k_cci_context_initial_ccache_name "Initial default ccache"

#include "cci_cred_union.h"
#include "cci_debugging.h"
#include "cci_identifier.h"
#include "cci_message.h"

#include "k5-ipc_stream.h"

#endif /* CCI_COMMON_H */
