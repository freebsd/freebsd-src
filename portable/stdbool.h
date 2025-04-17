/*
 * Portability wrapper around <stdbool.h>.
 *
 * Provides the bool and _Bool types and the true and false constants,
 * following the C99 specification, on hosts that don't have stdbool.h.  This
 * logic is based heavily on the example in the Autoconf manual.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2008, 2011
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Copying and distribution of this file, with or without modification, are
 * permitted in any medium without royalty provided the copyright notice and
 * this notice are preserved.  This file is offered as-is, without any
 * warranty.
 *
 * SPDX-License-Identifier: FSFAP
 */

#ifndef PORTABLE_STDBOOL_H
#define PORTABLE_STDBOOL_H 1

/*
 * Allow inclusion of config.h to be skipped, since sometimes we have to use a
 * stripped-down version of config.h with a different name.
 */
#ifndef CONFIG_H_INCLUDED
#    include <config.h>
#endif

#if HAVE_STDBOOL_H
#    include <stdbool.h>
#else
#    if HAVE__BOOL
#        define bool _Bool
#    else
#        ifdef __cplusplus
typedef bool _Bool;
#        elif _WIN32
#            include <windef.h>
#            define bool BOOL
#        else
typedef unsigned char _Bool;
#            define bool _Bool
#        endif
#    endif
#    define false 0
#    define true 1
#    define __bool_true_false_are_defined 1
#endif

/*
 * If we define bool and don't tell Perl, it will try to define its own and
 * fail.  Only of interest for programs that also include Perl headers.
 */
#ifndef HAS_BOOL
#    define HAS_BOOL 1
#endif

#endif /* !PORTABLE_STDBOOL_H */
