/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 2007, 2008 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 */

#include "gssapiP_generic.h"
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

/* The mapping table is 0-based, but let's export codes that are
   1-based, keeping 0 for errors or unknown errors.

   The elements in the mapping table currently have separate copies of
   each OID stored.  This is a bit wasteful, but we are assuming the
   table isn't likely to grow very large.  */

struct mecherror {
    gss_OID_desc mech;
    OM_uint32 code;
};

static inline int
cmp_OM_uint32(OM_uint32 m1, OM_uint32 m2)
{
    if (m1 < m2)
        return -1;
    else if (m1 > m2)
        return 1;
    else
        return 0;
}

static inline int
mecherror_cmp(struct mecherror m1, struct mecherror m2)
{
    if (m1.code < m2.code)
        return -1;
    if (m1.code > m2.code)
        return 1;
    if (m1.mech.length < m2.mech.length)
        return -1;
    if (m1.mech.length > m2.mech.length)
        return 1;
    if (m1.mech.length == 0)
        return 0;
    return memcmp(m1.mech.elements, m2.mech.elements, m1.mech.length);
}

static void
print_OM_uint32 (OM_uint32 value, FILE *f)
{
    fprintf(f, "%lu", (unsigned long) value);
}

static inline int
mecherror_copy(struct mecherror *dest, struct mecherror src)
{
    *dest = src;
    if (src.mech.length > 0) {
        dest->mech.elements = malloc(src.mech.length);
        if (dest->mech.elements == NULL)
            return ENOMEM;
        memcpy(dest->mech.elements, src.mech.elements, src.mech.length);
    } else {
        dest->mech.elements = NULL;
    }
    return 0;
}

static void
mecherror_print(struct mecherror value, FILE *f)
{
    OM_uint32 minor;
    gss_buffer_desc str;
    static const struct {
        const char *oidstr, *name;
    } mechnames[] = {
        { "{ 1 2 840 113554 1 2 2 }", "krb5-new" },
        { "{ 1 3 5 1 5 2 }", "krb5-old" },
        { "{ 1 2 840 48018 1 2 2 }", "krb5-microsoft" },
        { "{ 1 3 6 1 5 5 2 }", "spnego" },
    };
    unsigned int i;

    fprintf(f, "%lu@", (unsigned long) value.code);

    if (value.mech.length == 0) {
        fprintf(f, "(com_err)");
        return;
    }
    fprintf(f, "%p=", value.mech.elements);
    if (generic_gss_oid_to_str(&minor, &value.mech, &str)) {
        fprintf(f, "(error in conversion)");
        return;
    }
    /* Note: generic_gss_oid_to_str returns a null-terminated string.  */
    for (i = 0; i < sizeof(mechnames)/sizeof(mechnames[0]); i++) {
        if (!strcmp(str.value, mechnames[i].oidstr) && mechnames[i].name != 0) {
            fprintf(f, "%s", mechnames[i].name);
            break;
        }
    }
    if (i == sizeof(mechnames)/sizeof(mechnames[0]))
        fprintf(f, "%s", (char *) str.value);
    generic_gss_release_buffer(&minor, &str);
}

#include "errmap.h"
#include "krb5.h"               /* for KRB5KRB_AP_WRONG_PRINC */

static mecherrmap m;
static k5_mutex_t mutex = K5_MUTEX_PARTIAL_INITIALIZER;
static OM_uint32 next_fake = 100000;

int gssint_mecherrmap_init(void)
{
    int err;

    err = mecherrmap_init(&m);
    if (err)
        return err;
    err = k5_mutex_finish_init(&mutex);
    if (err) {
        mecherrmap_destroy(&m);
        return err;
    }

    return 0;
}

/* Currently the enumeration template doesn't handle freeing
   element storage when destroying the collection.  */
static int free_one(OM_uint32 i, struct mecherror value, void *p)
{
    free(value.mech.elements);
    return 0;
}

void gssint_mecherrmap_destroy(void)
{
    mecherrmap_foreach(&m, free_one, NULL);
    mecherrmap_destroy(&m);
    k5_mutex_destroy(&mutex);
}

OM_uint32 gssint_mecherrmap_map(OM_uint32 minor, const gss_OID_desc * oid)
{
    const struct mecherror *mep;
    struct mecherror me, me_copy;
    const OM_uint32 *p;
    int err;
    OM_uint32 new_status;

#ifdef DEBUG
    FILE *f;
    f = fopen("/dev/pts/9", "w+");
    if (f == NULL)
        f = stderr;
#endif

    me.code = minor;
    me.mech = *oid;
    k5_mutex_lock(&mutex);

    /* Is this status+oid already mapped?  */
    p = mecherrmap_findright(&m, me);
    if (p != NULL) {
        k5_mutex_unlock(&mutex);
#ifdef DEBUG
        fprintf(f, "%s: found ", __FUNCTION__);
        mecherror_print(me, f);
        fprintf(f, " in map as %lu\n", (unsigned long) *p);
        if (f != stderr) fclose(f);
#endif
        return *p;
    }
    /* Is this status code already mapped to something else
       mech-specific?  */
    mep = mecherrmap_findleft(&m, minor);
    if (mep == NULL) {
        /* Map it to itself plus this mech-oid.  */
        new_status = minor;
    } else {
        /* Already assigned.  Pick a fake new value and map it.  */
        /* There's a theoretical infinite loop risk here, if we fill
           in 2**32 values.  Also, returning 0 has a special
           meaning.  */
        do {
            next_fake++;
            new_status = next_fake;
            if (new_status == 0)
                /* ??? */;
        } while (mecherrmap_findleft(&m, new_status) != NULL);
    }
    err = mecherror_copy(&me_copy, me);
    if (err) {
        k5_mutex_unlock(&mutex);
        return err;
    }
    err = mecherrmap_add(&m, new_status, me_copy);
    k5_mutex_unlock(&mutex);
    if (err)
        free(me_copy.mech.elements);
#ifdef DEBUG
    fprintf(f, "%s: mapping ", __FUNCTION__);
    mecherror_print(me, f);
    fprintf(f, " to %lu: err=%d\nnew map: ", (unsigned long) new_status, err);
    mecherrmap_printmap(&m, f);
    fprintf(f, "\n");
    if (f != stderr) fclose(f);
#endif
    if (err)
        return 0;
    else
        return new_status;
}

static gss_OID_desc no_oid = { 0, 0 };
OM_uint32 gssint_mecherrmap_map_errcode(OM_uint32 errcode)
{
    return gssint_mecherrmap_map(errcode, &no_oid);
}

int gssint_mecherrmap_get(OM_uint32 minor, gss_OID mech_oid,
                          OM_uint32 *mech_minor)
{
    const struct mecherror *p;

    if (minor == 0) {
        return EINVAL;
    }
    k5_mutex_lock(&mutex);
    p = mecherrmap_findleft(&m, minor);
    k5_mutex_unlock(&mutex);
    if (!p) {
        return EINVAL;
    }
    *mech_oid = p->mech;
    *mech_minor = p->code;
    return 0;
}
