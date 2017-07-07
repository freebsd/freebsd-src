/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1987, 1988 by MIT Student Information Processing Board
 *
 * For copyright information, see copyright.h.
 */
#include "copyright.h"
#include <stdio.h>
#include "ss.h"

struct option {
    char *text;
    long value;
};

static struct option options[] = {
    { "dont_list", SS_OPT_DONT_LIST },
    { "^list", SS_OPT_DONT_LIST },
    { "dont_summarize", SS_OPT_DONT_SUMMARIZE },
    { "^summarize", SS_OPT_DONT_SUMMARIZE },
    { (char *)NULL, 0 }
};

long
flag_val(string)
    register char *string;
{
    register struct option *opt;
    for (opt = options; opt->text; opt++)
        if (!strcmp(opt->text, string))
            return(opt->value);
    return(0);
}
