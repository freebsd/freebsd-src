/*
 * Utility functions for tests that use subprocesses.
 *
 * Provides utility functions for subprocess manipulation.  Specifically,
 * provides a function, run_setup, which runs a command and bails if it fails,
 * using its error message as the bail output, and is_function_output, which
 * runs a function in a subprocess and checks its output and exit status
 * against expected values.
 *
 * Requires an Autoconf probe for sys/select.h and a replacement for a missing
 * mkstemp.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2002, 2004-2005, 2013, 2016-2017 Russ Allbery <eagle@eyrie.org>
 * Copyright 2009-2011, 2013-2014
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

#include <config.h>
#include <portable/system.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#ifdef HAVE_SYS_SELECT_H
#    include <sys/select.h>
#endif
#include <sys/stat.h>
#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif
#include <sys/wait.h>
#include <time.h>

#include <tests/tap/basic.h>
#include <tests/tap/process.h>
#include <tests/tap/string.h>

/* May be defined by the build system. */
#ifndef PATH_FAKEROOT
#    define PATH_FAKEROOT ""
#endif

/* How long to wait for the process to start in seconds. */
#define PROCESS_WAIT 10

/*
 * Used to store information about a background process.  This contains
 * everything required to stop the process and clean up after it.
 */
struct process {
    pid_t pid;            /* PID of child process */
    char *pidfile;        /* PID file to delete on process stop */
    char *tmpdir;         /* Temporary directory for log file */
    char *logfile;        /* Log file of process output */
    bool is_child;        /* Whether we can waitpid for process */
    struct process *next; /* Next process in global list */
};

/*
 * Global list of started processes, which will be cleaned up automatically on
 * program exit if they haven't been explicitly stopped with process_stop
 * prior to that point.
 */
static struct process *processes = NULL;


/*
 * Given a function, an expected exit status, and expected output, runs that
 * function in a subprocess, capturing stdout and stderr via a pipe, and
 * returns the function output in newly allocated memory.  Also captures the
 * process exit status.
 */
static void
run_child_function(test_function_type function, void *data, int *status,
                   char **output)
{
    int fds[2];
    pid_t child;
    char *buf;
    ssize_t count, ret, buflen;
    int rval;

    /* Flush stdout before we start to avoid odd forking issues. */
    fflush(stdout);

    /* Set up the pipe and call the function, collecting its output. */
    if (pipe(fds) == -1)
        sysbail("can't create pipe");
    child = fork();
    if (child == (pid_t) -1) {
        sysbail("can't fork");
    } else if (child == 0) {
        /* In child.  Set up our stdout and stderr. */
        close(fds[0]);
        if (dup2(fds[1], 1) == -1)
            _exit(255);
        if (dup2(fds[1], 2) == -1)
            _exit(255);

        /* Now, run the function and exit successfully if it returns. */
        (*function)(data);
        fflush(stdout);
        _exit(0);
    } else {
        /*
         * In the parent; close the extra file descriptor, read the output if
         * any, and then collect the exit status.
         */
        close(fds[1]);
        buflen = BUFSIZ;
        buf = bmalloc(buflen);
        count = 0;
        do {
            ret = read(fds[0], buf + count, buflen - count - 1);
            if (SSIZE_MAX - count <= ret)
                bail("maximum output size exceeded in run_child_function");
            if (ret > 0)
                count += ret;
            if (count >= buflen - 1) {
                buflen += BUFSIZ;
                buf = brealloc(buf, buflen);
            }
        } while (ret > 0);
        buf[count] = '\0';
        if (waitpid(child, &rval, 0) == (pid_t) -1)
            sysbail("waitpid failed");
        close(fds[0]);
    }

    /* Store the output and return. */
    *status = rval;
    *output = buf;
}


/*
 * Given a function, data to pass to that function, an expected exit status,
 * and expected output, runs that function in a subprocess, capturing stdout
 * and stderr via a pipe, and compare the combination of stdout and stderr
 * with the expected output and the exit status with the expected status.
 * Expects the function to always exit (not die from a signal).
 */
void
is_function_output(test_function_type function, void *data, int status,
                   const char *output, const char *format, ...)
{
    char *buf, *msg;
    int rval;
    va_list args;

    run_child_function(function, data, &rval, &buf);

    /* Now, check the results against what we expected. */
    va_start(args, format);
    bvasprintf(&msg, format, args);
    va_end(args);
    ok(WIFEXITED(rval), "%s (exited)", msg);
    is_int(status, WEXITSTATUS(rval), "%s (status)", msg);
    is_string(output, buf, "%s (output)", msg);
    free(buf);
    free(msg);
}


/*
 * A helper function for run_setup.  This is a function to run an external
 * command, suitable for passing into run_child_function.  The expected
 * argument must be an argv array, with argv[0] being the command to run.
 */
static void
exec_command(void *data)
{
    char *const *argv = data;

    execvp(argv[0], argv);
}


/*
 * Given a command expressed as an argv struct, with argv[0] the name or path
 * to the command, run that command.  If it exits with a non-zero status, use
 * the part of its output up to the first newline as the error message when
 * calling bail.
 */
void
run_setup(const char *const argv[])
{
    char *output, *p;
    int status;

    run_child_function(exec_command, (void *) argv, &status, &output);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        p = strchr(output, '\n');
        if (p != NULL)
            *p = '\0';
        if (output[0] != '\0')
            bail("%s", output);
        else
            bail("setup command failed with no output");
    }
    free(output);
}


/*
 * Free the resources associated with tracking a process, without doing
 * anything to the process.  This is kept separate so that we can free
 * resources during shutdown in a non-primary process.
 */
static void
process_free(struct process *process)
{
    struct process **prev;

    /* Do nothing if called with a NULL argument. */
    if (process == NULL)
        return;

    /* Remove the process from the global list. */
    prev = &processes;
    while (*prev != NULL && *prev != process)
        prev = &(*prev)->next;
    if (*prev == process)
        *prev = process->next;

    /* Free resources. */
    free(process->pidfile);
    free(process->logfile);
    test_tmpdir_free(process->tmpdir);
    free(process);
}


/*
 * Kill a process and wait for it to exit.  Returns the status of the process.
 * Calls bail on a system failure or a failure of the process to exit.
 *
 * We are quite aggressive with error reporting here because child processes
 * that don't exit or that don't exist often indicate some form of test
 * failure.
 */
static int
process_kill(struct process *process)
{
    int result, i;
    int status = -1;
    struct timeval tv;
    unsigned long pid = process->pid;

    /* If the process is not a child, just kill it and hope. */
    if (!process->is_child) {
        if (kill(process->pid, SIGTERM) < 0 && errno != ESRCH)
            sysbail("cannot send SIGTERM to process %lu", pid);
        return 0;
    }

    /* Check if the process has already exited. */
    result = waitpid(process->pid, &status, WNOHANG);
    if (result < 0)
        sysbail("cannot wait for child process %lu", pid);
    else if (result > 0)
        return status;

    /*
     * Kill the process and wait for it to exit.  I don't want to go to the
     * work of setting up a SIGCHLD handler or a full event loop here, so we
     * effectively poll every tenth of a second for process exit (and
     * hopefully faster when it does since the SIGCHLD may interrupt our
     * select, although we're racing with it.
     */
    if (kill(process->pid, SIGTERM) < 0 && errno != ESRCH)
        sysbail("cannot send SIGTERM to child process %lu", pid);
    for (i = 0; i < PROCESS_WAIT * 10; i++) {
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        select(0, NULL, NULL, NULL, &tv);
        result = waitpid(process->pid, &status, WNOHANG);
        if (result < 0)
            sysbail("cannot wait for child process %lu", pid);
        else if (result > 0)
            return status;
    }

    /* The process still hasn't exited.  Bail. */
    bail("child process %lu did not exit on SIGTERM", pid);

    /* Not reached, but some compilers may get confused. */
    return status;
}


/*
 * Stop a particular process given its process struct.  This kills the
 * process, waits for it to exit if possible (giving it at most five seconds),
 * and then removes it from the global processes struct so that it isn't
 * stopped again during global shutdown.
 */
void
process_stop(struct process *process)
{
    int status;
    unsigned long pid = process->pid;

    /* Stop the process. */
    status = process_kill(process);

    /* Call diag to flush logs as well as provide exit status. */
    if (process->is_child)
        diag("stopped process %lu (exit status %d)", pid, status);
    else
        diag("stopped process %lu", pid);

    /* Remove the log and PID file. */
    diag_file_remove(process->logfile);
    unlink(process->pidfile);
    unlink(process->logfile);

    /* Free resources. */
    process_free(process);
}


/*
 * Stop all running processes.  This is called as a cleanup handler during
 * process shutdown.  The first argument, which says whether the test was
 * successful, is ignored, since the same actions should be performed
 * regardless.  The second argument says whether this is the primary process,
 * in which case we do the full shutdown.  Otherwise, we only free resources
 * but don't stop the process.
 */
static void
process_stop_all(int success UNUSED, int primary)
{
    while (processes != NULL) {
        if (primary)
            process_stop(processes);
        else
            process_free(processes);
    }
}


/*
 * Read the PID of a process from a file.  This is necessary when running
 * under fakeroot to get the actual PID of the remctld process.
 */
static pid_t
read_pidfile(const char *path)
{
    FILE *file;
    char buffer[BUFSIZ];
    long pid;

    file = fopen(path, "r");
    if (file == NULL)
        sysbail("cannot open %s", path);
    if (fgets(buffer, sizeof(buffer), file) == NULL)
        sysbail("cannot read from %s", path);
    fclose(file);
    pid = strtol(buffer, NULL, 10);
    if (pid <= 0)
        bail("cannot read PID from %s", path);
    return (pid_t) pid;
}


/*
 * Start a process and return its status information.  The status information
 * is also stored in the global processes linked list so that it can be
 * stopped automatically on program exit.
 *
 * The boolean argument says whether to start the process under fakeroot.  If
 * true, PATH_FAKEROOT must be defined, generally by Autoconf.  If it's not
 * found, call skip_all.
 *
 * This is a helper function for process_start and process_start_fakeroot.
 */
static struct process *
process_start_internal(const char *const argv[], const char *pidfile,
                       bool fakeroot)
{
    size_t i;
    int log_fd;
    const char *name;
    struct timeval tv;
    struct process *process;
    const char **fakeroot_argv = NULL;
    const char *path_fakeroot = PATH_FAKEROOT;

    /* Check prerequisites. */
    if (fakeroot && path_fakeroot[0] == '\0')
        skip_all("fakeroot not found");

    /* Create the process struct and log file. */
    process = bcalloc(1, sizeof(struct process));
    process->pidfile = bstrdup(pidfile);
    process->tmpdir = test_tmpdir();
    name = strrchr(argv[0], '/');
    if (name != NULL)
        name++;
    else
        name = argv[0];
    basprintf(&process->logfile, "%s/%s.log.XXXXXX", process->tmpdir, name);
    log_fd = mkstemp(process->logfile);
    if (log_fd < 0)
        sysbail("cannot create log file for %s", argv[0]);

    /* If using fakeroot, rewrite argv accordingly. */
    if (fakeroot) {
        for (i = 0; argv[i] != NULL; i++)
            ;
        fakeroot_argv = bcalloc(2 + i + 1, sizeof(const char *));
        fakeroot_argv[0] = path_fakeroot;
        fakeroot_argv[1] = "--";
        for (i = 0; argv[i] != NULL; i++)
            fakeroot_argv[i + 2] = argv[i];
        fakeroot_argv[i + 2] = NULL;
        argv = fakeroot_argv;
    }

    /*
     * Fork off the child process, redirect its standard output and standard
     * error to the log file, and then exec the program.
     */
    process->pid = fork();
    if (process->pid < 0)
        sysbail("fork failed");
    else if (process->pid == 0) {
        if (dup2(log_fd, STDOUT_FILENO) < 0)
            sysbail("cannot redirect standard output");
        if (dup2(log_fd, STDERR_FILENO) < 0)
            sysbail("cannot redirect standard error");
        close(log_fd);
        if (execv(argv[0], (char *const *) argv) < 0)
            sysbail("exec of %s failed", argv[0]);
    }
    close(log_fd);
    free(fakeroot_argv);

    /*
     * In the parent.  Wait for the child to start by watching for the PID
     * file to appear in 100ms intervals.
     */
    for (i = 0; i < PROCESS_WAIT * 10 && access(pidfile, F_OK) != 0; i++) {
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        select(0, NULL, NULL, NULL, &tv);
    }

    /*
     * If the PID file still hasn't appeared after ten seconds, attempt to
     * kill the process and then bail.
     */
    if (access(pidfile, F_OK) != 0) {
        kill(process->pid, SIGTERM);
        alarm(5);
        waitpid(process->pid, NULL, 0);
        alarm(0);
        bail("cannot start %s", argv[0]);
    }

    /*
     * Read the PID back from the PID file.  This usually isn't necessary for
     * non-forking daemons, but always doing this makes this function general,
     * and it's required when running under fakeroot.
     */
    if (fakeroot)
        process->pid = read_pidfile(pidfile);
    process->is_child = !fakeroot;

    /* Register the log file as a source of diag messages. */
    diag_file_add(process->logfile);

    /*
     * Add the process to our global list and set our cleanup handler if this
     * is the first process we started.
     */
    if (processes == NULL)
        test_cleanup_register(process_stop_all);
    process->next = processes;
    processes = process;

    /* All done. */
    return process;
}


/*
 * Start a process and return the opaque process struct.  The process must
 * create pidfile with its PID when startup is complete.
 */
struct process *
process_start(const char *const argv[], const char *pidfile)
{
    return process_start_internal(argv, pidfile, false);
}


/*
 * Start a process under fakeroot and return the opaque process struct.  If
 * fakeroot is not available, calls skip_all.  The process must create pidfile
 * with its PID when startup is complete.
 */
struct process *
process_start_fakeroot(const char *const argv[], const char *pidfile)
{
    return process_start_internal(argv, pidfile, true);
}
