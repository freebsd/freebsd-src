/*
 * $Header$
 *
 * Copyright 2008 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 * require a specific license from the United States Government.
 * It is the responsibility of any person or organization contemplating
 * export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <opts.hxx>

#if 0
const struct Opts*
GetOpts(
    )
{
    bool done = false;
    struct Opts* o;
    if (!(o = new Opts))
        goto cleanup;
    if (!(o->pszString = new char[lstrlenA(opts.pszString) + 1]))
        goto cleanup;
    if (!(o->pszEndpoint = new char[lstrlenA(opts.pszEndpoint) + 1]))
        goto cleanup;
    strcpy(o->pszString, opts.pszString);
    strcpy(o->pszEndpoint, opts.pszEndpoint);
    done = true;
 cleanup:
    if (!done) {
        FreeOpts(o);
        o = 0;
    }
    return o;
}

void
FreeOpts(
    struct Opts* o
    )
{
    if (o) {
        if (o->pszString)
            delete [] o->pszString;
        if (o->pszEndpoint)
            delete [] o->pszEndpoint;
        delete o;
    }
}
#endif

bool
ParseOpts::IsValidOpt(
    char ch
    )
{
    return (m_ValidOpts[ch % 256] != 0);
}

void
ParseOpts::PrintOpt(
    char ch,
    char* text
    )
{
    if (IsValidOpt(ch))
        fprintf(stderr, "  -%c %s\n", ch, text);
}

void
ParseOpts::UsageOpts(
    char * program,
    int code
    )
{
    fprintf(stderr, "Usage: %s [options]\n", program);
    PrintOpt('k', "stop server");
#ifdef CCAPI_TEST_OPTIONS
    PrintOpt('s', "string");
    PrintOpt('e', "endpoint");
    PrintOpt('m', "maxcalls");
    PrintOpt('n', "mincalls");
    PrintOpt('f', "flag_wait_op");
    PrintOpt('u', "unprotected");
    PrintOpt('b', "use security callback");
#endif
    PrintOpt('c', "output debug info to console");
    exit(code);
}

void
ParseOpts::SetValidOpts(
    char* valid_opts
    )
{
    memset(m_ValidOpts, 0, sizeof(m_ValidOpts));
    char *p = valid_opts;
    for (p = valid_opts; *p; p++) {
        m_ValidOpts[*p % sizeof(m_ValidOpts)] = 1;
    }
}

void
ParseOpts::Parse(
    Opts& opts,
    int argc,
    char **argv
    )
{
    int i;
    for (i = 1; i < argc; i++) {
        if ((*argv[i] == '-') || (*argv[i] == '/')) {
            char ch = tolower(*(argv[i]+1));
            if (!IsValidOpt(ch))
                UsageOpts(argv[0]);
            switch (ch) {
            case 'k':
                opts.bShutdown = TRUE;
                break;
#ifdef CCAPI_TEST_OPTIONS
            case 's':
                opts.pszString = argv[++i];
                break;
            case 'e':
                opts.pszEndpoint = argv[++i];
                break;
            case 'm':
                opts.cMaxCalls = (unsigned int) atoi(argv[++i]);
                break;
            case 'n':
                opts.cMinCalls = (unsigned int) atoi(argv[++i]);
                break;
            case 'f':
                opts.fDontWait = (unsigned int) atoi(argv[++i]);
                break;
            case 'u':
                opts.bDontProtect = TRUE;
                break;
            case 'b':
                opts.bSecCallback = TRUE;
                break;
#endif
            case 'c':
                opts.bConsole = TRUE;
                break;
            case 'h':
            case '?':
            default:
                UsageOpts(argv[0]);
            }
        }
        else
            UsageOpts(argv[0]);
    }

}

ParseOpts::ParseOpts(char* valid_opts)
{
    SetValidOpts(valid_opts);
}

ParseOpts::ParseOpts()
{
}

ParseOpts::~ParseOpts()
{
}
