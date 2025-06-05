/*
 * Run a PAM interaction script for testing.
 *
 * Provides an interface that loads a PAM interaction script from a file and
 * runs through that script, calling the internal PAM module functions and
 * checking their results.  This allows automation of PAM testing through
 * external data files instead of coding everything in C.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2016, 2018, 2020-2021 Russ Allbery <eagle@eyrie.org>
 * Copyright 2011-2012, 2014
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
#include <portable/pam.h>
#include <portable/system.h>

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#ifdef HAVE_REGCOMP
#    include <regex.h>
#endif
#include <syslog.h>

#include <tests/fakepam/internal.h>
#include <tests/fakepam/pam.h>
#include <tests/fakepam/script.h>
#include <tests/tap/basic.h>
#include <tests/tap/macros.h>
#include <tests/tap/string.h>


/*
 * Compare a regex to a string.  If regular expression support isn't
 * available, we skip this test.
 */
#ifdef HAVE_REGCOMP
static void __attribute__((__format__(printf, 3, 4)))
like(const char *wanted, const char *seen, const char *format, ...)
{
    va_list args;
    regex_t regex;
    char err[BUFSIZ];
    int status;

    if (seen == NULL) {
        fflush(stderr);
        printf("# wanted: /%s/\n#   seen: (null)\n", wanted);
        va_start(args, format);
        okv(0, format, args);
        va_end(args);
        return;
    }
    memset(&regex, 0, sizeof(regex));
    status = regcomp(&regex, wanted, REG_EXTENDED | REG_NOSUB);
    if (status != 0) {
        regerror(status, &regex, err, sizeof(err));
        bail("invalid regex /%s/: %s", wanted, err);
    }
    status = regexec(&regex, seen, 0, NULL, 0);
    switch (status) {
    case 0:
        va_start(args, format);
        okv(1, format, args);
        va_end(args);
        break;
    case REG_NOMATCH:
        printf("# wanted: /%s/\n#   seen: %s\n", wanted, seen);
        va_start(args, format);
        okv(0, format, args);
        va_end(args);
        break;
    default:
        regerror(status, &regex, err, sizeof(err));
        bail("regexec failed for regex /%s/: %s", wanted, err);
    }
    regfree(&regex);
}
#else  /* !HAVE_REGCOMP */
static void
like(const char *wanted, const char *seen, const char *format UNUSED, ...)
{
    diag("wanted /%s/", wanted);
    diag("  seen %s", seen);
    skip("regex support not available");
}
#endif /* !HAVE_REGCOMP */


/*
 * Compare an expected string with a seen string, used by both output checking
 * and prompt checking.  This is a separate function because the expected
 * string may be a regex, determined by seeing if it starts and ends with a
 * slash (/), which may require a regex comparison.
 *
 * Eventually calls either is_string or ok to report results via TAP.
 */
static void __attribute__((__format__(printf, 3, 4)))
compare_string(char *wanted, char *seen, const char *format, ...)
{
    va_list args;
    char *comment, *regex;
    size_t length;

    /* Format the comment since we need it regardless. */
    va_start(args, format);
    bvasprintf(&comment, format, args);
    va_end(args);

    /* Check whether the wanted string is a regex. */
    length = strlen(wanted);
    if (wanted[0] == '/' && wanted[length - 1] == '/') {
        regex = bstrndup(wanted + 1, length - 2);
        like(regex, seen, "%s", comment);
        free(regex);
    } else {
        is_string(wanted, seen, "%s", comment);
    }
    free(comment);
}


/*
 * The PAM conversation function.  Takes the prompts struct from the
 * configuration and interacts appropriately.  If a prompt is of the expected
 * type but not the expected string, it still responds; if it's not of the
 * expected type, it returns PAM_CONV_ERR.
 *
 * Currently only handles a single prompt at a time.
 */
static int
converse(int num_msg, const struct pam_message **msg,
         struct pam_response **resp, void *appdata_ptr)
{
    struct prompts *prompts = appdata_ptr;
    struct prompt *prompt;
    char *message;
    size_t length;
    int i;

    *resp = bcalloc(num_msg, sizeof(struct pam_response));
    for (i = 0; i < num_msg; i++) {
        message = bstrdup(msg[i]->msg);

        /* Remove newlines for comparison purposes. */
        length = strlen(message);
        while (length > 0 && message[length - 1] == '\n')
            message[length-- - 1] = '\0';

        /* Check if we've gotten too many prompts but quietly ignore them. */
        if (prompts->current >= prompts->size) {
            diag("unexpected prompt: %s", message);
            free(message);
            ok(0, "more prompts than expected");
            continue;
        }

        /* Be sure everything matches and return the response, if any. */
        prompt = &prompts->prompts[prompts->current];
        is_int(prompt->style, msg[i]->msg_style, "style of prompt %lu",
               (unsigned long) prompts->current + 1);
        compare_string(prompt->prompt, message, "value of prompt %lu",
                       (unsigned long) prompts->current + 1);
        free(message);
        prompts->current++;
        if (prompt->style == msg[i]->msg_style && prompt->response != NULL) {
            (*resp)[i].resp = bstrdup(prompt->response);
            (*resp)[i].resp_retcode = 0;
        }
    }

    /*
     * Always return success even if the prompts don't match.  Otherwise,
     * we're likely to abort the conversation in the middle and possibly
     * leave passwords set incorrectly.
     */
    return PAM_SUCCESS;
}


/*
 * Check the actual PAM output against the expected output.  We divide the
 * expected and seen output into separate lines and compare each one so that
 * we can handle regular expressions and the output priority.
 */
static void
check_output(const struct output *wanted, const struct output *seen)
{
    size_t i;

    if (wanted == NULL && seen == NULL)
        ok(1, "no output");
    else if (wanted == NULL) {
        for (i = 0; i < seen->count; i++)
            diag("unexpected: (%d) %s", seen->lines[i].priority,
                 seen->lines[i].line);
        ok(0, "no output");
    } else if (seen == NULL) {
        for (i = 0; i < wanted->count; i++) {
            is_int(wanted->lines[i].priority, 0, "output priority %lu",
                   (unsigned long) i + 1);
            is_string(wanted->lines[i].line, NULL, "output line %lu",
                      (unsigned long) i + 1);
        }
    } else {
        for (i = 0; i < wanted->count && i < seen->count; i++) {
            is_int(wanted->lines[i].priority, seen->lines[i].priority,
                   "output priority %lu", (unsigned long) i + 1);
            compare_string(wanted->lines[i].line, seen->lines[i].line,
                           "output line %lu", (unsigned long) i + 1);
        }
        if (wanted->count > seen->count)
            for (i = seen->count; i < wanted->count; i++) {
                is_int(wanted->lines[i].priority, 0, "output priority %lu",
                       (unsigned long) i + 1);
                is_string(wanted->lines[i].line, NULL, "output line %lu",
                          (unsigned long) i + 1);
            }
        if (seen->count > wanted->count) {
            for (i = wanted->count; i < seen->count; i++)
                diag("unexpected: (%d) %s", seen->lines[i].priority,
                     seen->lines[i].line);
            ok(0, "unexpected output lines");
        } else {
            ok(1, "no excess output");
        }
    }
}


/*
 * The core of the work.  Given the path to a PAM interaction script, which
 * may be relative to C_TAP_SOURCE or C_TAP_BUILD, the user (may be NULL), and
 * the stored password (may be NULL), run that script, outputting the results
 * in TAP format.
 */
void
run_script(const char *file, const struct script_config *config)
{
    char *path;
    struct output *output;
    FILE *script;
    struct work *work;
    struct options *opts;
    struct action *action, *oaction;
    struct pam_conv conv = {NULL, NULL};
    pam_handle_t *pamh;
    int status;
    size_t i, j;
    const char *argv_empty[] = {NULL};

    /* Open and parse the script. */
    if (access(file, R_OK) == 0)
        path = bstrdup(file);
    else {
        path = test_file_path(file);
        if (path == NULL)
            bail("cannot find PAM script %s", file);
    }
    script = fopen(path, "r");
    if (script == NULL)
        sysbail("cannot open %s", path);
    work = parse_script(script, config);
    fclose(script);
    diag("Starting %s", file);
    if (work->prompts != NULL) {
        conv.conv = converse;
        conv.appdata_ptr = work->prompts;
    }

    /* Initialize PAM. */
    status = pam_start("test", config->user, &conv, &pamh);
    if (status != PAM_SUCCESS)
        sysbail("cannot create PAM handle");
    if (config->authtok != NULL)
        pamh->authtok = bstrdup(config->authtok);
    if (config->oldauthtok != NULL)
        pamh->oldauthtok = bstrdup(config->oldauthtok);

    /* Run the actions and check their return status. */
    for (action = work->actions; action != NULL; action = action->next) {
        if (work->options[action->group].argv == NULL)
            status = (*action->call)(pamh, action->flags, 0, argv_empty);
        else {
            opts = &work->options[action->group];
            status = (*action->call)(pamh, action->flags, opts->argc,
                                     (const char **) opts->argv);
        }
        is_int(action->status, status, "status for %s", action->name);
    }
    output = pam_output();
    check_output(work->output, output);
    pam_output_free(output);

    /* If we have a test callback, call it now. */
    if (config->callback != NULL)
        config->callback(pamh, config, config->data);

    /* Free memory and return. */
    pam_end(pamh, work->end_flags);
    action = work->actions;
    while (action != NULL) {
        free(action->name);
        oaction = action;
        action = action->next;
        free(oaction);
    }
    for (i = 0; i < ARRAY_SIZE(work->options); i++)
        if (work->options[i].argv != NULL) {
            for (j = 0; work->options[i].argv[j] != NULL; j++)
                free(work->options[i].argv[j]);
            free(work->options[i].argv);
        }
    if (work->output)
        pam_output_free(work->output);
    if (work->prompts != NULL) {
        for (i = 0; i < work->prompts->size; i++) {
            free(work->prompts->prompts[i].prompt);
            free(work->prompts->prompts[i].response);
        }
        free(work->prompts->prompts);
        free(work->prompts);
    }
    free(work);
    free(path);
}


/*
 * Check a filename for acceptable characters.  Returns true if the file
 * consists solely of [a-zA-Z0-9-] and false otherwise.
 */
static bool
valid_filename(const char *filename)
{
    const char *p;

    for (p = filename; *p != '\0'; p++) {
        if (*p >= 'A' && *p <= 'Z')
            continue;
        if (*p >= 'a' && *p <= 'z')
            continue;
        if (*p >= '0' && *p <= '9')
            continue;
        if (*p == '-')
            continue;
        return false;
    }
    return true;
}


/*
 * The same as run_script, but run every script found in the given directory,
 * skipping file names that contain characters other than alphanumerics and -.
 */
void
run_script_dir(const char *dir, const struct script_config *config)
{
    DIR *handle;
    struct dirent *entry;
    const char *path;
    char *file;

    if (access(dir, R_OK) == 0)
        path = dir;
    else
        path = test_file_path(dir);
    handle = opendir(path);
    if (handle == NULL)
        sysbail("cannot open directory %s", dir);
    errno = 0;
    while ((entry = readdir(handle)) != NULL) {
        if (!valid_filename(entry->d_name))
            continue;
        basprintf(&file, "%s/%s", path, entry->d_name);
        run_script(file, config);
        free(file);
        errno = 0;
    }
    if (errno != 0)
        sysbail("cannot read directory %s", dir);
    closedir(handle);
    if (path != dir)
        test_file_path_free((char *) path);
}
