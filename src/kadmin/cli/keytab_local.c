/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * A wrapper around keytab.c used by kadmin.local to expose the -norandkey
 * flag.  This avoids building two object files from the same source file,
 * which is otherwise tricky with compilers that don't support -c and -o
 * at the same time.
 */

#define KADMIN_LOCAL
#include "keytab.c"
