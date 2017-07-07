/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/rcache/rc_base.c */
/*
 * This file of the Kerberos V5 software is derived from public-domain code
 * contributed by Daniel J. Bernstein, <brnstnd@acf10.nyu.edu>.
 *
 */

/*
 * Base "glue" functions for the replay cache.
 */

#include "rc_base.h"
#include "rc-int.h"
#include "k5-thread.h"

struct krb5_rc_typelist {
    const krb5_rc_ops *ops;
    struct krb5_rc_typelist *next;
};
static struct krb5_rc_typelist none = { &krb5_rc_none_ops, 0 };
static struct krb5_rc_typelist krb5_rc_typelist_dfl = { &krb5_rc_dfl_ops, &none };
static struct krb5_rc_typelist *typehead = &krb5_rc_typelist_dfl;
static k5_mutex_t rc_typelist_lock = K5_MUTEX_PARTIAL_INITIALIZER;

int
krb5int_rc_finish_init(void)
{
    return k5_mutex_finish_init(&rc_typelist_lock);
}

void
krb5int_rc_terminate(void)
{
    struct krb5_rc_typelist *t, *t_next;
    k5_mutex_destroy(&rc_typelist_lock);
    for (t = typehead; t != &krb5_rc_typelist_dfl; t = t_next) {
        t_next = t->next;
        free(t);
    }
}

krb5_error_code
krb5_rc_register_type(krb5_context context, const krb5_rc_ops *ops)
{
    struct krb5_rc_typelist *t;

    k5_mutex_lock(&rc_typelist_lock);
    for (t = typehead;t && strcmp(t->ops->type,ops->type);t = t->next)
        ;
    if (t) {
        k5_mutex_unlock(&rc_typelist_lock);
        return KRB5_RC_TYPE_EXISTS;
    }
    t = (struct krb5_rc_typelist *) malloc(sizeof(struct krb5_rc_typelist));
    if (t == NULL) {
        k5_mutex_unlock(&rc_typelist_lock);
        return KRB5_RC_MALLOC;
    }
    t->next = typehead;
    t->ops = ops;
    typehead = t;
    k5_mutex_unlock(&rc_typelist_lock);
    return 0;
}

krb5_error_code
krb5_rc_resolve_type(krb5_context context, krb5_rcache *idptr,
                     const char *type)
{
    struct krb5_rc_typelist *t;
    krb5_error_code err;
    krb5_rcache id;

    *idptr = NULL;

    /* Find the named type in the list. */
    k5_mutex_lock(&rc_typelist_lock);
    for (t = typehead; t && strcmp(t->ops->type, type); t = t->next)
        ;
    k5_mutex_unlock(&rc_typelist_lock);
    if (!t)
        return KRB5_RC_TYPE_NOTFOUND;

    /* Create and return the rcache structure. */
    id = malloc(sizeof(*id));
    if (!id)
        return KRB5_RC_MALLOC;
    err = k5_mutex_init(&id->lock);
    if (err) {
        free(id);
        return err;
    }
    id->data = NULL;  /* Gets real data when resolved */
    id->magic = 0;    /* Gets real magic after resolved */
    id->ops = t->ops;
    *idptr = id;
    return 0;
}

char * krb5_rc_get_type(krb5_context context, krb5_rcache id)
{
    return id->ops->type;
}

char *
krb5_rc_default_type(krb5_context context)
{
    char *s;
    if ((s = getenv("KRB5RCACHETYPE")))
        return s;
    else
        return "dfl";
}

char *
krb5_rc_default_name(krb5_context context)
{
    char *s;
    if ((s = getenv("KRB5RCACHENAME")))
        return s;
    else
        return (char *) 0;
}

krb5_error_code
krb5_rc_default(krb5_context context, krb5_rcache *idptr)
{
    krb5_error_code retval;
    krb5_rcache id;

    *idptr = NULL;
    retval = krb5_rc_resolve_type(context, &id, krb5_rc_default_type(context));
    if (retval)
        return retval;
    retval = krb5_rc_resolve(context, id, krb5_rc_default_name(context));
    if (retval) {
        k5_mutex_destroy(&id->lock);
        free(id);
        return retval;
    }
    id->magic = KV5M_RCACHE;
    *idptr = id;
    return retval;
}


krb5_error_code
krb5_rc_resolve_full(krb5_context context, krb5_rcache *idptr,
                     const char *string_name)
{
    char *type;
    char *residual;
    krb5_error_code retval;
    unsigned int diff;
    krb5_rcache id;

    *idptr = NULL;

    if (!(residual = strchr(string_name,':')))
        return KRB5_RC_PARSE;

    diff = residual - string_name;
    if (!(type = malloc(diff + 1)))
        return KRB5_RC_MALLOC;
    (void) strncpy(type, string_name, diff);
    type[residual - string_name] = '\0';

    retval = krb5_rc_resolve_type(context, &id,type);
    free(type);
    if (retval)
        return retval;
    if ((retval = krb5_rc_resolve(context, id,residual + 1))) {
        k5_mutex_destroy(&id->lock);
        free(id);
        return retval;
    }
    id->magic = KV5M_RCACHE;
    *idptr = id;
    return retval;
}
