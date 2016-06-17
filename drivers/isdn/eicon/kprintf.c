/*
 * Source file for kernel interface to kernel log facility
 *
 * Copyright (C) Eicon Technology Corporation, 2000.
 *
 * Eicon File Revision :    1.3  
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#include "eicon.h"
#include "sys.h"
#include <stdarg.h>

#include "divas.h"
#include "divalog.h"
#include "uxio.h"

void    DivasPrintf(char  *fmt, ...)

{
    klog_t      log;            /* log entry buffer */

    va_list     argptr;         /* pointer to additional args */

    va_start(argptr, fmt);

    /* clear log entry */

    memset((void *) &log, 0, sizeof(klog_t));

    log.card = -1;
    log.type = KLOG_TEXT_MSG;

    /* time stamp the entry */

    log.time_stamp = UxTimeGet();

    /* call vsprintf to format the user's information */

    vsnprintf(log.buffer, DIM(log.buffer), fmt, argptr);

    va_end(argptr);

    /* send to the log streams driver and return */

    DivasLogAdd(&log, sizeof(klog_t));

    return;
}
