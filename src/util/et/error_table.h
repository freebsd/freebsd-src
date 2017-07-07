/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1988 by the Student Information Processing Board of the
 * Massachusetts Institute of Technology.
 *
 * For copyright info, see mit-sipb-copyright.h.
 */

#ifndef _ET_H

#include <errno.h>

#define ET_EBUFSIZ 1024

struct et_list {
    struct et_list *next;
    const struct error_table *table;
};

#define ERRCODE_RANGE   8       /* # of bits to shift table number */
#define BITS_PER_CHAR   6       /* # bits to shift per character in name */
#define ERRCODE_MAX   0xFFFFFFFFUL      /* Mask for maximum error table */

const char *error_table_name(unsigned long);
const char *error_table_name_r(unsigned long, char *outbuf);

#include "k5-thread.h"
extern k5_mutex_t com_err_hook_lock;
int com_err_finish_init(void);

#define _ET_H
#endif
