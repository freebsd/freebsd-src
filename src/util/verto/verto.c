/*
 * Copyright 2011 Red Hat, Inc.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation files
 * (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#define _GNU_SOURCE /* For asprintf() */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <stdarg.h>

#include <libgen.h>
#include <sys/types.h>
#include <dirent.h>

#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif

#include <verto-module.h>
#include "module.h"

#define  _str(s) # s
#define __str(s) _str(s)

/* Remove flags we can emulate */
#define make_actual(flags) ((flags) & ~(VERTO_EV_FLAG_PERSIST|VERTO_EV_FLAG_IO_CLOSE_FD))

struct verto_ctx {
    size_t ref;
    verto_mod_ctx *ctx;
    const verto_module *module;
    verto_ev *events;
    int deflt;
    int exit;
};

typedef struct {
    verto_proc proc;
    verto_proc_status status;
} verto_child;

typedef struct {
    int fd;
    verto_ev_flag state;
} verto_io;

struct verto_ev {
    verto_ev *next;
    verto_ctx *ctx;
    verto_ev_type type;
    verto_callback *callback;
    verto_callback *onfree;
    void *priv;
    verto_mod_ev *ev;
    verto_ev_flag flags;
    verto_ev_flag actual;
    size_t depth;
    int deleted;
    union {
        verto_io io;
        int signal;
        time_t interval;
        verto_child child;
    } option;
};

typedef struct module_record module_record;
struct module_record {
    module_record *next;
    const verto_module *module;
    void *dll;
    char *filename;
    verto_ctx *defctx;
};


#ifdef BUILTIN_MODULE
#define _MODTABLE(n) verto_module_table_ ## n
#define MODTABLE(n) _MODTABLE(n)
/*
 * This symbol can be used when embedding verto.c in a library along with a
 * built-in private module, to preload the module instead of dynamically
 * linking it in later.  Define to verto_module_table_<modulename>.
 */
extern verto_module MODTABLE(BUILTIN_MODULE);
static module_record builtin_record = {
    NULL, &MODTABLE(BUILTIN_MODULE), NULL, "", NULL
};
static module_record *loaded_modules = &builtin_record;
#else
static module_record *loaded_modules;
#endif

static void *(*resize_cb)(void *mem, size_t size);
static int resize_cb_hierarchical;

#ifdef HAVE_PTHREAD
static pthread_mutex_t loaded_modules_mutex = PTHREAD_MUTEX_INITIALIZER;
#define mutex_lock(x) pthread_mutex_lock(x)
#define mutex_unlock(x) pthread_mutex_unlock(x)
#else
#define mutex_lock(x)
#define mutex_unlock(x)
#endif

#define vfree(mem) vresize(mem, 0)
static void *
vresize(void *mem, size_t size)
{
    if (!resize_cb)
        resize_cb = &realloc;
    return (*resize_cb)(mem, size);
}

#ifndef BUILTIN_MODULE
static int
int_vasprintf(char **strp, const char *fmt, va_list ap) {
    va_list apc;
    int size = 0;

    va_copy(apc, ap);
    size = vsnprintf(NULL, 0, fmt, apc);
    va_end(apc);

    if (size <= 0 || !(*strp = malloc(size + 1)))
        return -1;

    return vsnprintf(*strp, size + 1, fmt, ap);
}

static int
int_asprintf(char **strp, const char *fmt, ...) {
    va_list ap;
    int size = 0;

    va_start(ap, fmt);
    size = int_vasprintf(strp, fmt, ap);
    va_end(ap);
    return size;
}

static char *
int_get_table_name_from_filename(const char *filename)
{
    char *bn = NULL, *tmp = NULL;

    if (!filename)
        return NULL;

    tmp = strdup(filename);
    if (!tmp)
        return NULL;

    bn = basename(tmp);
    if (bn)
        bn = strdup(bn);
    free(tmp);
    if (!bn)
        return NULL;

    tmp = strchr(bn, '-');
    if (tmp) {
        if (strchr(tmp+1, '.')) {
            *strchr(tmp+1, '.') = '\0';
            if (int_asprintf(&tmp, "%s%s", __str(VERTO_MODULE_TABLE()), tmp + 1) < 0)
                tmp = NULL;
        } else
            tmp = NULL;
    }

    free(bn);
    return tmp;
}

typedef struct {
    int reqsym;
    verto_ev_type reqtypes;
} shouldload_data;

static int
shouldload(void *symb, void *misc, char **err)
{
    verto_module *table = (verto_module*) symb;
    shouldload_data *data = (shouldload_data*) misc;

    /* Make sure we have the proper version */
    if (table->vers != VERTO_MODULE_VERSION) {
        if (err)
            *err = strdup("Invalid module version!");
        return 0;
    }

    /* Check to make sure that we have our required symbol if reqsym == true */
    if (table->symb && data->reqsym
            && !module_symbol_is_present(NULL, table->symb)) {
        if (err)
            int_asprintf(err, "Symbol not found: %s!", table->symb);
        return 0;
    }

    /* Check to make sure that this module supports our required features */
    if (data->reqtypes != VERTO_EV_TYPE_NONE
            && (table->types & data->reqtypes) != data->reqtypes) {
        if (err)
            *err = strdup("Module does not support required features!");
        return 0;
    }

    return 1;
}

static int
do_load_file(const char *filename, int reqsym, verto_ev_type reqtypes,
             module_record **record)
{
    char *tblname = NULL, *error = NULL;
    module_record *tmp;
    shouldload_data data  = { reqsym, reqtypes };

    /* Check the loaded modules to see if we already loaded one */
    mutex_lock(&loaded_modules_mutex);
    for (*record = loaded_modules ; *record ; *record = (*record)->next) {
        if (!strcmp((*record)->filename, filename)) {
            mutex_unlock(&loaded_modules_mutex);
            return 1;
        }
    }
    mutex_unlock(&loaded_modules_mutex);

    /* Create our module record */
    tmp = *record = vresize(NULL, sizeof(module_record));
    if (!tmp)
        return 0;
    memset(tmp, 0, sizeof(module_record));
    tmp->filename = strdup(filename);
    if (!tmp->filename) {
        vfree(tmp);
        return 0;
    }

    /* Get the name of the module struct in the library */
    tblname = int_get_table_name_from_filename(filename);
    if (!tblname) {
        free(tblname);
        vfree(tmp);
        return 0;
    }

    /* Load the module */
    error = module_load(filename, tblname, shouldload, &data, &tmp->dll,
                        (void **) &tmp->module);
    if (error || !tmp->dll || !tmp->module) {
        /*if (error)
            fprintf(stderr, "%s\n", error);*/
        free(error);
        module_close(tmp->dll);
        free(tblname);
        vfree(tmp);
        return 0;
    }

    /* Append the new module to the end of the loaded modules */
    mutex_lock(&loaded_modules_mutex);
    for (tmp = loaded_modules ; tmp && tmp->next; tmp = tmp->next)
        continue;
    if (tmp)
        tmp->next = *record;
    else
        loaded_modules = *record;
    mutex_unlock(&loaded_modules_mutex);

    free(tblname);
    return 1;
}

static int
do_load_dir(const char *dirname, const char *prefix, const char *suffix,
            int reqsym, verto_ev_type reqtypes, module_record **record)
{
    DIR *dir;
    struct dirent *ent = NULL;

    *record = NULL;
    dir = opendir(dirname);
    if (!dir)
        return 0;


    while ((ent = readdir(dir))) {
        char *tmp = NULL;
        int success;
        size_t flen, slen;

        flen = strlen(ent->d_name);
        slen = strlen(suffix);

        if (!strcmp(".", ent->d_name) || !strcmp("..", ent->d_name))
            continue;
        if (strstr(ent->d_name, prefix) != ent->d_name)
            continue;
        if (flen < slen || strcmp(ent->d_name + flen - slen, suffix))
            continue;

        if (int_asprintf(&tmp, "%s/%s", dirname, ent->d_name) < 0)
            continue;

        success = do_load_file(tmp, reqsym, reqtypes, record);
        free(tmp);
        if (success)
            break;
        *record = NULL;
    }

    closedir(dir);
    return *record != NULL;
}
#endif

static int
load_module(const char *impl, verto_ev_type reqtypes, module_record **record)
{
    int success = 0;
#ifndef BUILTIN_MODULE
    char *prefix = NULL;
    char *suffix = NULL;
    char *tmp = NULL;
#endif

    /* Check the cache */
    mutex_lock(&loaded_modules_mutex);
    if (impl) {
        for (*record = loaded_modules ; *record ; *record = (*record)->next) {
            if ((strchr(impl, '/') && !strcmp(impl, (*record)->filename))
                    || !strcmp(impl, (*record)->module->name)) {
                mutex_unlock(&loaded_modules_mutex);
                return 1;
            }
        }
    } else if (loaded_modules) {
        for (*record = loaded_modules ; *record ; *record = (*record)->next) {
            if (reqtypes == VERTO_EV_TYPE_NONE
                    || ((*record)->module->types & reqtypes) == reqtypes) {
                mutex_unlock(&loaded_modules_mutex);
                return 1;
            }
        }
    }
    mutex_unlock(&loaded_modules_mutex);

#ifndef BUILTIN_MODULE
    if (!module_get_filename_for_symbol(verto_convert_module, &prefix))
        return 0;

    /* Example output:
     *    prefix == /usr/lib/libverto-
     *    impl == glib
     *    suffix == .so.0
     * Put them all together: /usr/lib/libverto-glib.so.0 */
    tmp = strdup(prefix);
    if (!tmp) {
        free(prefix);
        return 0;
    }

    suffix = basename(tmp);
    suffix = strchr(suffix, '.');
    if (!suffix || strlen(suffix) < 1 || !(suffix = strdup(suffix))) {
        free(prefix);
        free(tmp);
        return 0;
    }
    strcpy(prefix + strlen(prefix) - strlen(suffix), "-");
    free(tmp);

    if (impl) {
        /* Try to do a load by the path */
        if (!success && strchr(impl, '/'))
            success = do_load_file(impl, 0, reqtypes, record);
        if (!success) {
            /* Try to do a load by the name */
            tmp = NULL;
            if (int_asprintf(&tmp, "%s%s%s", prefix, impl, suffix) > 0) {
                success = do_load_file(tmp, 0, reqtypes, record);
                free(tmp);
            }
        }
    } else {
        /* NULL was passed, so we will use the dirname of
         * the prefix to try and find any possible plugins */
        tmp = strdup(prefix);
        if (tmp) {
            char *dname = strdup(dirname(tmp));
            free(tmp);

            tmp = strdup(basename(prefix));
            free(prefix);
            prefix = tmp;

            if (dname && prefix) {
                /* Attempt to find a module we are already linked to */
                success = do_load_dir(dname, prefix, suffix, 1, reqtypes,
                                      record);
                if (!success) {
#ifdef DEFAULT_MODULE
                    /* Attempt to find the default module */
                    success = load_module(DEFAULT_MODULE, reqtypes, record);
                    if (!success)
#endif /* DEFAULT_MODULE */
                        /* Attempt to load any plugin (we're desperate) */
                        success = do_load_dir(dname, prefix, suffix, 0,
                                              reqtypes, record);
                }
            }

            free(dname);
        }
    }

    free(suffix);
    free(prefix);
#endif /* BUILTIN_MODULE */
    return success;
}

static verto_ev *
make_ev(verto_ctx *ctx, verto_callback *callback,
        verto_ev_type type, verto_ev_flag flags)
{
    verto_ev *ev = NULL;

    if (!ctx || !callback)
        return NULL;

    ev = vresize(NULL, sizeof(verto_ev));
    if (ev) {
        memset(ev, 0, sizeof(verto_ev));
        ev->ctx        = ctx;
        ev->type       = type;
        ev->callback   = callback;
        ev->flags      = flags;
    }

    return ev;
}

static void
push_ev(verto_ctx *ctx, verto_ev *ev)
{
    verto_ev *tmp;

    if (!ctx || !ev)
        return;

    tmp = ctx->events;
    ctx->events = ev;
    ctx->events->next = tmp;
}

static void
remove_ev(verto_ev **origin, verto_ev *item)
{
    if (!origin || !*origin || !item)
        return;

    if (*origin == item)
        *origin = (*origin)->next;
    else
        remove_ev(&((*origin)->next), item);
}

static void
signal_ignore(verto_ctx *ctx, verto_ev *ev)
{
}

verto_ctx *
verto_new(const char *impl, verto_ev_type reqtypes)
{
    module_record *mr = NULL;

    if (!load_module(impl, reqtypes, &mr))
        return NULL;

    return verto_convert_module(mr->module, 0, NULL);
}

verto_ctx *
verto_default(const char *impl, verto_ev_type reqtypes)
{
    module_record *mr = NULL;

    if (!load_module(impl, reqtypes, &mr))
        return NULL;

    return verto_convert_module(mr->module, 1, NULL);
}

int
verto_set_default(const char *impl, verto_ev_type reqtypes)
{
    module_record *mr;

    mutex_lock(&loaded_modules_mutex);
    if (loaded_modules || !impl) {
        mutex_unlock(&loaded_modules_mutex);
        return 0;
    }
    mutex_unlock(&loaded_modules_mutex);

    return load_module(impl, reqtypes, &mr);
}

int
verto_set_allocator(void *(*resize)(void *mem, size_t size),
                    int hierarchical)
{
    if (resize_cb || !resize)
        return 0;
    resize_cb = resize;
    resize_cb_hierarchical = hierarchical;
    return 1;
}

void
verto_free(verto_ctx *ctx)
{
    if (!ctx)
        return;

    ctx->ref = ctx->ref > 0 ? ctx->ref - 1 : 0;
    if (ctx->ref > 0)
        return;

    /* Cancel all pending events */
    while (ctx->events)
        verto_del(ctx->events);

    /* Free the private */
    if (!ctx->deflt || !ctx->module->funcs->ctx_default)
        ctx->module->funcs->ctx_free(ctx->ctx);

    vfree(ctx);
}

void
verto_run(verto_ctx *ctx)
{
    if (!ctx)
        return;

    if (ctx->module->funcs->ctx_break && ctx->module->funcs->ctx_run)
        ctx->module->funcs->ctx_run(ctx->ctx);
    else {
        while (!ctx->exit)
            ctx->module->funcs->ctx_run_once(ctx->ctx);
        ctx->exit = 0;
    }
}

void
verto_run_once(verto_ctx *ctx)
{
    if (!ctx)
        return;
    ctx->module->funcs->ctx_run_once(ctx->ctx);
}

void
verto_break(verto_ctx *ctx)
{
    if (!ctx)
        return;

    if (ctx->module->funcs->ctx_break && ctx->module->funcs->ctx_run)
        ctx->module->funcs->ctx_break(ctx->ctx);
    else
        ctx->exit = 1;
}

int
verto_reinitialize(verto_ctx *ctx)
{
    verto_ev *tmp, *next;
    int error = 1;

    if (!ctx)
        return 0;

    /* Delete all events, but keep around the forkable ev structs */
    for (tmp = ctx->events; tmp; tmp = next) {
        next = tmp->next;

        if (tmp->flags & VERTO_EV_FLAG_REINITIABLE)
            ctx->module->funcs->ctx_del(ctx->ctx, tmp, tmp->ev);
        else
            verto_del(tmp);
    }

    /* Reinit the loop */
    if (ctx->module->funcs->ctx_reinitialize)
        ctx->module->funcs->ctx_reinitialize(ctx->ctx);

    /* Recreate events that were marked forkable */
    for (tmp = ctx->events; tmp; tmp = tmp->next) {
        tmp->actual = make_actual(tmp->flags);
        tmp->ev = ctx->module->funcs->ctx_add(ctx->ctx, tmp, &tmp->actual);
        if (!tmp->ev)
            error = 0;
    }

    return error;
}

#define doadd(ev, set, type) \
    ev = make_ev(ctx, callback, type, flags); \
    if (ev) { \
        set; \
        ev->actual = make_actual(ev->flags); \
        ev->ev = ctx->module->funcs->ctx_add(ctx->ctx, ev, &ev->actual); \
        if (!ev->ev) { \
            vfree(ev); \
            return NULL; \
        } \
        push_ev(ctx, ev); \
    }

verto_ev *
verto_add_io(verto_ctx *ctx, verto_ev_flag flags,
             verto_callback *callback, int fd)
{
    verto_ev *ev;

    if (fd < 0 || !(flags & (VERTO_EV_FLAG_IO_READ | VERTO_EV_FLAG_IO_WRITE)))
        return NULL;

    doadd(ev, ev->option.io.fd = fd, VERTO_EV_TYPE_IO);
    return ev;
}

verto_ev *
verto_add_timeout(verto_ctx *ctx, verto_ev_flag flags,
                  verto_callback *callback, time_t interval)
{
    verto_ev *ev;
    doadd(ev, ev->option.interval = interval, VERTO_EV_TYPE_TIMEOUT);
    return ev;
}

verto_ev *
verto_add_idle(verto_ctx *ctx, verto_ev_flag flags,
               verto_callback *callback)
{
    verto_ev *ev;
    doadd(ev,, VERTO_EV_TYPE_IDLE);
    return ev;
}

verto_ev *
verto_add_signal(verto_ctx *ctx, verto_ev_flag flags,
                 verto_callback *callback, int signal)
{
    verto_ev *ev;

    if (signal < 0)
        return NULL;
#ifndef WIN32
    if (signal == SIGCHLD)
        return NULL;
#endif
    if (callback == VERTO_SIG_IGN) {
        callback = signal_ignore;
        if (!(flags & VERTO_EV_FLAG_PERSIST))
            return NULL;
    }
    doadd(ev, ev->option.signal = signal, VERTO_EV_TYPE_SIGNAL);
    return ev;
}

verto_ev *
verto_add_child(verto_ctx *ctx, verto_ev_flag flags,
                verto_callback *callback, verto_proc proc)
{
    verto_ev *ev;

    if (flags & VERTO_EV_FLAG_PERSIST) /* persist makes no sense */
        return NULL;
#ifdef WIN32
    if (proc == NULL)
#else
    if (proc < 1)
#endif
        return NULL;
    doadd(ev, ev->option.child.proc = proc, VERTO_EV_TYPE_CHILD);
    return ev;
}

void
verto_set_private(verto_ev *ev, void *priv, verto_callback *free)
{
    if (!ev)
        return;
    if (ev->onfree && free)
        ev->onfree(ev->ctx, ev);
    ev->priv = priv;
    ev->onfree = free;
}

void *
verto_get_private(const verto_ev *ev)
{
    return ev->priv;
}

verto_ev_type
verto_get_type(const verto_ev *ev)
{
    return ev->type;
}

verto_ev_flag
verto_get_flags(const verto_ev *ev)
{
    return ev->flags;
}

void
verto_set_flags(verto_ev *ev, verto_ev_flag flags)
{
    if (!ev)
        return;

    ev->flags  &= ~_VERTO_EV_FLAG_MUTABLE_MASK;
    ev->flags  |= flags & _VERTO_EV_FLAG_MUTABLE_MASK;

    /* If setting flags isn't supported, just rebuild the event */
    if (!ev->ctx->module->funcs->ctx_set_flags) {
        ev->ctx->module->funcs->ctx_del(ev->ctx->ctx, ev, ev->ev);
        ev->actual = make_actual(ev->flags);
        ev->ev = ev->ctx->module->funcs->ctx_add(ev->ctx->ctx, ev, &ev->actual);
        assert(ev->ev); /* Here is the main reason why modules should */
        return;         /* implement set_flags(): we cannot fail gracefully. */
    }

    ev->actual &= ~_VERTO_EV_FLAG_MUTABLE_MASK;
    ev->actual |= flags & _VERTO_EV_FLAG_MUTABLE_MASK;
    ev->ctx->module->funcs->ctx_set_flags(ev->ctx->ctx, ev, ev->ev);
}

int
verto_get_fd(const verto_ev *ev)
{
    if (ev && (ev->type == VERTO_EV_TYPE_IO))
        return ev->option.io.fd;
    return -1;
}

verto_ev_flag
verto_get_fd_state(const verto_ev *ev)
{
    return ev->option.io.state;
}

time_t
verto_get_interval(const verto_ev *ev)
{
    if (ev && (ev->type == VERTO_EV_TYPE_TIMEOUT))
        return ev->option.interval;
    return 0;
}

int
verto_get_signal(const verto_ev *ev)
{
    if (ev && (ev->type == VERTO_EV_TYPE_SIGNAL))
        return ev->option.signal;
    return -1;
}

verto_proc
verto_get_proc(const verto_ev *ev) {
    if (ev && ev->type == VERTO_EV_TYPE_CHILD)
        return ev->option.child.proc;
    return (verto_proc) 0;
}

verto_proc_status
verto_get_proc_status(const verto_ev *ev)
{
    return ev->option.child.status;
}

verto_ctx *
verto_get_ctx(const verto_ev *ev)
{
    return ev->ctx;
}

void
verto_del(verto_ev *ev)
{
    if (!ev)
        return;

    /* If the event is freed in the callback, we just set a flag so that
     * verto_fire() can actually do the delete when the callback completes.
     *
     * If we don't do this, than verto_fire() will access freed memory. */
    if (ev->depth > 0) {
        ev->deleted = 1;
        return;
    }

    if (ev->onfree)
        ev->onfree(ev->ctx, ev);
    ev->ctx->module->funcs->ctx_del(ev->ctx->ctx, ev, ev->ev);
    remove_ev(&(ev->ctx->events), ev);

    if ((ev->type == VERTO_EV_TYPE_IO) &&
        (ev->flags & VERTO_EV_FLAG_IO_CLOSE_FD) &&
        !(ev->actual & VERTO_EV_FLAG_IO_CLOSE_FD))
        close(ev->option.io.fd);

    vfree(ev);
}

verto_ev_type
verto_get_supported_types(verto_ctx *ctx)
{
    return ctx->module->types;
}

/*** THE FOLLOWING ARE FOR IMPLEMENTATION MODULES ONLY ***/

verto_ctx *
verto_convert_module(const verto_module *module, int deflt, verto_mod_ctx *mctx)
{
    verto_ctx *ctx = NULL;
    module_record *mr;

    if (!module)
        goto error;

    if (deflt) {
        mutex_lock(&loaded_modules_mutex);
        for (mr = loaded_modules ; mr ; mr = mr->next) {
            verto_ctx *tmp;
            if (mr->module == module && mr->defctx) {
                if (mctx)
                    module->funcs->ctx_free(mctx);
                tmp = mr->defctx;
                tmp->ref++;
                mutex_unlock(&loaded_modules_mutex);
                return tmp;
            }
        }
        mutex_unlock(&loaded_modules_mutex);
    }

    if (!mctx) {
        mctx = deflt
                    ? (module->funcs->ctx_default
                        ? module->funcs->ctx_default()
                        : module->funcs->ctx_new())
                    : module->funcs->ctx_new();
        if (!mctx)
            goto error;
    }

    ctx = vresize(NULL, sizeof(verto_ctx));
    if (!ctx)
        goto error;
    memset(ctx, 0, sizeof(verto_ctx));

    ctx->ref = 1;
    ctx->ctx = mctx;
    ctx->module = module;
    ctx->deflt = deflt;

    if (deflt) {
        module_record **tmp;

        mutex_lock(&loaded_modules_mutex);
        tmp = &loaded_modules;
        for (mr = loaded_modules ; mr ; mr = mr->next) {
            if (mr->module == module) {
                assert(mr->defctx == NULL);
                mr->defctx = ctx;
                mutex_unlock(&loaded_modules_mutex);
                return ctx;
            }

            if (!mr->next) {
                tmp = &mr->next;
                break;
            }
        }
        mutex_unlock(&loaded_modules_mutex);

        *tmp = vresize(NULL, sizeof(module_record));
        if (!*tmp) {
            vfree(ctx);
            goto error;
        }

        memset(*tmp, 0, sizeof(module_record));
        (*tmp)->defctx = ctx;
        (*tmp)->module = module;
    }

    return ctx;

error:
    if (mctx)
        module->funcs->ctx_free(mctx);
    return NULL;
}

void
verto_fire(verto_ev *ev)
{
    void *priv;

    ev->depth++;
    ev->callback(ev->ctx, ev);
    ev->depth--;

    if (ev->depth == 0) {
        if (!(ev->flags & VERTO_EV_FLAG_PERSIST) || ev->deleted)
            verto_del(ev);
        else {
            if (!(ev->actual & VERTO_EV_FLAG_PERSIST)) {
                ev->actual = make_actual(ev->flags);
                priv = ev->ctx->module->funcs->ctx_add(ev->ctx->ctx, ev, &ev->actual);
                assert(priv); /* TODO: create an error callback */
                ev->ctx->module->funcs->ctx_del(ev->ctx->ctx, ev, ev->ev);
                ev->ev = priv;
            }

            if (ev->type == VERTO_EV_TYPE_IO)
                ev->option.io.state = VERTO_EV_FLAG_NONE;
            if (ev->type == VERTO_EV_TYPE_CHILD)
                ev->option.child.status = 0;
        }
    }
}

void
verto_set_proc_status(verto_ev *ev, verto_proc_status status)
{
    if (ev && ev->type == VERTO_EV_TYPE_CHILD)
        ev->option.child.status = status;
}

void
verto_set_fd_state(verto_ev *ev, verto_ev_flag state)
{
    /* Filter out only the io flags */
    state = state & (VERTO_EV_FLAG_IO_READ |
                     VERTO_EV_FLAG_IO_WRITE |
                     VERTO_EV_FLAG_IO_ERROR);

    /* Don't report read/write if the socket is closed */
    if (state & VERTO_EV_FLAG_IO_ERROR)
        state = VERTO_EV_FLAG_IO_ERROR;

    if (ev && ev->type == VERTO_EV_TYPE_IO)
        ev->option.io.state = state;
}
