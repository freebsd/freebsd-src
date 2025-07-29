/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* util/ss/listen.c */
/*
 * Copyright 1987, 1988 by MIT Student Information Processing Board
 *
 * For copyright information, see copyright.h.
 */

#include "copyright.h"
#include "ss_internal.h"
#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <termios.h>
#include <sys/param.h>

#if defined(HAVE_LIBEDIT)
#include <editline/readline.h>
#elif defined(HAVE_READLINE)
#include <readline/readline.h>
#include <readline/history.h>
#else
#define NO_READLINE
#endif

static ss_data *current_info;
static jmp_buf listen_jmpb;

#ifdef NO_READLINE
/* Dumb replacement for readline when we don't have support for a real one. */
static char *readline(const char *prompt)
{
    struct termios termbuf;
    char input[BUFSIZ];

    /* Make sure we don't buffer anything beyond the line read. */
    setvbuf(stdin, 0, _IONBF, 0);

    if (tcgetattr(STDIN_FILENO, &termbuf) == 0) {
        termbuf.c_lflag |= ICANON|ISIG|ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &termbuf);
    }
    printf("%s", prompt);
    fflush(stdout);
    if (fgets(input, BUFSIZ, stdin) == NULL)
        return NULL;
    input[strcspn(input, "\r\n")] = '\0';
    return strdup(input);
}

/* No-op replacement for add_history() when we have no readline support. */
static void add_history(const char *line)
{
}
#endif

static void listen_int_handler(signo)
    int signo;
{
    putc('\n', stdout);
    longjmp(listen_jmpb, 1);
}

int ss_listen (sci_idx)
    int sci_idx;
{
    char *cp;
    ss_data *info;
    char *input;
    int code;
    jmp_buf old_jmpb;
    ss_data *old_info = current_info;
#ifdef POSIX_SIGNALS
    struct sigaction isig, csig, nsig, osig;
    sigset_t nmask, omask;
#else
    void (*sig_cont)();
    void (*sig_int)(), (*old_sig_cont)();
    int mask;
#endif

    current_info = info = ss_info(sci_idx);
    info->abort = 0;

#ifdef POSIX_SIGNALS
    csig.sa_handler = (void (*)())0;
    sigemptyset(&nmask);
    sigaddset(&nmask, SIGINT);
    sigprocmask(SIG_BLOCK, &nmask, &omask);
#else
    sig_cont = (void (*)())0;
    mask = sigblock(sigmask(SIGINT));
#endif

    memcpy(old_jmpb, listen_jmpb, sizeof(jmp_buf));

#ifdef POSIX_SIGNALS
    nsig.sa_handler = listen_int_handler;
    sigemptyset(&nsig.sa_mask);
    nsig.sa_flags = 0;
    sigaction(SIGINT, &nsig, &isig);
#else
    sig_int = signal(SIGINT, listen_int_handler);
#endif

    setjmp(listen_jmpb);

#ifdef POSIX_SIGNALS
    sigprocmask(SIG_SETMASK, &omask, (sigset_t *)0);
#else
    (void) sigsetmask(mask);
#endif
    while(!info->abort) {
#ifdef POSIX_SIGNALS
        nsig.sa_handler = listen_int_handler;   /* fgets is not signal-safe */
        osig = csig;
        sigaction(SIGCONT, &nsig, &csig);
        if ((void (*)())csig.sa_handler==(void (*)())listen_int_handler)
            csig = osig;
#else
        old_sig_cont = sig_cont;
        sig_cont = signal(SIGCONT, listen_int_handler);
        if (sig_cont == listen_int_handler)
            sig_cont = old_sig_cont;
#endif

        input = readline(current_info->prompt);
        if (input == NULL) {
            code = SS_ET_EOF;
            goto egress;
        }
        add_history(input);

#ifdef POSIX_SIGNALS
        sigaction(SIGCONT, &csig, (struct sigaction *)0);
#else
        (void) signal(SIGCONT, sig_cont);
#endif

        code = ss_execute_line (sci_idx, input);
        if (code == SS_ET_COMMAND_NOT_FOUND) {
            char *c = input;
            while (*c == ' ' || *c == '\t')
                c++;
            cp = strchr (c, ' ');
            if (cp)
                *cp = '\0';
            cp = strchr (c, '\t');
            if (cp)
                *cp = '\0';
            ss_error (sci_idx, 0,
                      "Unknown request \"%s\".  Type \"?\" for a request list.",
                      c);
        }
        free(input);
    }
    code = 0;
egress:
#ifdef POSIX_SIGNALS
    sigaction(SIGINT, &isig, (struct sigaction *)0);
#else
    (void) signal(SIGINT, sig_int);
#endif
    memcpy(listen_jmpb, old_jmpb, sizeof(jmp_buf));
    current_info = old_info;
    return code;
}

void ss_abort_subsystem(sci_idx, code)
    int sci_idx;
    int code;
{
    ss_info(sci_idx)->abort = 1;
    ss_info(sci_idx)->exit_status = code;

}

void ss_quit(argc, argv, sci_idx, infop)
    int argc;
    char const * const *argv;
    int sci_idx;
    pointer infop;
{
    ss_abort_subsystem(sci_idx, 0);
}
