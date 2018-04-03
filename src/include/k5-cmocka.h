/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* include/k5-cmocka.h - indirect header file for cmocka test programs */

/*
 * This header conditionally includes cmocka.h, so that "make depend" can work
 * on cmocka test programs when cmocka isn't available.  It also includes the
 * three system headers required for cmocka.h.
 */

#include "autoconf.h"
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#ifdef HAVE_CMOCKA
#include <cmocka.h>
#endif
