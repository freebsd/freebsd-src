/****************************************************************
Copyright 1990 by AT&T Bell Laboratories and Bellcore.

Permission to use, copy, modify, and distribute this software
and its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the names of AT&T Bell Laboratories or
Bellcore or any of their entities not be used in advertising or
publicity pertaining to distribution of the software without
specific, written prior permission.

AT&T and Bellcore disclaim all warranties with regard to this
software, including all implied warranties of merchantability
and fitness.  In no event shall AT&T or Bellcore be liable for
any special, indirect or consequential damages or any damages
whatsoever resulting from loss of use, data or profits, whether
in an action of contract, negligence or other tortious action,
arising out of or in connection with the use or performance of
this software.
****************************************************************/

#ifndef CRAY
#define STACKMIN 512
#define MINBLK (2*sizeof(struct mem) + 16)
#define MSTUFF _malloc_stuff_
#define F MSTUFF.free
#define B MSTUFF.busy
#define SBGULP 8192
char *memcpy();

struct mem {
	struct mem *next;
	unsigned len;
	};

struct {
	struct mem *free;
	char *busy;
	} MSTUFF;

char *
malloc(size)
register unsigned size;
{
	register struct mem *p, *q, *r, *s;
	unsigned register k, m;
	extern char *sbrk();
	char *top, *top1;

	size = (size+7) & ~7;
	r = (struct mem *) &F;
	for (p = F, q = 0; p; r = p, p = p->next) {
		if ((k = p->len) >= size && (!q || m > k)) { m = k; q = p; s = r; }
		}
	if (q) {
		if (q->len - size >= MINBLK) { /* split block */
			p = (struct mem *) (((char *) (q+1)) + size);
			p->next = q->next;
			p->len = q->len - size - sizeof(struct mem);
			s->next = p;
			q->len = size;
			}
		else s->next = q->next;
		}
	else {
		top = B ? B : (char *)(((long)sbrk(0) + 7) & ~7);
		if (F && (char *)(F+1) + F->len == B)
			{ q = F; F = F->next; }
		else q = (struct mem *) top;
		top1 = (char *)(q+1) + size;
		if (top1 > top) {
			if (sbrk((int)(top1-top+SBGULP)) == (char *) -1)
				return 0;
			r = (struct mem *)top1;
			r->len = SBGULP - sizeof(struct mem);
			r->next = F;
			F = r;
			top1 += SBGULP;
			}
		q->len = size;
		B = top1;
		}
	return (char *) (q+1);
	}

free(f)
char *f;
{
	struct mem *p, *q, *r;
	char *pn, *qn;

	if (!f) return;
	q = (struct mem *) (f - sizeof(struct mem));
	qn = f + q->len;
	for (p = F, r = (struct mem *) &F; ; r = p, p = p->next) {
		if (qn == (char *) p) {
			q->len += p->len + sizeof(struct mem);
			p = p->next;
			}
		pn = p ? ((char *) (p+1)) + p->len : 0;
		if (pn == (char *) q) {
			p->len += sizeof(struct mem) + q->len;
			q->len = 0;
			q->next = p;
			r->next = p;
			break;
			}
		if (pn < (char *) q) {
			r->next = q;
			q->next = p;
			break;
			}
		}
	}

char *
realloc(f, size)
char *f;
unsigned size;
{
	struct mem *p;
	char *q, *f1;
	unsigned s1;

	if (!f) return malloc(size);
	p = (struct mem *) (f - sizeof(struct mem));
	s1 = p->len;
	free(f);
	if (s1 > size) s1 = size + 7 & ~7;
	if (!p->len) {
		f1 = (char *)(p->next + 1);
		memcpy(f1, f, s1);
		f = f1;
		}
	q = malloc(size);
	if (q && q != f)
		memcpy(q, f, s1);
	return q;
	}
#endif
