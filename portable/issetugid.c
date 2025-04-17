/*
 * Replacement for a missing issetugid.
 *
 * Simulates the functionality as the Solaris function issetugid, which
 * returns true if the running program was setuid or setgid.  The replacement
 * test is not quite as comprehensive as what the Solaris function does, but
 * it should be good enough.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2010-2011
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Copying and distribution of this file, with or without modification, are
 * permitted in any medium without royalty provided the copyright notice and
 * this notice are preserved.  This file is offered as-is, without any
 * warranty.
 *
 * SPDX-License-Identifier: FSFAP
 */

#include <config.h>
#include <portable/system.h>

int
issetugid(void)
{
    if (getuid() != geteuid())
        return 1;
    if (getgid() != getegid())
        return 1;
    return 0;
}
