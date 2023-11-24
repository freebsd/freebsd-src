/*
 * Copyright (c) 2016 Proofpoint, Inc. and its suppliers.
 *      All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 */

#ifndef SM_NOTIFY_H
#define SM_NOTIFY_H

/* microseconds */
#define SM_MICROS 1000000L

int sm_notify_init __P((int));
int sm_notify_start __P((bool, int));
int sm_notify_stop __P((bool, int));
int sm_notify_rcv __P((char *, size_t, long));
int sm_notify_snd __P((char *, size_t));

#endif /* ! SM_MSG_H */
