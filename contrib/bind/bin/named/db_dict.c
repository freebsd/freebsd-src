#if !defined(lint) && !defined(SABER)
static char rcsid[] = "$Id: db_dict.c,v 8.1 1997/09/26 17:55:40 halley Exp $";
#endif /* not lint */

/*
 * Portions Copyright (c) 1997 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#include "port_before.h"

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <isc/eventlib.h>
#include <isc/logging.h>
#include <isc/memcluster.h>

#include "port_after.h"

#include "named.h"

#define	DICT_BLOCKBITS	8
#define	DICT_BLOCKSHIFT	16
#define	DICT_BLOCKMAX	(1 << DICT_BLOCKBITS)
#define	DICT_OFFSETBITS	16
#define	DICT_OFFSETSHIFT 0
#define	DICT_OFFSETMAX	(1 << DICT_OFFSETBITS)

#define	DICT_CONSUMED(Length) ((Length) + 1)
#define	DICT_INDEX(Block,Offset) (((Block) << DICT_BLOCKSHIFT) | \
				  ((Offset) << DICT_OFFSETSHIFT))

static int		dict_new(const char *, int);

static char *		blocks[DICT_BLOCKMAX];
static int		offsets[DICT_BLOCKMAX];
static int		cur_block = 0;
static int		cur_offset = -1;

int
dict_lookup(const char *text, int length, int flags) {
	int block, offset, ret;

	/* XXX this is a proof of concept, some kind of hash is needed. */
	for (block = 0; block <= cur_block; block++) {
		const char *cur = &blocks[block][0];
		const char *end = &blocks[block][offsets[block]];

		while (cur < end) {
			int xlength = *cur;

			if (xlength == length &&
			    memcmp(cur+1, text, length) == 0)
				return (DICT_INDEX(block, offset));
			cur += DICT_CONSUMED(length);
		}
	}
	if ((flags & DICT_INSERT_P) != 0)
		return (dict_new(text, length));
	return (-ENOENT);
}

static int
dict_new(const char *text, int length) {
	int ret;

	if (length < 0 || length > DICT_MAXLENGTH)
		return (-E2BIG);
	if (cur_offset + DICT_CONSUMED(length) >= DICT_OFFSETMAX) {
		if (cur_block + 1 == DICT_BLOCKMAX)
			return (-ENOSPC);
		cur_block++;
		blocks[cur_block] = memget(DICT_OFFSETMAX);
		if (blocks[cur_block] == NULL)
			return (-ENOMEM);
		cur_offset = 0;
	}
	assert(cur_offset >= 0);
	assert(cur_offset + DICT_CONSUMED(length) < DICT_OFFSETMAX);
	ret = DICT_INDEX(cur_block, cur_offset);
	blocks[cur_block][cur_offset] = length;
	memcpy(&blocks[cur_block][cur_offset+1], text, length);
	cur_offset += DICT_CONSUMED(length);
	offsets[cur_block] = cur_offset;
	return (ret);
}
