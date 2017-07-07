/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/os/write_msg.c - convenience sendauh/recvauth functions */
/*
 * Copyright 1991, 2009 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
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

#include "k5-int.h"
#include <errno.h>
#include "os-proto.h"

/*
 * Try to write a series of messages with as few write(v) system calls
 * as possible, to avoid Nagle/DelayedAck problems.  Cheating here a
 * little -- I know the only cases we have at the moment will send one
 * or two messages in a call.  Sending more will work, but not as
 * efficiently.
 */
krb5_error_code
k5_write_messages(krb5_context context, krb5_pointer fdp, krb5_data *outbuf,
                  int nbufs)
{
    int fd = *( (int *) fdp);

    while (nbufs) {
        int nbufs1;
        sg_buf sg[4];
        krb5_int32 len[2];

        if (nbufs > 1)
            nbufs1 = 2;
        else
            nbufs1 = 1;
        len[0] = htonl(outbuf[0].length);
        SG_SET(&sg[0], &len[0], 4);
        SG_SET(&sg[1], outbuf[0].length ? outbuf[0].data : NULL,
               outbuf[0].length);
        if (nbufs1 == 2) {
            len[1] = htonl(outbuf[1].length);
            SG_SET(&sg[2], &len[1], 4);
            SG_SET(&sg[3], outbuf[1].length ? outbuf[1].data : NULL,
                   outbuf[1].length);
        }
        if (krb5int_net_writev(context, fd, sg, nbufs1 * 2) < 0) {
            return errno;
        }
        outbuf += nbufs1;
        nbufs -= nbufs1;
    }
    return(0);
}

krb5_error_code
krb5_write_message(krb5_context context, krb5_pointer fdp, krb5_data *outbuf)
{
    return k5_write_messages(context, fdp, outbuf, 1);
}
