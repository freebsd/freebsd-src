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

#include "uwx.h"

#define WORDSZ			4
#define DWORDSZ			8
#define BUNDLESZ		16
#define SLOTSPERBUNDLE		3

#define UNWIND_TBL_32BIT	0x8000000000000000LL

#define UNW_VER(x)		((x) >> 48)
#define UNW_FLAG_MASK		0x0000ffff00000000LL
#define UNW_FLAG_EHANDLER	0x0000000100000000LL
#define UNW_FLAG_UHANDLER	0x0000000200000000LL
#define UNW_LENGTH(x)		((x) & 0x00000000ffffffffLL)

struct uwx_scoreboard;

#define NSCOREBOARDS	8	/* Initial allocation of scoreboards */

#define NSPECIALREG	16	/* Must be even, so FRs are aligned */
#define NPRESERVEDGR	4
#define NPRESERVEDBR	5
#define NPRESERVEDFR	20

struct uwx_fpreg {
    uint64_t part0;
    uint64_t part1;
};

struct uwx_context {
    unsigned int valid_regs;
    unsigned int valid_frs;
    uint64_t special[NSPECIALREG];
    uint64_t gr[NPRESERVEDGR];
    uint64_t br[NPRESERVEDBR];
    struct uwx_fpreg fr[NPRESERVEDFR];
};

#define VALID_GR_SHIFT	NSPECIALREG
#define VALID_BR_SHIFT	(NSPECIALREG + NPRESERVEDGR)

#define VALID_BASIC4	0x0f	/* IP, SP, BSP, CFM */
#define VALID_MARKERS	0x70	/* RP, PSP, PFS */

struct uwx_history {
    uint64_t special[NSPECIALREG];
    uint64_t gr[NPRESERVEDGR];
    uint64_t br[NPRESERVEDBR];
    uint64_t fr[NPRESERVEDFR];
};

struct uwx_str_pool;

struct uwx_env {
    struct uwx_context context;
    uint64_t *rstate;
    int64_t function_offset;
    struct uwx_history history;
    alloc_cb allocate_cb;
    free_cb free_cb;
    struct uwx_scoreboard *free_scoreboards;
    struct uwx_scoreboard *used_scoreboards;
    struct uwx_scoreboard *labeled_scoreboards;
    struct uwx_str_pool *string_pool;
    char *module_name;
    char *function_name;
    intptr_t cb_token;
    copyin_cb copyin;
    lookupip_cb lookupip;
    int remote;
    int byte_swap;
    int abi_context;
    int nsbreg;
    int nscoreboards;
    int trace;
};

extern alloc_cb uwx_allocate_cb;
extern free_cb uwx_free_cb;
