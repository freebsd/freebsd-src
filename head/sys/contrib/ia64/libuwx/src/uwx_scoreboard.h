/*
Copyright (c) 2003-2006 Hewlett-Packard Development Company, L.P.
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

#define NSB_SPECIAL	9
#define NSB_GR		4
#define NSB_BR		5
#define NSB_FR		20

#define SBREG_RP	0
#define SBREG_PSP	1
#define SBREG_PFS	2
#define SBREG_PREDS	3
#define SBREG_UNAT	4
#define SBREG_PRIUNAT	5
#define SBREG_RNAT	6
#define SBREG_LC	7
#define SBREG_FPSR	8
#define SBREG_GR	(0 + NSB_SPECIAL)
#define SBREG_BR	(SBREG_GR + NSB_GR)
#define SBREG_FR	(SBREG_BR + NSB_BR)

#define NSBREG_NOFR	(NSB_SPECIAL + NSB_GR + NSB_BR)
#define NSBREG		(NSB_SPECIAL + NSB_GR + NSB_BR + NSB_FR)

struct uwx_scoreboard {
    struct uwx_scoreboard *nextused;
    struct uwx_scoreboard *nextfree;
    struct uwx_scoreboard *nextstack;
    struct uwx_scoreboard *nextlabel;
    uint64_t rstate[NSBREG];
    int label;
    int id;
    int prealloc;
};

extern void uwx_prealloc_scoreboard(
    struct uwx_env *env,
    struct uwx_scoreboard *sb);

extern struct uwx_scoreboard *uwx_alloc_scoreboard(struct uwx_env *env);

extern struct uwx_scoreboard *uwx_init_scoreboards(struct uwx_env *env);

extern struct uwx_scoreboard *uwx_new_scoreboard(
    struct uwx_env *env,
    struct uwx_scoreboard *prevsb);

extern struct uwx_scoreboard *uwx_pop_scoreboards(
    struct uwx_env *env,
    struct uwx_scoreboard *sb,
    int ecount);

extern int uwx_label_scoreboard(
    struct uwx_env *env,
    struct uwx_scoreboard *sb,
    int label);

extern int uwx_copy_scoreboard(
    struct uwx_env *env,
    struct uwx_scoreboard *sb,
    int label);

extern void uwx_free_scoreboards(struct uwx_env *env);
