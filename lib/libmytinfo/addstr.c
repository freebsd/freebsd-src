/*
 * addstr.c
 *
 * By Ross Ridge
 * Public Domain
 * 92/02/01 07:34:01
 *
 */

#include "defs.h"
#include <term.h>

#include <ctype.h>

#ifdef USE_SCCS_IDS
static char const SCCSid[] = "@(#) mytinfo addstr.c 3.2 92/02/01 public domain, By Ross Ridge";
#endif
/*
 * I think this routine could be improved, as it is now it searches a
 * linked list of strbufs for one that has enough room left for the
 * string. The only thing else I can think of doing would be to 
 * expand a buffer by realloc and then fix the string pointers if it
 * moves.
 */

static struct strbuf *strbuf = NULL;
   
struct strbuf *
_endstr() {
	register struct strbuf *p;

	p = strbuf;
	strbuf = NULL;
	return p;
}

char *
_addstr(s)
register char *s; {
	register struct strbuf *p;
	register int l;

	if (s == NULL) {
		strbuf = NULL;
		return NULL;
	}

	if (strbuf == NULL) {
		strbuf = (struct strbuf *) malloc(sizeof(struct strbuf));
		if (strbuf == NULL)
			return NULL;
		strbuf->len = 0;
		strbuf->next = NULL;
	}
	l = strlen(s) + 1;
	if (l > MAX_CHUNK)
		return NULL;
	p = strbuf;
	while (l + p->len > MAX_CHUNK) {
		if (p->next == NULL) {
			p->next = (struct strbuf *)
					malloc(sizeof(struct strbuf));
			p = p->next;
			if (p == NULL)
				return NULL;
			p->len = 0;
			p->next = NULL;
			break;
		}
		p = p->next;
	}
	s = strcpy(p->buf + p->len, s);
	p->len += l;
	return s;
}

void
_del_strs(p)
TERMINAL *p; {
	struct strbuf *q;

	q = p->strbuf;
	while(q != NULL) {
		p->strbuf = q->next; 
		free((anyptr) q);
		q = p->strbuf;
	}
}
