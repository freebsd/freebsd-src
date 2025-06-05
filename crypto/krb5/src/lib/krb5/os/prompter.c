/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include "k5-int.h"
#include "os-proto.h"
#if !defined(_WIN32) || (defined(_WIN32) && defined(__CYGWIN32__))
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>
/* Is vxworks broken w.r.t. termios? --tlyu */
#ifdef __vxworks
#define ECHO_PASSWORD
#endif

#include <termios.h>

#ifdef POSIX_SIGNALS
typedef struct sigaction osiginfo;
#else
typedef struct void (*osiginfo)();
#endif

static void     catch_signals(osiginfo *);
static void     restore_signals(osiginfo *);
static void     intrfunc(int sig);

static krb5_error_code  setup_tty(FILE*, int, struct termios *, osiginfo *);
static krb5_error_code  restore_tty(FILE*, struct termios *, osiginfo *);

static volatile int got_int;    /* should be sig_atomic_t */

krb5_error_code KRB5_CALLCONV
krb5_prompter_posix(
    krb5_context        context,
    void                *data,
    const char          *name,
    const char          *banner,
    int                 num_prompts,
    krb5_prompt         prompts[])
{
    int         fd, i, scratchchar;
    FILE        *fp;
    char        *retp;
    krb5_error_code     errcode;
    struct termios saveparm;
    osiginfo osigint;

    errcode = KRB5_LIBOS_CANTREADPWD;

    if (name) {
        fputs(name, stdout);
        fputs("\n", stdout);
    }
    if (banner) {
        fputs(banner, stdout);
        fputs("\n", stdout);
    }

    /*
     * Get a non-buffered stream on stdin.
     */
    fp = NULL;
    fd = dup(STDIN_FILENO);
    if (fd < 0)
        return KRB5_LIBOS_CANTREADPWD;
    set_cloexec_fd(fd);
    fp = fdopen(fd, "r");
    if (fp == NULL)
        goto cleanup;
    if (setvbuf(fp, NULL, _IONBF, 0))
        goto cleanup;

    for (i = 0; i < num_prompts; i++) {
        errcode = KRB5_LIBOS_CANTREADPWD;
        /* fgets() takes int, but krb5_data.length is unsigned. */
        if (prompts[i].reply->length > INT_MAX)
            goto cleanup;

        errcode = setup_tty(fp, prompts[i].hidden, &saveparm, &osigint);
        if (errcode)
            break;

        /* put out the prompt */
        (void)fputs(prompts[i].prompt, stdout);
        (void)fputs(": ", stdout);
        (void)fflush(stdout);
        (void)memset(prompts[i].reply->data, 0, prompts[i].reply->length);

        got_int = 0;
        retp = fgets(prompts[i].reply->data, (int)prompts[i].reply->length,
                     fp);
        if (prompts[i].hidden)
            putchar('\n');
        if (retp == NULL) {
            if (got_int)
                errcode = KRB5_LIBOS_PWDINTR;
            else
                errcode = KRB5_LIBOS_CANTREADPWD;
            restore_tty(fp, &saveparm, &osigint);
            break;
        }

        /* replace newline with null */
        retp = strchr(prompts[i].reply->data, '\n');
        if (retp != NULL)
            *retp = '\0';
        else {
            /* flush rest of input line */
            do {
                scratchchar = getc(fp);
            } while (scratchchar != EOF && scratchchar != '\n');
        }

        errcode = restore_tty(fp, &saveparm, &osigint);
        if (errcode)
            break;
        prompts[i].reply->length = strlen(prompts[i].reply->data);
    }
cleanup:
    if (fp != NULL)
        fclose(fp);
    else if (fd >= 0)
        close(fd);

    return errcode;
}

static void
intrfunc(int sig)
{
    got_int = 1;
}

static void
catch_signals(osiginfo *osigint)
{
#ifdef POSIX_SIGNALS
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = intrfunc;
    sigaction(SIGINT, &sa, osigint);
#else
    *osigint = signal(SIGINT, intrfunc);
#endif
}

static void
restore_signals(osiginfo *osigint)
{
#ifdef POSIX_SIGNALS
    sigaction(SIGINT, osigint, NULL);
#else
    signal(SIGINT, *osigint);
#endif
}

static krb5_error_code
setup_tty(FILE *fp, int hidden, struct termios *saveparm, osiginfo *osigint)
{
    krb5_error_code     ret;
    int                 fd;
    struct termios      tparm;

    ret = KRB5_LIBOS_CANTREADPWD;
    catch_signals(osigint);
    fd = fileno(fp);
    do {
        if (!isatty(fd)) {
            ret = 0;
            break;
        }
        if (tcgetattr(fd, &tparm) < 0)
            break;
        *saveparm = tparm;
#ifndef ECHO_PASSWORD
        if (hidden)
            tparm.c_lflag &= ~(ECHO|ECHONL);
#endif
        tparm.c_lflag |= ISIG|ICANON;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &tparm) < 0)
            break;
        ret = 0;
    } while (0);
    /* If we're losing, restore signal handlers. */
    if (ret)
        restore_signals(osigint);
    return ret;
}

static krb5_error_code
restore_tty(FILE* fp, struct termios *saveparm, osiginfo *osigint)
{
    int ret, fd;

    ret = 0;
    fd = fileno(fp);
    if (isatty(fd)) {
        ret = tcsetattr(fd, TCSANOW, saveparm);
        if (ret < 0)
            ret = KRB5_LIBOS_CANTREADPWD;
        else
            ret = 0;
    }
    restore_signals(osigint);
    return ret;
}

#else /* non-Cygwin Windows, or Mac */

#if defined(_WIN32)

#include <io.h>

krb5_error_code KRB5_CALLCONV
krb5_prompter_posix(krb5_context context,
                    void *data,
                    const char *name,
                    const char *banner,
                    int num_prompts,
                    krb5_prompt prompts[])
{
    HANDLE              handle;
    DWORD               old_mode, new_mode;
    char                *ptr;
    int                 scratchchar;
    krb5_error_code     errcode = 0;
    int                 i;

    handle = GetStdHandle(STD_INPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE)
        return ENOTTY;
    if (!GetConsoleMode(handle, &old_mode))
        return ENOTTY;

    new_mode = old_mode;
    new_mode |=  ( ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT );
    new_mode &= ~( ENABLE_ECHO_INPUT );

    if (!SetConsoleMode(handle, new_mode))
        return ENOTTY;

    if (!SetConsoleMode(handle, old_mode))
        return ENOTTY;

    if (name) {
        fputs(name, stdout);
        fputs("\n", stdout);
    }

    if (banner) {
        fputs(banner, stdout);
        fputs("\n", stdout);
    }

    for (i = 0; i < num_prompts; i++) {
        if (prompts[i].hidden) {
            if (!SetConsoleMode(handle, new_mode)) {
                errcode = ENOTTY;
                goto cleanup;
            }
        }

        fputs(prompts[i].prompt,stdout);
        fputs(": ", stdout);
        fflush(stdout);
        memset(prompts[i].reply->data, 0, prompts[i].reply->length);

        if (fgets(prompts[i].reply->data, prompts[i].reply->length, stdin)
            == NULL) {
            if (prompts[i].hidden)
                putchar('\n');
            errcode = KRB5_LIBOS_CANTREADPWD;
            goto cleanup;
        }
        if (prompts[i].hidden)
            putchar('\n');
        /* fgets always null-terminates the returned string */

        /* replace newline with null */
        if ((ptr = strchr(prompts[i].reply->data, '\n')))
            *ptr = '\0';
        else /* flush rest of input line */
            do {
                scratchchar = getchar();
            } while (scratchchar != EOF && scratchchar != '\n');

        prompts[i].reply->length = strlen(prompts[i].reply->data);

        if (!SetConsoleMode(handle, old_mode)) {
            errcode = ENOTTY;
            goto cleanup;
        }
    }

cleanup:
    if (errcode) {
        for (i = 0; i < num_prompts; i++) {
            memset(prompts[i].reply->data, 0, prompts[i].reply->length);
        }
    }
    return errcode;
}

#else /* !_WIN32 */

krb5_error_code KRB5_CALLCONV
krb5_prompter_posix(krb5_context context,
                    void *data,
                    const char *name,
                    const char *banner,
                    int num_prompts,
                    krb5_prompt prompts[])
{
    return(EINVAL);
}
#endif /* !_WIN32 */
#endif /* Windows or Mac */

void
k5_set_prompt_types(krb5_context context, krb5_prompt_type *types)
{
    context->prompt_types = types;
}

krb5_prompt_type*
KRB5_CALLCONV
krb5_get_prompt_types(krb5_context context)
{
    return context->prompt_types;
}
