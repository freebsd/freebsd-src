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
 * Copyright 2017-2018, 2020 Russ Allbery <eagle@eyrie.org>
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

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <syslog.h>

#include <tests/fakepam/internal.h>
#include <tests/fakepam/script.h>
#include <tests/tap/basic.h>
#include <tests/tap/string.h>

/* Used for enumerating arrays. */
#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))

/* Mapping of strings to PAM function pointers and group numbers. */
static const struct {
    const char *name;
    pam_call call;
    enum group_type group;
} CALLS[] = {
    /* clang-format off */
    {"acct_mgmt",     pam_sm_acct_mgmt,     GROUP_ACCOUNT },
    {"authenticate",  pam_sm_authenticate,  GROUP_AUTH    },
    {"setcred",       pam_sm_setcred,       GROUP_AUTH    },
    {"chauthtok",     pam_sm_chauthtok,     GROUP_PASSWORD},
    {"open_session",  pam_sm_open_session,  GROUP_SESSION },
    {"close_session", pam_sm_close_session, GROUP_SESSION },
    /* clang-format on */
};

/* Mapping of PAM flag names without the leading PAM_ to values. */
static const struct {
    const char *name;
    int value;
} FLAGS[] = {
    /* clang-format off */
    {"CHANGE_EXPIRED_AUTHTOK", PAM_CHANGE_EXPIRED_AUTHTOK},
    {"DELETE_CRED",            PAM_DELETE_CRED           },
    {"DISALLOW_NULL_AUTHTOK",  PAM_DISALLOW_NULL_AUTHTOK },
    {"ESTABLISH_CRED",         PAM_ESTABLISH_CRED        },
    {"PRELIM_CHECK",           PAM_PRELIM_CHECK          },
    {"REFRESH_CRED",           PAM_REFRESH_CRED          },
    {"REINITIALIZE_CRED",      PAM_REINITIALIZE_CRED     },
    {"SILENT",                 PAM_SILENT                },
    {"UPDATE_AUTHTOK",         PAM_UPDATE_AUTHTOK        },
    /* clang-format on */
};

/* Mapping of strings to PAM groups. */
static const struct {
    const char *name;
    enum group_type group;
} GROUPS[] = {
    /* clang-format off */
    {"account",  GROUP_ACCOUNT },
    {"auth",     GROUP_AUTH    },
    {"password", GROUP_PASSWORD},
    {"session",  GROUP_SESSION },
    /* clang-format on */
};

/* Mapping of strings to PAM return values. */
static const struct {
    const char *name;
    int status;
} RETURNS[] = {
    /* clang-format off */
    {"PAM_AUTH_ERR",         PAM_AUTH_ERR        },
    {"PAM_AUTHINFO_UNAVAIL", PAM_AUTHINFO_UNAVAIL},
    {"PAM_AUTHTOK_ERR",      PAM_AUTHTOK_ERR     },
    {"PAM_DATA_SILENT",      PAM_DATA_SILENT     },
    {"PAM_IGNORE",           PAM_IGNORE          },
    {"PAM_NEW_AUTHTOK_REQD", PAM_NEW_AUTHTOK_REQD},
    {"PAM_SESSION_ERR",      PAM_SESSION_ERR     },
    {"PAM_SUCCESS",          PAM_SUCCESS         },
    {"PAM_USER_UNKNOWN",     PAM_USER_UNKNOWN    },
    /* clang-format on */
};

/* Mapping of PAM prompt styles to their values. */
static const struct {
    const char *name;
    int style;
} STYLES[] = {
    /* clang-format off */
    {"echo_off",  PAM_PROMPT_ECHO_OFF},
    {"echo_on",   PAM_PROMPT_ECHO_ON },
    {"error_msg", PAM_ERROR_MSG      },
    {"info",      PAM_TEXT_INFO      },
    /* clang-format on */
};

/* Mappings of strings to syslog priorities. */
static const struct {
    const char *name;
    int priority;
} PRIORITIES[] = {
    /* clang-format off */
    {"DEBUG",  LOG_DEBUG },
    {"INFO",   LOG_INFO  },
    {"NOTICE", LOG_NOTICE},
    {"ERR",    LOG_ERR   },
    {"CRIT",   LOG_CRIT  },
    /* clang-format on */
};


/*
 * Given a pointer to a string, skip any leading whitespace and return a
 * pointer to the first non-whitespace character.
 */
static char *
skip_whitespace(char *p)
{
    while (isspace((unsigned char) (*p)))
        p++;
    return p;
}


/*
 * Read a line from a file into a BUFSIZ buffer, failing if the line was too
 * long to fit into the buffer, and returns a copy of that line in newly
 * allocated memory.  Ignores blank lines and comments.  Caller is responsible
 * for freeing.  Returns NULL on end of file and fails on read errors.
 */
static char *
readline(FILE *file)
{
    char buffer[BUFSIZ];
    char *line, *first;

    do {
        line = fgets(buffer, sizeof(buffer), file);
        if (line == NULL) {
            if (feof(file))
                return NULL;
            sysbail("cannot read line from script");
        }
        if (buffer[strlen(buffer) - 1] != '\n')
            bail("script line too long");
        buffer[strlen(buffer) - 1] = '\0';
        first = skip_whitespace(buffer);
    } while (first[0] == '#' || first[0] == '\0');
    line = bstrdup(buffer);
    return line;
}


/*
 * Given the name of a PAM call, map it to a call enum.  This is used later in
 * switch statements to determine which function to call.  Fails on any
 * unrecognized string.  If the optional second argument is not NULL, also
 * store the group number in that argument.
 */
static pam_call
string_to_call(const char *name, enum group_type *group)
{
    size_t i;

    for (i = 0; i < ARRAY_SIZE(CALLS); i++)
        if (strcmp(name, CALLS[i].name) == 0) {
            if (group != NULL)
                *group = CALLS[i].group;
            return CALLS[i].call;
        }
    bail("unrecognized PAM call %s", name);
}


/*
 * Given a PAM flag value without the leading PAM_, map it to the numeric
 * value of that flag.  Fails on any unrecognized string.
 */
static int
string_to_flag(const char *name)
{
    size_t i;

    for (i = 0; i < ARRAY_SIZE(FLAGS); i++)
        if (strcmp(name, FLAGS[i].name) == 0)
            return FLAGS[i].value;
    bail("unrecognized PAM flag %s", name);
}


/*
 * Given a PAM group name, map it to the array index for the options array for
 * that group.  Fails on any unrecognized string.
 */
static enum group_type
string_to_group(const char *name)
{
    size_t i;

    for (i = 0; i < ARRAY_SIZE(GROUPS); i++)
        if (strcmp(name, GROUPS[i].name) == 0)
            return GROUPS[i].group;
    bail("unrecognized PAM group %s", name);
}


/*
 * Given a syslog priority name, map it to the numeric value of that priority.
 * Fails on any unrecognized string.
 */
static int
string_to_priority(const char *name)
{
    size_t i;

    for (i = 0; i < ARRAY_SIZE(PRIORITIES); i++)
        if (strcmp(name, PRIORITIES[i].name) == 0)
            return PRIORITIES[i].priority;
    bail("unrecognized syslog priority %s", name);
}


/*
 * Given a PAM return status, map it to the actual expected value.  Fails on
 * any unrecognized string.
 */
static int
string_to_status(const char *name)
{
    size_t i;

    if (name == NULL)
        bail("no PAM status on line");
    for (i = 0; i < ARRAY_SIZE(RETURNS); i++)
        if (strcmp(name, RETURNS[i].name) == 0)
            return RETURNS[i].status;
    bail("unrecognized PAM status %s", name);
}


/*
 * Given a PAM prompt style value without the leading PAM_PROMPT_, map it to
 * the numeric value of that flag.  Fails on any unrecognized string.
 */
static int
string_to_style(const char *name)
{
    size_t i;

    for (i = 0; i < ARRAY_SIZE(STYLES); i++)
        if (strcmp(name, STYLES[i].name) == 0)
            return STYLES[i].style;
    bail("unrecognized PAM prompt style %s", name);
}


/*
 * We found a section delimiter while parsing another section.  Rewind our
 * input file back before the section delimiter so that we'll read it again.
 * Takes the length of the line we read, which is used to determine how far to
 * rewind.
 */
static void
rewind_section(FILE *script, size_t length)
{
    if (fseek(script, -length - 1, SEEK_CUR) != 0)
        sysbail("cannot rewind file");
}


/*
 * Given a string that may contain %-escapes, expand it into the resulting
 * value.  The following escapes are supported:
 *
 *     %i   current UID (not target user UID)
 *     %n   new password
 *     %p   password
 *     %u   username
 *     %0   user-supplied string
 *     ...
 *     %9   user-supplied string
 *
 * The %* escape is preserved as-is, as it has to be interpreted at the time
 * of checking output.  Returns the expanded string in newly-allocated memory.
 */
static char *
expand_string(const char *template, const struct script_config *config)
{
    size_t length = 0;
    const char *p, *extra;
    char *output, *out;
    char *uid = NULL;

    length = 0;
    for (p = template; *p != '\0'; p++) {
        if (*p != '%')
            length++;
        else {
            p++;
            switch (*p) {
            case 'i':
                if (uid == NULL)
                    basprintf(&uid, "%lu", (unsigned long) getuid());
                length += strlen(uid);
                break;
            case 'n':
                if (config->newpass == NULL)
                    bail("new password not set");
                length += strlen(config->newpass);
                break;
            case 'p':
                if (config->password == NULL)
                    bail("password not set");
                length += strlen(config->password);
                break;
            case 'u':
                length += strlen(config->user);
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                if (config->extra[*p - '0'] == NULL)
                    bail("extra script parameter %%%c not set", *p);
                length += strlen(config->extra[*p - '0']);
                break;
            case '*':
                length += 2;
                break;
            default:
                length++;
                break;
            }
        }
    }
    output = bmalloc(length + 1);
    for (p = template, out = output; *p != '\0'; p++) {
        if (*p != '%')
            *out++ = *p;
        else {
            p++;
            switch (*p) {
            case 'i':
                assert(uid != NULL);
                memcpy(out, uid, strlen(uid));
                out += strlen(uid);
                break;
            case 'n':
                memcpy(out, config->newpass, strlen(config->newpass));
                out += strlen(config->newpass);
                break;
            case 'p':
                memcpy(out, config->password, strlen(config->password));
                out += strlen(config->password);
                break;
            case 'u':
                memcpy(out, config->user, strlen(config->user));
                out += strlen(config->user);
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                extra = config->extra[*p - '0'];
                memcpy(out, extra, strlen(extra));
                out += strlen(extra);
                break;
            case '*':
                *out++ = '%';
                *out++ = '*';
                break;
            default:
                *out++ = *p;
                break;
            }
        }
    }
    *out = '\0';
    free(uid);
    return output;
}


/*
 * Given a whitespace-delimited string of PAM options, split it into an argv
 * array and argc count and store it in the provided option struct.
 */
static void
split_options(char *string, struct options *options,
              const struct script_config *config)
{
    char *opt;
    size_t size, count;

    for (opt = strtok(string, " "); opt != NULL; opt = strtok(NULL, " ")) {
        if (options->argv == NULL) {
            options->argv = bcalloc(2, sizeof(const char *));
            options->argv[0] = expand_string(opt, config);
            options->argc = 1;
        } else {
            count = (options->argc + 2);
            size = sizeof(const char *);
            options->argv = breallocarray(options->argv, count, size);
            options->argv[options->argc] = expand_string(opt, config);
            options->argv[options->argc + 1] = NULL;
            options->argc++;
        }
    }
}


/*
 * Parse the options section of a PAM script.  This consists of one or more
 * lines in the format:
 *
 *     <group> = <options>
 *
 * where options are either option names or option=value pairs, where the
 * value may not contain whitespace.  Returns an options struct, which stores
 * argc and argv values for each group.
 *
 * Takes the work struct as an argument and puts values into its array.
 */
static void
parse_options(FILE *script, struct work *work,
              const struct script_config *config)
{
    char *line, *group, *token;
    size_t length = 0;
    enum group_type type;

    for (line = readline(script); line != NULL; line = readline(script)) {
        length = strlen(line);
        group = strtok(line, " ");
        if (group == NULL)
            bail("malformed script line");
        if (group[0] == '[')
            break;
        type = string_to_group(group);
        token = strtok(NULL, " ");
        if (token == NULL)
            bail("malformed action line");
        if (strcmp(token, "=") != 0)
            bail("malformed action line near %s", token);
        token = strtok(NULL, "");
        split_options(token, &work->options[type], config);
        free(line);
    }
    if (line != NULL) {
        free(line);
        rewind_section(script, length);
    }
}


/*
 * Parse the call portion of a PAM call in the run section of a PAM script.
 * This handles parsing the PAM flags that optionally may be given as part of
 * the call.  Takes the token representing the call and a pointer to the
 * action struct to fill in with the call and the option flags.
 */
static void
parse_call(char *token, struct action *action)
{
    char *flags, *flag;

    action->flags = 0;
    flags = strchr(token, '(');
    if (flags != NULL) {
        *flags = '\0';
        flags++;
        for (flag = strtok(flags, "|,)"); flag != NULL;
             flag = strtok(NULL, "|,)")) {
            action->flags |= string_to_flag(flag);
        }
    }
    action->call = string_to_call(token, &action->group);
}


/*
 * Parse the run section of a PAM script.  This consists of one or more lines
 * in the format:
 *
 *     <call> = <status>
 *
 * where <call> is a PAM call and <status> is what it should return.  Returns
 * a linked list of actions.  Fails on any error in parsing.
 */
static struct action *
parse_run(FILE *script)
{
    struct action *head = NULL, *current = NULL, *next;
    char *line, *token, *call;
    size_t length = 0;

    for (line = readline(script); line != NULL; line = readline(script)) {
        length = strlen(line);
        token = strtok(line, " ");
        if (token[0] == '[')
            break;
        next = bmalloc(sizeof(struct action));
        next->next = NULL;
        if (head == NULL)
            head = next;
        else
            current->next = next;
        next->name = bstrdup(token);
        call = token;
        token = strtok(NULL, " ");
        if (token == NULL)
            bail("malformed action line");
        if (strcmp(token, "=") != 0)
            bail("malformed action line near %s", token);
        token = strtok(NULL, " ");
        next->status = string_to_status(token);
        parse_call(call, next);
        free(line);
        current = next;
    }
    if (head == NULL)
        bail("empty run section in script");
    if (line != NULL) {
        free(line);
        rewind_section(script, length);
    }
    return head;
}


/*
 * Parse the end section of a PAM script.  There is one supported line in the
 * format:
 *
 *     flags = <flag>|<flag>
 *
 * where <flag> is a flag to pass to pam_end.  Returns the flags.
 */
static int
parse_end(FILE *script)
{
    char *line, *token, *flag;
    size_t length = 0;
    int flags = PAM_SUCCESS;

    for (line = readline(script); line != NULL; line = readline(script)) {
        length = strlen(line);
        token = strtok(line, " ");
        if (token[0] == '[')
            break;
        if (strcmp(token, "flags") != 0)
            bail("unknown end setting %s", token);
        token = strtok(NULL, " ");
        if (token == NULL)
            bail("malformed end line");
        if (strcmp(token, "=") != 0)
            bail("malformed end line near %s", token);
        token = strtok(NULL, " ");
        flag = strtok(token, "|");
        while (flag != NULL) {
            flags |= string_to_status(flag);
            flag = strtok(NULL, "|");
        }
        free(line);
    }
    if (line != NULL) {
        free(line);
        rewind_section(script, length);
    }
    return flags;
}


/*
 * Parse the output section of a PAM script.  This consists of zero or more
 * lines in the format:
 *
 *     PRIORITY some output information
 *     PRIORITY /output regex/
 *
 * where PRIORITY is replaced by the numeric syslog priority corresponding to
 * that priority and the rest of the output undergoes %-esacape expansion.
 * Returns the accumulated output as a vector.
 */
static struct output *
parse_output(FILE *script, const struct script_config *config)
{
    char *line, *token, *message;
    struct output *output;
    int priority;

    output = output_new();
    if (output == NULL)
        sysbail("cannot allocate vector");
    for (line = readline(script); line != NULL; line = readline(script)) {
        token = strtok(line, " ");
        priority = string_to_priority(token);
        token = strtok(NULL, "");
        if (token == NULL)
            bail("malformed line %s", line);
        message = expand_string(token, config);
        output_add(output, priority, message);
        free(message);
        free(line);
    }
    return output;
}


/*
 * Parse the prompts section of a PAM script.  This consists of zero or more
 * lines in one of the formats:
 *
 *     type = prompt
 *     type = /prompt/
 *     type = prompt|response
 *     type = /prompt/|response
 *
 * If the type is error_msg or info, there is no response.  Otherwise,
 * everything after the last | is taken to be the response that should be
 * provided to that prompt.  The response undergoes %-escape expansion.
 */
static struct prompts *
parse_prompts(FILE *script, const struct script_config *config)
{
    struct prompts *prompts = NULL;
    struct prompt *prompt;
    char *line, *token, *style, *end;
    size_t size, count, i;
    size_t length = 0;

    for (line = readline(script); line != NULL; line = readline(script)) {
        length = strlen(line);
        token = strtok(line, " ");
        if (token[0] == '[')
            break;
        if (prompts == NULL) {
            prompts = bcalloc(1, sizeof(struct prompts));
            prompts->prompts = bcalloc(1, sizeof(struct prompt));
            prompts->allocated = 1;
        } else if (prompts->allocated == prompts->size) {
            count = prompts->allocated * 2;
            size = sizeof(struct prompt);
            prompts->prompts = breallocarray(prompts->prompts, count, size);
            prompts->allocated = count;
            for (i = prompts->size; i < prompts->allocated; i++) {
                prompts->prompts[i].prompt = NULL;
                prompts->prompts[i].response = NULL;
            }
        }
        prompt = &prompts->prompts[prompts->size];
        style = token;
        token = strtok(NULL, " ");
        if (token == NULL)
            bail("malformed prompt line");
        if (strcmp(token, "=") != 0)
            bail("malformed prompt line near %s", token);
        prompt->style = string_to_style(style);
        token = strtok(NULL, "");
        if (prompt->style == PAM_ERROR_MSG || prompt->style == PAM_TEXT_INFO)
            prompt->prompt = expand_string(token, config);
        else {
            end = strrchr(token, '|');
            if (end == NULL)
                bail("malformed prompt line near %s", token);
            *end = '\0';
            prompt->prompt = expand_string(token, config);
            token = end + 1;
            prompt->response = expand_string(token, config);
        }
        prompts->size++;
        free(line);
    }
    if (line != NULL) {
        free(line);
        rewind_section(script, length);
    }
    return prompts;
}


/*
 * Parse a PAM interaction script.  This handles parsing of the top-level
 * section markers and dispatches the parsing to other functions.  Returns the
 * total work to do as a work struct.
 */
struct work *
parse_script(FILE *script, const struct script_config *config)
{
    struct work *work;
    char *line, *token;

    work = bmalloc(sizeof(struct work));
    memset(work, 0, sizeof(struct work));
    work->end_flags = PAM_SUCCESS;
    for (line = readline(script); line != NULL; line = readline(script)) {
        token = strtok(line, " ");
        if (token[0] != '[')
            bail("line outside of section: %s", line);
        if (strcmp(token, "[options]") == 0)
            parse_options(script, work, config);
        else if (strcmp(token, "[run]") == 0)
            work->actions = parse_run(script);
        else if (strcmp(token, "[end]") == 0)
            work->end_flags = parse_end(script);
        else if (strcmp(token, "[output]") == 0)
            work->output = parse_output(script, config);
        else if (strcmp(token, "[prompts]") == 0)
            work->prompts = parse_prompts(script, config);
        else
            bail("unknown section: %s", token);
        free(line);
    }
    if (work->actions == NULL)
        bail("no run section defined");
    return work;
}
