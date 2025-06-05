/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/ss/prompt.c */
/*
 * Copyright 1987, 1988 by MIT Student Information Processing Board
 *
 * For copyright information, see copyright.h.
 */

#include "copyright.h"
#include <stdio.h>
#include "ss_internal.h"

void
ss_set_prompt(sci_idx, new_prompt)
    int sci_idx;
    char *new_prompt;
{
    ss_info(sci_idx)->prompt = new_prompt;
}

char *
ss_get_prompt(sci_idx)
    int sci_idx;
{
    return(ss_info(sci_idx)->prompt);
}
