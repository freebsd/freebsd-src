/****************************************************************
Copyright 1990, 1991 by AT&T Bell Laboratories and Bellcore.

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

#include "defs.h"
#include "iob.h"

#define MEMBSIZE	32000
#define GMEMBSIZE	16000

 extern void exit();

 char *
gmem(n, round)
 int n, round;
{
	static char *last, *next;
	char *rv;
	if (round)
#ifdef CRAY
		if ((long)next & 0xe000000000000000)
			next = (char *)(((long)next & 0x1fffffffffffffff) + 1);
#else
#ifdef MSDOS
		if ((int)next & 1)
			next++;
#else
		next = (char *)(((long)next + sizeof(char *)-1)
				& ~((long)sizeof(char *)-1));
#endif
#endif
	rv = next;
	if ((next += n) > last) {
		rv = Alloc(n + GMEMBSIZE);

		next = rv + n;
		last = next + GMEMBSIZE;
		}
	return rv;
	}

 struct memblock {
	struct memblock *next;
	char buf[MEMBSIZE];
	};
 typedef struct memblock memblock;

 static memblock *mem0;
 memblock *curmemblock, *firstmemblock;

 char *mem_first, *mem_next, *mem_last, *mem0_last;

 void
mem_init()
{
	curmemblock = firstmemblock = mem0
		= (memblock *)Alloc(sizeof(memblock));
	mem_first = mem0->buf;
	mem_next  = mem0->buf;
	mem_last  = mem0->buf + MEMBSIZE;
	mem0_last = mem0->buf + MEMBSIZE;
	mem0->next = 0;
	}

 char *
mem(n, round)
 int n, round;
{
	memblock *b;
	register char *rv, *s;

	if (round)
#ifdef CRAY
		if ((long)mem_next & 0xe000000000000000)
			mem_next = (char *)(((long)mem_next & 0x1fffffffffffffff) + 1);
#else
#ifdef MSDOS
		if ((int)mem_next & 1)
			mem_next++;
#else
		mem_next = (char *)(((long)mem_next + sizeof(char *)-1)
				& ~((long)sizeof(char *)-1));
#endif
#endif
	rv = mem_next;
	s = rv + n;
	if (s >= mem_last) {
		if (n > MEMBSIZE)  {
			fprintf(stderr, "mem(%d) failure!\n", n);
			exit(1);
			}
		if (!(b = curmemblock->next)) {
			b = (memblock *)Alloc(sizeof(memblock));
			curmemblock->next = b;
			b->next = 0;
			}
		curmemblock = b;
		rv = b->buf;
		mem_last = rv + sizeof(b->buf);
		s = rv + n;
		}
	mem_next = s;
	return rv;
	}

 char *
tostring(s,n)
 register char *s;
 int n;
{
	register char *s1, *se, **sf;
	char *rv, *s0;
	register int k = n + 2, t;

	sf = str_fmt;
	sf['%'] = "%";
	s0 = s;
	se = s + n;
	for(; s < se; s++) {
		t = *(unsigned char *)s;
		s1 = sf[t];
		while(*++s1)
			k++;
		}
	sf['%'] = "%%";
	rv = s1 = mem(k,0);
	*s1++ = '"';
	for(s = s0; s < se; s++) {
		t = *(unsigned char *)s;
		sprintf(s1, sf[t], t);
		s1 += strlen(s1);
		}
	*s1 = 0;
	return rv;
	}

 char *
cpstring(s)
 register char *s;
{
	return strcpy(mem(strlen(s)+1,0), s);
	}

 void
new_iob_data(ios, name)
 register io_setup *ios;
 char *name;
{
	register iob_data *iod;
	register char **s, **se;

	iod = (iob_data *)
		mem(sizeof(iob_data) + ios->nelt*sizeof(char *), 1);
	iod->next = iob_list;
	iob_list = iod;
	iod->type = ios->fields[0];
	iod->name = cpstring(name);
	s = iod->fields;
	se = s + ios->nelt;
	while(s < se)
		*s++ = "0";
	*s = 0;
	}

 char *
string_num(pfx, n)
 char *pfx;
 long n;
{
	char buf[32];
	sprintf(buf, "%s%ld", pfx, n);
	/* can't trust return type of sprintf -- BSD gets it wrong */
	return strcpy(mem(strlen(buf)+1,0), buf);
	}

static defines *define_list;

 void
def_start(outfile, s1, s2, post)
 FILE *outfile;
 char *s1, *s2, *post;
{
	defines *d;
	int n, n1;
	extern int in_define;

	n = n1 = strlen(s1);
	if (s2)
		n += strlen(s2);
	d = (defines *)mem(sizeof(defines)+n, 1);
	d->next = define_list;
	define_list = d;
	strcpy(d->defname, s1);
	if (s2)
		strcpy(d->defname + n1, s2);
	in_define = 1;
	nice_printf(outfile, "#define %s", d->defname);
	if (post)
		nice_printf(outfile, " %s", post);
	}

 void
other_undefs(outfile)
 FILE *outfile;
{
	defines *d;
	if (d = define_list) {
		define_list = 0;
		nice_printf(outfile, "\n");
		do
			nice_printf(outfile, "#undef %s\n", d->defname);
			while(d = d->next);
		nice_printf(outfile, "\n");
		}
	}
