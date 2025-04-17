/*
 * Interface to PAM option parsing.
 *
 * This interface defines a lot of macros and types with very short names, and
 * hence without a lot of namespace protection.  It should be included only in
 * the file that's doing the option parsing and not elsewhere to remove the
 * risk of clashes.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2010-2011, 2013
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

#ifndef PAM_UTIL_OPTIONS_H
#define PAM_UTIL_OPTIONS_H 1

#include <config.h>
#ifdef HAVE_KRB5
#    include <portable/krb5.h>
#endif
#include <portable/macros.h>
#include <portable/stdbool.h>

#include <stddef.h>

/* Forward declarations to avoid additional includes. */
struct vector;

/*
 * The types of configuration values possible.  STRLIST is a list data type
 * that takes its default from a string value instead of a vector.  For
 * STRLIST, the default string value will be turned into a vector by splitting
 * on comma, space, and tab.  (This is the same as would be done with the
 * value of a PAM setting when the target variable type is a list.)
 */
enum type
{
    TYPE_BOOLEAN,
    TYPE_NUMBER,
    TYPE_TIME,
    TYPE_STRING,
    TYPE_LIST,
    TYPE_STRLIST
};

/*
 * Each configuration option is defined by a struct option.  This specifies
 * the name of the option, its offset into the configuration struct, whether
 * it can be specified in a krb5.conf file, its type, and its default value if
 * not set.  Note that PAM configuration options are specified as strings, so
 * there's no native way of representing a list argument.  List values are
 * always initialized by splitting a string on whitespace or commas.
 *
 * The default value should really be a union, but you can't initialize unions
 * properly in C in a static initializer without C99 named initializer
 * support, which we can't (yet) assume.  So use a struct instead, and
 * initialize all the members, even though we'll only care about one of them.
 *
 * Note that numbers set in the configuration struct created by this interface
 * must be longs, not ints.  There is currently no provision for unsigned
 * numbers.
 *
 * Times take their default from defaults.number.  The difference between time
 * and number is in the parsing of a user-supplied value and the type of the
 * stored attribute.
 */
struct option {
    const char *name;
    size_t location;
    bool krb5_config;
    enum type type;
    struct {
        bool boolean;
        long number;
        const char *string;
        const struct vector *list;
    } defaults;
};

/*
 * The following macros are helpers to make it easier to define the table that
 * specifies how to convert the configuration into a struct.  They provide an
 * initializer for the type and default fields.
 */
/* clang-format off */
#define BOOL(def)    TYPE_BOOLEAN, { (def),     0,  NULL,  NULL }
#define NUMBER(def)  TYPE_NUMBER,  {     0, (def),  NULL,  NULL }
#define TIME(def)    TYPE_TIME,    {     0, (def),  NULL,  NULL }
#define STRING(def)  TYPE_STRING,  {     0,     0, (def),  NULL }
#define LIST(def)    TYPE_LIST,    {     0,     0,  NULL, (def) }
#define STRLIST(def) TYPE_STRLIST, {     0,     0, (def),  NULL }
/* clang-format on */

/*
 * The user of this file should also define a macro of the following form:
 *
 *     #define K(name) (#name), offsetof(struct pam_config, name)
 *
 * Then, the definition of the necessary table for building the configuration
 * will look something like this:
 *
 *     const struct option options[] = {
 *         { K(aklog_homedir), true,  BOOL   (false) },
 *         { K(cells),         true,  LIST   (NULL)  },
 *         { K(debug),         false, BOOL   (false) },
 *         { K(minimum_uid),   true,  NUMBER (0)     },
 *         { K(program),       true,  STRING (NULL)  },
 *     };
 *
 * which provides a nice, succinct syntax for creating the table.  The options
 * MUST be in sorted order, since the options parsing code does a binary
 * search.
 */

BEGIN_DECLS

/* Default to a hidden visibility for all internal functions. */
#pragma GCC visibility push(hidden)

/*
 * Set the defaults for the PAM configuration.  Takes the PAM arguments, an
 * option table defined as above, and the number of entries in the table.  The
 * config member of the args struct must already be allocated.  Returns true
 * on success and false on error (generally out of memory).  Errors will
 * already be reported using putil_crit().
 *
 * This function must be called before either putil_args_krb5() or
 * putil_args_parse(), since neither of those functions set defaults.
 */
bool putil_args_defaults(struct pam_args *, const struct option options[],
                         size_t optlen) __attribute__((__nonnull__));

/*
 * Fill out options from krb5.conf.  Takes the PAM args structure, the name of
 * the section for the software being configured, an option table defined as
 * above, and the number of entries in the table.  The config member of the
 * args struct must already be allocated.  Only those options whose
 * krb5_config attribute is true will be considered.
 *
 * This code automatically checks for configuration settings scoped to the
 * local realm, so the default realm should be set before calling this
 * function.  If that's done based on a configuration option, one may need to
 * pre-parse the configuration options.
 *
 * Returns true on success and false on an error.  An error return should be
 * considered fatal.  Errors will already be reported using putil_crit*() or
 * putil_err*() as appropriate.  If Kerberos is not available, returns without
 * doing anything.
 *
 * putil_args_defaults() should be called before this function.
 */
bool putil_args_krb5(struct pam_args *, const char *section,
                     const struct option options[], size_t optlen)
    __attribute__((__nonnull__));

/*
 * Parse the PAM arguments and fill out the provided struct.  Takes the PAM
 * arguments, the argument count and vector, an option table defined as above,
 * and the number of entries in the table.  The config member of the args
 * struct must already be allocated.  Returns true on success and false on
 * error.  An error return should be considered fatal.  Errors will already be
 * reported using putil_crit().  Unknown options will also be diagnosed (to
 * syslog at LOG_ERR using putil_err()), but are not considered fatal errors
 * and will still return true.
 *
 * The krb5_config option of the option configuration is ignored by this
 * function.  If options should be retrieved from krb5.conf, call
 * putil_args_krb5() first, before calling this function.
 *
 * putil_args_defaults() should be called before this function.
 */
bool putil_args_parse(struct pam_args *, int argc, const char *argv[],
                      const struct option options[], size_t optlen)
    __attribute__((__nonnull__));

/* Undo default visibility change. */
#pragma GCC visibility pop

END_DECLS

#endif /* !PAM_UTIL_OPTIONS_H */
