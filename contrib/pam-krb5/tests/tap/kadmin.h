/*
 * Utility functions for tests needing Kerberos admin actions.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2011, 2013
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef TAP_KADMIN_H
#define TAP_KADMIN_H 1

#include <config.h>
#include <portable/stdbool.h>

#include <time.h>

#include <tests/tap/macros.h>

BEGIN_DECLS

/*
 * Given the principal to set an expiration on and the expiration time, set
 * that principal's key to expire at that time.  Authentication is done using
 * the keytab stored in config/admin-keytab.
 *
 * Returns true on success.  Returns false if necessary configuration is
 * missing so that the caller can choose whether to call bail or skip_all.  If
 * the configuration is present but the operation fails, bails.
 */
bool kerberos_expire_password(const char *, time_t)
    __attribute__((__nonnull__));

END_DECLS

#endif /* !TAP_KADMIN_H */
