/*
 * Dummy symbol to prevent an empty library.
 *
 * On platforms that already have all of the functions that libportable would
 * supply, Automake builds an empty library and then calls ar with nonsensical
 * arguments.  Ensure that libportable always contains at least one symbol.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2017 Russ Allbery <eagle@eyrie.org>
 * Copyright 2008, 2011, 2013
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Copying and distribution of this file, with or without modification, are
 * permitted in any medium without royalty provided the copyright notice and
 * this notice are preserved.  This file is offered as-is, without any
 * warranty.
 *
 * SPDX-License-Identifier: FSFAP
 */

#include <portable/macros.h>

/* Prototype to avoid gcc warnings and set visibility. */
int portable_dummy(void) __attribute__((__const__, __visibility__("hidden")));

int
portable_dummy(void)
{
    return 42;
}
