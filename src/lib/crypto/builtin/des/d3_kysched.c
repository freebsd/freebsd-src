/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1995 by Richard P. Basch.  All Rights Reserved.
 * Copyright 1995 by Lehman Brothers, Inc.  All Rights Reserved.
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
 * the name of Richard P. Basch, Lehman Brothers and M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  Richard P. Basch,
 * Lehman Brothers and M.I.T. make no representations about the suitability
 * of this software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#include "k5-int.h"
#include "des_int.h"

int
mit_des3_key_sched(mit_des3_cblock k, mit_des3_key_schedule schedule)
{
    mit_des_make_key_sched(k[0],schedule[0]);
    mit_des_make_key_sched(k[1],schedule[1]);
    mit_des_make_key_sched(k[2],schedule[2]);

    if (!mit_des_check_key_parity(k[0]))        /* bad parity --> return -1 */
        return(-1);
    if (mit_des_is_weak_key(k[0]))
        return(-2);

    if (!mit_des_check_key_parity(k[1]))
        return(-1);
    if (mit_des_is_weak_key(k[1]))
        return(-2);

    if (!mit_des_check_key_parity(k[2]))
        return(-1);
    if (mit_des_is_weak_key(k[2]))
        return(-2);

    /* if key was good, return 0 */
    return 0;
}
