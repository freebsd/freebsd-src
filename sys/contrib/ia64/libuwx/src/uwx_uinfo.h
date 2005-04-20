/*
Copyright (c) 2003 Hewlett-Packard Development Company, L.P.
Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/

struct uwx_utable_entry;

extern int uwx_decode_uinfo(
    struct uwx_env *env,
    struct uwx_utable_entry *uentry,
    uint64_t **rstatep);

extern int uwx_default_rstate(
    struct uwx_env *env,
    uint64_t **rstatep);

/* Region header record */

struct uwx_rhdr {
    int is_prologue;		/* true if prologue region */
    unsigned int rlen;		/* length of region (# instruction slots) */
    int mask;			/* register save mask */
    int grsave;			/* first gr used for saving */
    unsigned int ecount;	/* epilogue count (0 = no epilogue) */
    unsigned int epilogue_t;	/* epilogue "t" value */
};

struct uwx_bstream;

extern int uwx_decode_rhdr(
    struct uwx_env *env,
    struct uwx_bstream *bstream,
    struct uwx_rhdr *rhdr);

extern int uwx_decode_prologue(
    struct uwx_env *env,
    struct uwx_bstream *bstream,
    struct uwx_scoreboard *scoreboard,
    struct uwx_rhdr *rhdr,
    int ip_slot);

extern int uwx_decode_body(
    struct uwx_env *env,
    struct uwx_bstream *bstream,
    struct uwx_scoreboard *scoreboard,
    struct uwx_rhdr *rhdr,
    int ip_slot);
