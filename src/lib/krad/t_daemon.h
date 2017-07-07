/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krad/t_daemon.h - Daemonization helper for RADIUS test programs */
/*
 * Copyright 2013 Red Hat, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef T_DAEMON_H_
#define T_DAEMON_H_

#include "t_test.h"
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static pid_t daemon_pid;

static void
daemon_stop(void)
{
    if (daemon_pid == 0)
        return;
    kill(daemon_pid, SIGTERM);
    waitpid(daemon_pid, NULL, 0);
    daemon_pid = 0;
}

static krb5_boolean
daemon_start(int argc, const char **argv)
{
    int fds[2];
    char buf[1];

    if (argc != 3 || argv == NULL)
        return FALSE;

    if (daemon_pid != 0)
        return TRUE;

    if (pipe(fds) != 0)
        return FALSE;

    /* Start the child process with the write end of the pipe as stdout. */
    daemon_pid = fork();
    if (daemon_pid == 0) {
        dup2(fds[1], STDOUT_FILENO);
        close(fds[0]);
        close(fds[1]);
        exit(execlp(argv[1], argv[1], argv[2], NULL));
    }
    close(fds[1]);

    /* The child will write a sentinel character when it is listening. */
    if (read(fds[0], buf, 1) != 1 || *buf != '~')
        return FALSE;
    close(fds[0]);

    atexit(daemon_stop);
    return TRUE;
}

#endif /* T_DAEMON_H_ */
