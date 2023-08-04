/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1997,2000,2001,2004,2008 by Massachusetts Institute of Technology
 *
 * Copyright 1987, 1988 by MIT Student Information Processing Board
 *
 * Permission to use, copy, modify, and distribute this software
 * and its documentation for any purpose and without fee is
 * hereby granted, provided that the above copyright notice
 * appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation,
 * and that the names of M.I.T. and the M.I.T. S.I.P.B. not be
 * used in advertising or publicity pertaining to distribution
 * of the software without specific, written prior permission.
 * Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. and the M.I.T. S.I.P.B. make no representations about
 * the suitability of this software for any purpose.  It is
 * provided "as is" without express or implied warranty.
 */

#include "k5-platform.h"
#include "com_err.h"
#include "error_table.h"

static struct et_list *et_list;
static k5_mutex_t et_list_lock = K5_MUTEX_PARTIAL_INITIALIZER;
static int terminated = 0;      /* for safety and finalization debugging */

MAKE_INIT_FUNCTION(com_err_initialize);
MAKE_FINI_FUNCTION(com_err_terminate);

int com_err_initialize(void)
{
    int err;
#ifdef SHOW_INITFINI_FUNCS
    printf("com_err_initialize\n");
#endif
    terminated = 0;
    err = k5_mutex_finish_init(&et_list_lock);
    if (err)
        return err;
    err = k5_mutex_finish_init(&com_err_hook_lock);
    if (err)
        return err;
    err = k5_key_register(K5_KEY_COM_ERR, free);
    if (err)
        return err;
    return 0;
}

void com_err_terminate(void)
{
    struct et_list *e, *enext;
    if (! INITIALIZER_RAN(com_err_initialize) || PROGRAM_EXITING()) {
#ifdef SHOW_INITFINI_FUNCS
        printf("com_err_terminate: skipping\n");
#endif
        return;
    }
#ifdef SHOW_INITFINI_FUNCS
    printf("com_err_terminate\n");
#endif
    k5_key_delete(K5_KEY_COM_ERR);
    k5_mutex_destroy(&com_err_hook_lock);
    k5_mutex_lock(&et_list_lock);
    for (e = et_list; e; e = enext) {
        enext = e->next;
        free(e);
    }
    et_list = NULL;
    k5_mutex_unlock(&et_list_lock);
    k5_mutex_destroy(&et_list_lock);
    terminated = 1;
}

#ifndef DEBUG_TABLE_LIST
#define dprintf(X)
#else
#define dprintf(X) printf X
#endif

static char *
get_thread_buffer ()
{
    char *cp;
    cp = k5_getspecific(K5_KEY_COM_ERR);
    if (cp == NULL) {
        cp = malloc(ET_EBUFSIZ);
        if (cp == NULL) {
            return NULL;
        }
        if (k5_setspecific(K5_KEY_COM_ERR, cp) != 0) {
            free(cp);
            return NULL;
        }
    }
    return cp;
}

const char * KRB5_CALLCONV
error_message(long code)
{
    unsigned long offset;
    unsigned long l_offset;
    struct et_list *e;
    unsigned long table_num;
    int started = 0;
    unsigned int divisor = 100;
    char *cp, *cp1;
    const struct error_table *table;

    if (CALL_INIT_FUNCTION(com_err_initialize))
        return 0;

    l_offset = (unsigned long)code & ((1<<ERRCODE_RANGE)-1);
    offset = l_offset;
    table_num = ((unsigned long)code - l_offset) & ERRCODE_MAX;
    if (table_num == 0
#ifdef __sgi
        /* Irix 6.5 uses a much bigger table than other UNIX
           systems I've looked at, but the table is sparse.  The
           sparse entries start around 500, but sys_nerr is only
           152.  */
        || (code > 0 && code <= 1600)
#endif
    ) {
        if (code == 0)
            goto oops;

        /* This could trip if int is 16 bits.  */
        if ((unsigned long)(int)code != (unsigned long)code)
            abort ();
        cp = get_thread_buffer();
        if (cp && strerror_r(code, cp, ET_EBUFSIZ) == 0)
            return cp;
        return strerror(code);
    }

    k5_mutex_lock(&et_list_lock);
    dprintf(("scanning list for %x\n", table_num));
    for (e = et_list; e != NULL; e = e->next) {
        dprintf(("\t%x = %s\n", e->table->base & ERRCODE_MAX,
                 e->table->msgs[0]));
        if ((e->table->base & ERRCODE_MAX) == table_num) {
            table = e->table;
            goto found;
        }
    }
    goto no_table_found;

found:
    k5_mutex_unlock(&et_list_lock);
    dprintf (("found it!\n"));
    /* This is the right table */

    /* This could trip if int is 16 bits.  */
    if ((unsigned long)(unsigned int)offset != offset)
        goto no_table_found;

    if (table->n_msgs <= (unsigned int) offset)
        goto no_table_found;

    /* If there's a string at the end of the table, it's a text domain. */
    if (table->msgs[table->n_msgs] != NULL)
        return dgettext(table->msgs[table->n_msgs], table->msgs[offset]);
    else
        return table->msgs[offset];

no_table_found:
    k5_mutex_unlock(&et_list_lock);
#if defined(_WIN32)
    /*
     * WinSock errors exist in the 10000 and 11000 ranges
     * but might not appear if WinSock is not initialized
     */
    if (code >= WSABASEERR && code < WSABASEERR + 1100) {
        table_num = 0;
        offset = code;
        divisor = WSABASEERR;
    }
#endif
#ifdef _WIN32
    {
        LPVOID msgbuf;

        if (! FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                             NULL /* lpSource */,
                             (DWORD) code,
                             MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                             (LPTSTR) &msgbuf,
                             (DWORD) 0 /*sizeof(buffer)*/,
                             NULL /* va_list */ )) {
            /*
             * WinSock errors exist in the 10000 and 11000 ranges
             * but might not appear if WinSock is not initialized
             */
            if (code >= WSABASEERR && code < WSABASEERR + 1100) {
                table_num = 0;
                offset = code;
                divisor = 10000;
            }

            goto oops;
        } else {
            char *buffer;
            cp = get_thread_buffer();
            if (cp == NULL)
                return "Unknown error code";
            buffer = cp;
            strncpy(buffer, msgbuf, ET_EBUFSIZ);
            buffer[ET_EBUFSIZ-1] = '\0';
            cp = buffer + strlen(buffer) - 1;
            if (*cp == '\n') *cp-- = '\0';
            if (*cp == '\r') *cp-- = '\0';
            if (*cp == '.') *cp-- = '\0';

            LocalFree(msgbuf);
            return buffer;
        }
    }
#endif

oops:

    cp = get_thread_buffer();
    if (cp == NULL)
        return "Unknown error code";
    cp1 = cp;
    strlcpy(cp, "Unknown code ", ET_EBUFSIZ);
    cp += sizeof("Unknown code ") - 1;
    if (table_num != 0L) {
        (void) error_table_name_r(table_num, cp);
        while (*cp != '\0')
            cp++;
        *cp++ = ' ';
    }
    while (divisor > 1) {
        if (started != 0 || offset >= divisor) {
            *cp++ = '0' + offset / divisor;
            offset %= divisor;
            started++;
        }
        divisor /= 10;
    }
    *cp++ = '0' + offset;
    *cp = '\0';
    return(cp1);
}

errcode_t KRB5_CALLCONV
add_error_table(const struct error_table *et)
{
    struct et_list *e;

    if (CALL_INIT_FUNCTION(com_err_initialize))
        return 0;

    e = malloc(sizeof(struct et_list));
    if (e == NULL)
        return ENOMEM;

    e->table = et;

    k5_mutex_lock(&et_list_lock);
    e->next = et_list;
    et_list = e;

    /* If there are two strings at the end of the table, they are a text domain
     * and locale dir, and we are supposed to call bindtextdomain. */
    if (et->msgs[et->n_msgs] != NULL && et->msgs[et->n_msgs + 1] != NULL)
        bindtextdomain(et->msgs[et->n_msgs], et->msgs[et->n_msgs + 1]);

    k5_mutex_unlock(&et_list_lock);
    return 0;
}

errcode_t KRB5_CALLCONV
remove_error_table(const struct error_table *et)
{
    struct et_list **ep, *e;

    /* Safety check in case libraries are finalized in the wrong order. */
    if (terminated)
        return ENOENT;

    if (CALL_INIT_FUNCTION(com_err_initialize))
        return 0;
    k5_mutex_lock(&et_list_lock);

    /* Remove the entry that matches the error table instance. */
    for (ep = &et_list; *ep; ep = &(*ep)->next) {
        if ((*ep)->table == et) {
            e = *ep;
            *ep = e->next;
            free(e);
            k5_mutex_unlock(&et_list_lock);
            return 0;
        }
    }
    k5_mutex_unlock(&et_list_lock);
    return ENOENT;
}

int com_err_finish_init()
{
    return CALL_INIT_FUNCTION(com_err_initialize);
}
