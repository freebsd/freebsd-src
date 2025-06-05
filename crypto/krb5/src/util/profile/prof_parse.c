/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include "prof_int.h"

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <errno.h>
#include <ctype.h>
#ifndef _WIN32
#include <dirent.h>
#endif

#define SECTION_SEP_CHAR '/'

#define STATE_INIT_COMMENT      1
#define STATE_STD_LINE          2
#define STATE_GET_OBRACE        3

struct parse_state {
    int     state;
    int     group_level;
    struct profile_node *root_section;
    struct profile_node *current_section;
};

static errcode_t parse_file(FILE *f, struct parse_state *state,
                            char **ret_modspec);

static char *skip_over_blanks(char *cp)
{
    while (*cp && isspace((int) (*cp)))
        cp++;
    return cp;
}

static void strip_line(char *line)
{
    char *p = line + strlen(line);
    while (p > line && (p[-1] == '\n' || p[-1] == '\r'))
        *--p = 0;
}

static void parse_quoted_string(char *str)
{
    char *to, *from;

    for (to = from = str; *from && *from != '"'; to++, from++) {
        if (*from == '\\' && *(from + 1) != '\0') {
            from++;
            switch (*from) {
            case 'n':
                *to = '\n';
                break;
            case 't':
                *to = '\t';
                break;
            case 'b':
                *to = '\b';
                break;
            default:
                *to = *from;
            }
            continue;
        }
        *to = *from;
    }
    *to = '\0';
}


static errcode_t parse_std_line(char *line, struct parse_state *state)
{
    char    *cp, ch, *tag, *value;
    char    *p;
    errcode_t retval;
    struct profile_node     *node;
    int do_subsection = 0;
    void *iter = 0;

    if (*line == 0)
        return 0;
    cp = skip_over_blanks(line);
    if (cp[0] == ';' || cp[0] == '#')
        return 0;
    strip_line(cp);
    ch = *cp;
    if (ch == 0)
        return 0;
    if (ch == '[') {
        if (state->group_level > 0)
            return PROF_SECTION_NOTOP;
        cp++;
        p = strchr(cp, ']');
        if (p == NULL)
            return PROF_SECTION_SYNTAX;
        *p = '\0';
        retval = profile_find_node_subsection(state->root_section,
                                              cp, &iter, 0,
                                              &state->current_section);
        if (retval == PROF_NO_SECTION) {
            retval = profile_add_node(state->root_section,
                                      cp, 0,
                                      &state->current_section);
            if (retval)
                return retval;
        } else if (retval)
            return retval;

        /*
         * Finish off the rest of the line.
         */
        cp = p+1;
        if (*cp == '*') {
            profile_make_node_final(state->current_section);
            cp++;
        }
        /*
         * A space after ']' should not be fatal
         */
        cp = skip_over_blanks(cp);
        if (*cp)
            return PROF_SECTION_SYNTAX;
        return 0;
    }
    if (ch == '}') {
        if (state->group_level == 0)
            return PROF_EXTRA_CBRACE;
        if (*(cp+1) == '*')
            profile_make_node_final(state->current_section);
        retval = profile_get_node_parent(state->current_section,
                                         &state->current_section);
        if (retval)
            return retval;
        state->group_level--;
        return 0;
    }
    /*
     * Parse the relations
     */
    tag = cp;
    cp = strchr(cp, '=');
    if (!cp)
        return PROF_RELATION_SYNTAX;
    if (cp == tag)
        return PROF_RELATION_SYNTAX;
    *cp = '\0';
    p = tag;
    /* Look for whitespace on left-hand side.  */
    while (p < cp && !isspace((int)*p))
        p++;
    if (p < cp) {
        /* Found some sort of whitespace.  */
        *p++ = 0;
        /* If we have more non-whitespace, it's an error.  */
        while (p < cp) {
            if (!isspace((int)*p))
                return PROF_RELATION_SYNTAX;
            p++;
        }
    }
    cp = skip_over_blanks(cp+1);
    value = cp;
    if (value[0] == '"') {
        value++;
        parse_quoted_string(value);
    } else if (value[0] == 0) {
        do_subsection++;
        state->state = STATE_GET_OBRACE;
    } else if (value[0] == '{' && *(skip_over_blanks(value+1)) == 0)
        do_subsection++;
    else {
        cp = value + strlen(value) - 1;
        while ((cp > value) && isspace((int) (*cp)))
            *cp-- = 0;
    }
    if (do_subsection) {
        p = strchr(tag, '*');
        if (p)
            *p = '\0';
        retval = profile_add_node(state->current_section,
                                  tag, 0, &state->current_section);
        if (retval)
            return retval;
        if (p)
            profile_make_node_final(state->current_section);
        state->group_level++;
        return 0;
    }
    p = strchr(tag, '*');
    if (p)
        *p = '\0';
    profile_add_node(state->current_section, tag, value, &node);
    if (p)
        profile_make_node_final(node);
    return 0;
}

/* Open and parse an included profile file. */
static errcode_t parse_include_file(const char *filename,
                                    struct profile_node *root_section)
{
    FILE    *fp;
    errcode_t retval = 0;
    struct parse_state state;

    /* Create a new state so that fragments are syntactically independent but
     * share a root section. */
    state.state = STATE_INIT_COMMENT;
    state.group_level = 0;
    state.root_section = root_section;
    state.current_section = NULL;

    fp = fopen(filename, "r");
    if (fp == NULL)
        return PROF_FAIL_INCLUDE_FILE;
    retval = parse_file(fp, &state, NULL);
    fclose(fp);
    return retval;
}

/* Return non-zero if filename contains only alphanumeric characters, dashes,
 * and underscores, or if the filename ends in ".conf" and is not a dotfile. */
static int valid_name(const char *filename)
{
    const char *p;
    size_t len = strlen(filename);

    /* Ignore dotfiles, which might be editor or filesystem artifacts. */
    if (*filename == '.')
        return 0;

    if (len >= 5 && !strcmp(filename + len - 5, ".conf"))
        return 1;

    for (p = filename; *p != '\0'; p++) {
        if (!isalnum((unsigned char)*p) && *p != '-' && *p != '_')
            return 0;
    }
    return 1;
}

/*
 * Include files within dirname.  Only files with names ending in ".conf", or
 * consisting entirely of alphanumeric characters, dashes, and underscores are
 * included.  This restriction avoids including editor backup files, .rpmsave
 * files, and the like.  Files are processed in alphanumeric order.
 */
static errcode_t parse_include_dir(const char *dirname,
                                   struct profile_node *root_section)
{
    errcode_t retval = 0;
    char **fnames, *pathname;
    int i;

    if (k5_dir_filenames(dirname, &fnames) != 0)
        return PROF_FAIL_INCLUDE_DIR;

    for (i = 0; fnames != NULL && fnames[i] != NULL; i++) {
        if (!valid_name(fnames[i]))
            continue;
        if (asprintf(&pathname, "%s/%s", dirname, fnames[i]) < 0) {
            retval = ENOMEM;
            break;
        }
        retval = parse_include_file(pathname, root_section);
        free(pathname);
        if (retval)
            break;
    }
    k5_free_filenames(fnames);
    return retval;
}

static errcode_t parse_line(char *line, struct parse_state *state,
                            char **ret_modspec)
{
    char    *cp;

    if (strncmp(line, "include", 7) == 0 && isspace(line[7])) {
        cp = skip_over_blanks(line + 7);
        strip_line(cp);
        return parse_include_file(cp, state->root_section);
    }
    if (strncmp(line, "includedir", 10) == 0 && isspace(line[10])) {
        cp = skip_over_blanks(line + 10);
        strip_line(cp);
        return parse_include_dir(cp, state->root_section);
    }
    switch (state->state) {
    case STATE_INIT_COMMENT:
        if (strncmp(line, "module", 6) == 0 && isspace(line[6])) {
            /*
             * If we are expecting a module declaration, fill in *ret_modspec
             * and return PROF_MODULE, which will cause parsing to abort and
             * the module to be loaded instead.  If we aren't expecting a
             * module declaration, return PROF_MODULE without filling in
             * *ret_modspec, which will be treated as an ordinary error.
             */
            if (ret_modspec) {
                cp = skip_over_blanks(line + 6);
                strip_line(cp);
                *ret_modspec = strdup(cp);
                if (!*ret_modspec)
                    return ENOMEM;
            }
            return PROF_MODULE;
        }
        if (line[0] != '[')
            return 0;
        state->state = STATE_STD_LINE;
    case STATE_STD_LINE:
        return parse_std_line(line, state);
    case STATE_GET_OBRACE:
        cp = skip_over_blanks(line);
        if (*cp != '{')
            return PROF_MISSING_OBRACE;
        state->state = STATE_STD_LINE;
    }
    return 0;
}

static errcode_t parse_file(FILE *f, struct parse_state *state,
                            char **ret_modspec)
{
#define BUF_SIZE        2048
    char *bptr;
    errcode_t retval;

    bptr = malloc (BUF_SIZE);
    if (!bptr)
        return ENOMEM;

    while (!feof(f)) {
        if (fgets(bptr, BUF_SIZE, f) == NULL)
            break;
#ifndef PROFILE_SUPPORTS_FOREIGN_NEWLINES
        retval = parse_line(bptr, state, ret_modspec);
        if (retval) {
            free (bptr);
            return retval;
        }
#else
        {
            char *p, *end;

            if (strlen(bptr) >= BUF_SIZE - 1) {
                /* The string may have foreign newlines and
                   gotten chopped off on a non-newline
                   boundary.  Seek backwards to the last known
                   newline.  */
                long offset;
                char *c = bptr + strlen (bptr);
                for (offset = 0; offset > -BUF_SIZE; offset--) {
                    if (*c == '\r' || *c == '\n') {
                        *c = '\0';
                        fseek (f, offset, SEEK_CUR);
                        break;
                    }
                    c--;
                }
            }

            /* First change all newlines to \n */
            for (p = bptr; *p != '\0'; p++) {
                if (*p == '\r')
                    *p = '\n';
            }
            /* Then parse all lines */
            p = bptr;
            end = bptr + strlen (bptr);
            while (p < end) {
                char* newline;
                char* newp;

                newline = strchr (p, '\n');
                if (newline != NULL)
                    *newline = '\0';

                /* parse_line modifies contents of p */
                newp = p + strlen (p) + 1;
                retval = parse_line (p, state, ret_modspec);
                if (retval) {
                    free (bptr);
                    return retval;
                }

                p = newp;
            }
        }
#endif
    }

    free (bptr);
    return 0;
}

errcode_t profile_parse_file(FILE *f, struct profile_node **root,
                             char **ret_modspec)
{
    struct parse_state state;
    errcode_t retval;

    *root = NULL;

    /* Initialize parsing state with a new root node. */
    state.state = STATE_INIT_COMMENT;
    state.group_level = 0;
    state.current_section = NULL;
    retval = profile_create_node("(root)", 0, &state.root_section);
    if (retval)
        return retval;

    retval = parse_file(f, &state, ret_modspec);
    if (retval) {
        profile_free_node(state.root_section);
        return retval;
    }
    *root = state.root_section;
    return 0;
}

errcode_t profile_process_directory(const char *dirname,
                                    struct profile_node **root)
{
    errcode_t retval;
    struct profile_node *node;

    *root = NULL;
    retval = profile_create_node("(root)", 0, &node);
    if (retval)
        return retval;
    retval = parse_include_dir(dirname, node);
    if (retval) {
        profile_free_node(node);
        return retval;
    }
    *root = node;
    return 0;
}

/*
 * Return TRUE if the string begins or ends with whitespace
 */
static int need_double_quotes(char *str)
{
    if (!str)
        return 0;
    if (str[0] == '\0')
        return 1;
    if (isspace((int) (*str)) ||isspace((int) (*(str + strlen(str) - 1))))
        return 1;
    if (strchr(str, '\n') || strchr(str, '\t') || strchr(str, '\b'))
        return 1;
    return 0;
}

/*
 * Output a string with double quotes, doing appropriate backquoting
 * of characters as necessary.
 */
static void output_quoted_string(char *str, void (*cb)(const char *,void *),
                                 void *data)
{
    char    ch;
    char buf[2];

    cb("\"", data);
    if (!str) {
        cb("\"", data);
        return;
    }
    buf[1] = 0;
    while ((ch = *str++)) {
        switch (ch) {
        case '\\':
            cb("\\\\", data);
            break;
        case '\n':
            cb("\\n", data);
            break;
        case '\t':
            cb("\\t", data);
            break;
        case '\b':
            cb("\\b", data);
            break;
        default:
            /* This would be a lot faster if we scanned
               forward for the next "interesting"
               character.  */
            buf[0] = ch;
            cb(buf, data);
            break;
        }
    }
    cb("\"", data);
}



#if defined(_WIN32)
#define EOL "\r\n"
#endif

#ifndef EOL
#define EOL "\n"
#endif

/* Errors should be returned, not ignored!  */
static void dump_profile(struct profile_node *root, int level,
                         void (*cb)(const char *, void *), void *data)
{
    int i;
    struct profile_node *p;
    void *iter;
    long retval;
    char *name, *value;

    iter = 0;
    do {
        retval = profile_find_node_relation(root, 0, &iter,
                                            &name, &value);
        if (retval)
            break;
        for (i=0; i < level; i++)
            cb("\t", data);
        if (need_double_quotes(value)) {
            cb(name, data);
            cb(" = ", data);
            output_quoted_string(value, cb, data);
            cb(EOL, data);
        } else {
            cb(name, data);
            cb(" = ", data);
            cb(value, data);
            cb(EOL, data);
        }
    } while (iter != 0);

    iter = 0;
    do {
        retval = profile_find_node_subsection(root, 0, &iter,
                                              &name, &p);
        if (retval)
            break;
        if (level == 0) { /* [xxx] */
            cb("[", data);
            cb(name, data);
            cb("]", data);
            cb(profile_is_node_final(p) ? "*" : "", data);
            cb(EOL, data);
            dump_profile(p, level+1, cb, data);
            cb(EOL, data);
        } else {        /* xxx = { ... } */
            for (i=0; i < level; i++)
                cb("\t", data);
            cb(name, data);
            cb(" = {", data);
            cb(EOL, data);
            dump_profile(p, level+1, cb, data);
            for (i=0; i < level; i++)
                cb("\t", data);
            cb("}", data);
            cb(profile_is_node_final(p) ? "*" : "", data);
            cb(EOL, data);
        }
    } while (iter != 0);
}

static void dump_profile_to_file_cb(const char *str, void *data)
{
    fputs(str, data);
}

errcode_t profile_write_tree_file(struct profile_node *root, FILE *dstfile)
{
    dump_profile(root, 0, dump_profile_to_file_cb, dstfile);
    return 0;
}

struct prof_buf {
    char *base;
    size_t cur, max;
    int err;
};

static void add_data_to_buffer(struct prof_buf *b, const void *d, size_t len)
{
    if (b->err)
        return;
    if (b->max - b->cur < len) {
        size_t newsize;
        char *newptr;

        newsize = b->max + (b->max >> 1) + len + 1024;
        newptr = realloc(b->base, newsize);
        if (newptr == NULL) {
            b->err = 1;
            return;
        }
        b->base = newptr;
        b->max = newsize;
    }
    memcpy(b->base + b->cur, d, len);
    b->cur += len;          /* ignore overflow */
}

static void dump_profile_to_buffer_cb(const char *str, void *data)
{
    add_data_to_buffer((struct prof_buf *)data, str, strlen(str));
}

errcode_t profile_write_tree_to_buffer(struct profile_node *root,
                                       char **buf)
{
    struct prof_buf prof_buf = { 0, 0, 0, 0 };

    dump_profile(root, 0, dump_profile_to_buffer_cb, &prof_buf);
    if (prof_buf.err) {
        *buf = NULL;
        return ENOMEM;
    }
    add_data_to_buffer(&prof_buf, "", 1); /* append nul */
    if (prof_buf.max - prof_buf.cur > (prof_buf.max >> 3)) {
        char *newptr = realloc(prof_buf.base, prof_buf.cur);
        if (newptr)
            prof_buf.base = newptr;
    }
    *buf = prof_buf.base;
    return 0;
}
