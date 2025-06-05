/* ccapi/common/win/win-utils.h */
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

#ifndef _win_utils_h
#define _win_utils_h

#ifndef TRUE
#define TRUE    (1==1)
#endif

#ifndef FALSE
#define FALSE   (1==0)
#endif

static enum ccapiMsgType {
    CCMSG_INVALID   = 0,
    CCMSG_CONNECT,
    CCMSG_REQUEST,
    CCMSG_CONNECT_REPLY,
    CCMSG_REQUEST_REPLY,
    CCMSG_DISCONNECT,
    CCMSG_LISTEN,
    CCMSG_PING,
    CCMSG_QUIT
    };

char*                   clientEndpoint(const char* UUID);
char*                   serverEndpoint(const char* UUID);
extern unsigned char*   pszProtocolSequence;

char* timestamp();

#endif // _win_utils_h