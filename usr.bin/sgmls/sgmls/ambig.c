/* ambig.c -
   Content model ambiguity checking.

     Written by James Clark (jjc@jclark.com).
*/
/*
This uses the construction in pp8-9 of [1], extended to deal with AND
groups.

Note that it is not correct for the purposes of ambiguity analysis to
handle AND groups by turning them into an OR group of SEQ groups
(consider (a&b?)).

We build an automaton for the entire content model by adding the
following case for AND:

nullable(v) := nullable(left child) and nullable(right child)
if nullable(right child) then
    for each x in last(left child) do
       follow(v,x) = follow(left child,x) U first(right child);
if nullable(left child) then
    for each x in last(right child) do
        follow(v,x) = follow(right child,x) U first(left child);
first(v) := first(left child) U first(right child);
last(v) := first(left child) U first(right child);

We also build an automaton for each AND group by building automata for
each of the members of the AND group using the above procedure and
then combine the members using:

for each x in last(left child) do
   follow(v,x) = follow(left child,x) U first(right child);
for each x in last(right child) do
   follow(v,x) = follow(right child,x) U first(left child);
first(v) := first(left child) U first(right child);

The content model is ambiguous just in case one of these automata is
non-deterministic.  (Note that when checking determinism we need to
check the `first' set as well as all the `follow' sets.)

Why is this correct?  Consider a primitive token in a member of an AND
group.  There are two worst cases for ambiguity: firstly, when none of
the other members of AND group have been matched; secondly, when just
the nullable members remain to be matched.  The first case is not
affected by context of the AND group (unless the first case is
identical to the second case.)

Note that inclusions are not relevant for the purposes of determining
the ambiguity of content models. Otherwise the case in clause
11.2.5.1:

   An element that can satisfy an element in the content model is
   considered to do so, even if the element is also an inclusion.

could never arise.

[1] Anne Brueggemann-Klein, Regular Expressions into Finite Automata,
Universitaet Freiburg, Institut fur Informatik, 33 July 1991.
*/

#include "sgmlincl.h"

/* Sets of states are represented by 0-terminated, ordered lists of
indexes in gbuf. */

#define MAXSTATES (GRPGTCNT+2)
#define listcat(x, y) strcat((char *)(x), (char *)(y))
#define listcpy(x, y) strcpy((char *)(x), (char *)(y))

/* Information about a content token. */

struct contoken {
     UNCH size;
     UNCH nullable;
     UNCH *first;
     UNCH *last;
};

static VOID contoken P((int, int, struct contoken *));
static VOID andgroup P((int, int, struct contoken *));
static VOID orgroup P((int, int, struct contoken *));
static VOID seqgroup P((int, int, struct contoken *));
static VOID andambig P((int));
static int listambig P((UNCH *));
static VOID listmerge P((UNCH *, UNCH *));
static struct contoken *newcontoken P((void));
static VOID freecontoken P((struct contoken *));


/* Dynamically allocated vector of follow sets. */

static UNCH **follow;
static UNCH *mergebuf;		/* for use by listmerge */

/* Set to non-zero if the content model is ambiguous. */

static int ambigsw;

/* Check the current content model (in gbuf) for ambiguity. */

VOID ambig()
{
     struct contoken *s;
     int i;

     if (!follow) {
	  /* We can't allocate everything in one chunk, because that would
	     overflow a 16-bit unsigned if GRPGTCNT was 253. */
	  UNCH *ptr;
	  follow = (UNCH **)rmalloc(MAXSTATES*sizeof(UNCH *));
	  follow[0] = 0;
	  ptr = (UNCH *)rmalloc((MAXSTATES - 1)*MAXSTATES);
	  for (i = 1; i < MAXSTATES; i++) {
	       follow[i] = ptr;
	       ptr += MAXSTATES;
	  }
	  mergebuf = (UNCH *)rmalloc(MAXSTATES);
     }

     for (i = 1; i < MAXSTATES; i++)
	  follow[i][0] = 0;

     ambigsw = 0;

     s = newcontoken();
     contoken(1, 1, s);

     ambigsw = ambigsw || listambig(s->first);

     freecontoken(s);

     for (i = 1; !ambigsw && i < MAXSTATES; i++)
	  if (listambig(follow[i]))
	       ambigsw = 1;

     if (ambigsw)
	  mderr(137, (UNCH *)0, (UNCH *)0);
}

/* Free memory used for ambiguity checking. */

VOID ambigfree()
{
     if (follow) {
	  frem((UNIV)follow[1]);
	  frem((UNIV)follow);
	  frem((UNIV)mergebuf);
	  follow = 0;
     }
}

/* Determine whether a list of primitive content tokens (each
represented by its index in gbuf) is ambiguous. */

static
int listambig(list)
UNCH *list;
{
     UNCH *p;
     int chars = 0;
     int rc = 0;

     for (p = list; *p; p++) {
	  if ((gbuf[*p].ttype & TTMASK) == TTETD) {
	       struct etd *e = gbuf[*p].tu.thetd;
	       if (e->mark) {
		    rc = 1;
		    break;
	       }
	       e->mark = 1;
	  }
	  else {
	       assert((gbuf[*p].ttype & TTMASK) == TTCHARS);
	       if (chars) {
		    rc = 1;
		    break;
	       }
	       chars = 1;
	  }
     }

     for (p = list; *p; p++)
	  if ((gbuf[*p].ttype & TTMASK) == TTETD)
	       gbuf[*p].tu.thetd->mark = 0;

     return rc;
}


/* Analyze a content token.  The `checkand' argument is needed to ensure
that the algorithm is not exponential in the AND-group nesting depth.
*/

static
VOID contoken(m, checkand, res)
int m;				/* Index of content token in gbuf */
int checkand;			/* Non-zero if AND groups should be checked */
struct contoken *res;		/* Result */
{
     UNCH flags = gbuf[m].ttype;
     switch (flags & TTMASK) {
     case TTCHARS:
     case TTETD:
	  res->first[0] = m;
	  res->first[1] = 0;
	  res->last[0] = m;
	  res->last[1] = 0;
	  res->size = 1;
	  res->nullable = 0;
	  break;
     case TTAND:
	  if (checkand)
	       andambig(m);
	  andgroup(m, checkand, res);
	  break;
     case TTOR:
	  orgroup(m, checkand, res);
	  break;
     case TTSEQ:
	  seqgroup(m, checkand, res);
	  break;
     default:
	  abort();
     }
     if (flags & TREP) {
	  UNCH *p;
	  for (p = res->last; *p; p++)
	       listmerge(follow[*p], res->first);
     }
     if (flags & TOPT)
	  res->nullable = 1;
}

/* Check an AND group for ambiguity. */

static
VOID andambig(m)
int m;
{
     int i, tnum;
     int lim;
     struct contoken *curr;
     struct contoken *next;

     tnum = gbuf[m].tu.tnum;
     assert(tnum > 0);
     curr = newcontoken();
     next = newcontoken();
     contoken(m + 1, 0, curr);
     i = m + 1 + curr->size;
     curr->size += 1;
     for (--tnum; tnum > 0; --tnum) {
	  UNCH *p;
	  contoken(i, 0, next);
	  curr->size += next->size;
	  i += next->size;
	  for (p = curr->last; *p; p++)
	       listcat(follow[*p], next->first);
	  for (p = next->last; *p; p++)
	       listmerge(follow[*p], curr->first);
	  listcat(curr->first, next->first);
	  listcat(curr->last, next->last);
     }
     lim = m + curr->size;
     for (i = m + 1; i < lim; i++) {
	  if (listambig(follow[i]))
	       ambigsw = 1;
	  follow[i][0] = 0;
     }
     freecontoken(curr);
     freecontoken(next);
}

/* Handle an AND group. */

static
VOID andgroup(m, checkand, res)
int m;
int checkand;
struct contoken *res;
{
     int i, tnum;
     /* union of the first sets of nullable members of the group */
     UNCH *nullablefirst;
     struct contoken *next;

     tnum = gbuf[m].tu.tnum;
     assert(tnum > 0);
     contoken(m + 1, checkand, res);
     nullablefirst = (UNCH *)rmalloc(MAXSTATES);
     if (res->nullable)
	  listcpy(nullablefirst, res->first);
     else
	  nullablefirst[0] = 0;
     i = m + 1 + res->size;
     res->size += 1;
     next = newcontoken();
     for (--tnum; tnum > 0; --tnum) {
	  UNCH *p;
	  contoken(i, checkand, next);
	  res->size += next->size;
	  i += next->size;
	  if (next->nullable)
	       for (p = res->last; *p; p++)
		    listcat(follow[*p], next->first);
	  for (p = next->last; *p; p++)
	       listmerge(follow[*p], nullablefirst);
	  listcat(res->first, next->first);
	  if (next->nullable)
	       listcat(nullablefirst, next->first);
	  listcat(res->last, next->last);
	  res->nullable &= next->nullable;
     }
     frem((UNIV)nullablefirst);
     freecontoken(next);
}

/* Handle a SEQ group. */

static
VOID seqgroup(m, checkand, res)
int m;
int checkand;
struct contoken *res;
{
     int i, tnum;
     struct contoken *next;

     tnum = gbuf[m].tu.tnum;
     assert(tnum > 0);
     contoken(m + 1, checkand, res);
     i = m + 1 + res->size;
     res->size += 1;
     next = newcontoken();
     for (--tnum; tnum > 0; --tnum) {
	  UNCH *p;
	  contoken(i, checkand, next);
	  res->size += next->size;
	  i += next->size;
	  for (p = res->last; *p; p++)
	       listcat(follow[*p], next->first);
	  if (res->nullable)
	       listcat(res->first, next->first);
	  if (next->nullable)
	       listcat(res->last, next->last);
	  else
	       listcpy(res->last, next->last);
	  res->nullable &= next->nullable;
     }
     freecontoken(next);
}

/* Handle an OR group. */

static
VOID orgroup(m, checkand, res)
int m;
int checkand;
struct contoken *res;
{
     int i, tnum;
     struct contoken *next;

     tnum = gbuf[m].tu.tnum;
     assert(tnum > 0);
     contoken(m + 1, checkand, res);
     i = m + 1 + res->size;
     res->size += 1;
     next = newcontoken();
     for (--tnum; tnum > 0; --tnum) {
	  contoken(i, checkand, next);
	  res->size += next->size;
	  i += next->size;
	  listcat(res->first, next->first);
	  listcat(res->last, next->last);
	  res->nullable |= next->nullable;
     }
     freecontoken(next);
}


/* Merge the second ordered list into the first. */

static
VOID listmerge(p, b)
UNCH *p, *b;
{
     UNCH *a = mergebuf;

     strcpy((char *)a, (char *)p);

     for (;;) {
	  if (*a) {
	       if (*b) {
		    if (*a < *b)
			 *p++ = *a++;
		    else if (*a > *b)
			 *p++ = *b++;
		    else
			 a++;
	       }
	       else
		    *p++ = *a++;
	  }
	  else if (*b)
	       *p++ = *b++;
	  else
	       break;
     }
     *p = '\0';
}

static
struct contoken *newcontoken()
{
     struct contoken *p = (struct contoken *)rmalloc(sizeof(struct contoken)
						     + MAXSTATES*2);
     p->first = (UNCH *)(p + 1);
     p->last = p->first + MAXSTATES;
     return p;
}

static
VOID freecontoken(p)
struct contoken *p;
{
     frem((UNIV)p);
}

/*
Local Variables:
c-indent-level: 5
c-continued-statement-offset: 5
c-brace-offset: -5
c-argdecl-indent: 0
c-label-offset: -5
End:
*/
