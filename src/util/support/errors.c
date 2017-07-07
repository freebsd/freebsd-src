/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Can't include krb5.h here, or k5-int.h which includes it, because krb5.h
 * needs to be generated with error tables, after util/et, which builds after
 * this directory.
 */
#include "k5-platform.h"
#include "k5-err.h"
#include "k5-thread.h"
#include "supp-int.h"

/*
 * It would be nice to just use error_message() always.  Pity that it's defined
 * in a library that depends on this one, and we're not allowed to make
 * circular dependencies.
 */
/*
 * We really want a rwlock here, since we should hold it while calling the
 * function and copying out its results.  But I haven't implemented shims for
 * rwlock yet.
 */
static k5_mutex_t krb5int_error_info_support_mutex =
    K5_MUTEX_PARTIAL_INITIALIZER;
static const char *(KRB5_CALLCONV *fptr)(long); /* = &error_message */

/* Fallback error message if we cannot allocate a copy of the real one. */
static char *oom_msg = "Out of memory";

int
krb5int_err_init (void)
{
    return k5_mutex_finish_init(&krb5int_error_info_support_mutex);
}
#define initialize()    krb5int_call_thread_support_init()
#define lock()          k5_mutex_lock(&krb5int_error_info_support_mutex)
#define unlock()        k5_mutex_unlock(&krb5int_error_info_support_mutex)

void
k5_set_error(struct errinfo *ep, long code, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    k5_vset_error(ep, code, fmt, args);
    va_end(args);
}

void
k5_vset_error(struct errinfo *ep, long code, const char *fmt, va_list args)
{
    char *str;

    k5_clear_error(ep);
    ep->code = code;

    if (vasprintf(&str, fmt, args) < 0)
        return;
    ep->msg = str;
}

static inline const char *
oom_check(const char *str)
{
    return (str == NULL) ? oom_msg : str;
}

const char *
k5_get_error(struct errinfo *ep, long code)
{
    const char *r;
    char buf[128];

    if (code == ep->code && ep->msg != NULL)
        return oom_check(strdup(ep->msg));

    if (initialize())
        return oom_check(strdup(_("Kerberos library initialization failure")));

    lock();
    if (fptr == NULL) {
        unlock();
        if (strerror_r(code, buf, sizeof(buf)) == 0)
            return oom_check(strdup(buf));
        return oom_check(strdup(strerror(code)));
    }
    r = fptr(code);
#ifndef HAVE_COM_ERR_INTL
    /* Translate com_err results here if libcom_err won't do it. */
    r = _(r);
#endif
    if (r == NULL) {
        unlock();
        snprintf(buf, sizeof(buf), _("error %ld"), code);
        return oom_check(strdup(buf));
    }

    r = strdup(r);
    unlock();
    return oom_check(r);
}

void
k5_free_error(struct errinfo *ep, const char *msg)
{
    if (msg != oom_msg)
        free((char *)msg);
}

void
k5_clear_error(struct errinfo *ep)
{
    k5_free_error(ep, ep->msg);
    ep->msg = NULL;
}

void
k5_set_error_info_callout_fn(const char *(KRB5_CALLCONV *f)(long))
{
    initialize();
    lock();
    fptr = f;
    unlock();
}
