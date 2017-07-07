/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1987, 1988, 1989 Massachusetts Institute of Technology
 * (Student Information Processing Board)
 *
 * For copyright info, see copyright.h.
 */

#include <stdio.h>
#include "ss_internal.h"
#include "copyright.h"

const static char copyright[] =
    "Copyright 1987, 1988, 1989 by the Massachusetts Institute of Technology";

ss_data **_ss_table = (ss_data **)NULL;
char *_ss_pager_name = (char *)NULL;
