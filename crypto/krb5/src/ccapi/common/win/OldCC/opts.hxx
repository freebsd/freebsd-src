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

#pragma once

class ParseOpts
{
public:
    struct Opts {
        char* pszString;
        char* pszEndpoint;
        unsigned int cMinCalls;
        unsigned int cMaxCalls;
        unsigned int fDontWait;
        bool bDontProtect;
        bool bShutdown;
        bool bSecCallback;
        bool bConsole;
    };

    ParseOpts(char* valid_opts);
    ParseOpts();
    ~ParseOpts();
    void SetValidOpts(char* valid_opts);
    void Parse(Opts& opts, int argc, char **argv);

private:
    bool IsValidOpt(char ch);
    void PrintOpt(char ch, char* text);
    void UsageOpts(char* program, int code = 0);

    char m_ValidOpts[256];
};
