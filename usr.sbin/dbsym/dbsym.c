/* Written by Pace Willisson (pace@blitz.com)
 * and placed in the public domain.
 */

#ifndef lint
static char rcsid[] = "$Id: dbsym.c,v 1.3 1994/01/19 03:52:25 nate Exp $";
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <a.out.h>
#include <stab.h>
#include <machine/param.h>
#include <vm/vm_param.h>
#include <vm/lock.h>
#include <vm/vm_statistics.h>
#include <machine/pmap.h>
#include <machine/vmparam.h>

#define FILE_OFFSET(vadr) (((vadr) - text_adr) - N_DATADDR(hdr) + \
			   N_DATOFF(hdr) + N_TXTADDR(hdr))

u_int text_adr = KERNBASE;

struct nlist *old_syms;
int num_old_syms;
char *old_strtab;
int old_strtab_size;

struct nlist *new_syms;
int num_new_syms;
int new_syms_bytes;
char *new_strtab;
int new_strtab_size;

int db_symtabsize_adr;
int db_symtab_adr;

int avail;

int force = 0;
int zap_locals = 0;
int debugging = 0;

usage ()
{
	fprintf (stderr, "usage: dbsym [-fgx] [-T addr] file\n");
	exit (1);
}

struct exec hdr;

main (argc, argv)
char **argv;
{
	FILE *f;
	char *name;
	extern int optind;
	int c, i;
	int need;
	char *buf, *p;
	struct nlist *nsp, *sp;
	int len;


	while ((c = getopt (argc, argv, "fgxT:")) != EOF) {
		switch (c) {
		case 'f':
			force = 1;
			break;
		case 'g':
			debugging = 1;
			break;
                case 'x':
                        zap_locals = 1;
                        break;
		case 'T':
			text_adr = strtoul(optarg, &p, 16);
			if (*p)
				err("illegal text address: %s", optarg);
			break;
		default:
			usage ();
		}
	}

	if (optind >= argc)
		usage ();

	name = argv[optind++];

	if (optind != argc)
		usage ();

	if ((f = fopen (name, "r+")) == NULL) {
		fprintf (stderr, "can't open %s\n", name);
		exit (1);
	}

	if (fread ((char *)&hdr, sizeof hdr, 1, f) != 1) {
		fprintf (stderr, "can't read header\n");
		exit (1);
	}

	if (N_BADMAG (hdr)) {
		fprintf (stderr, "bad magic number\n");
		exit (1);
	}

	if (hdr.a_syms == 0) {
		fprintf (stderr, "no symbols\n");
		exit (1);
	}

	fseek (f, N_STROFF (hdr), 0);
	if (fread ((char *)&old_strtab_size, sizeof (int), 1, f) != 1) {
		fprintf (stderr, "can't read old strtab size\n");
		exit (1);
	}

	if ((old_syms = (struct nlist *)malloc (hdr.a_syms)) == NULL
	    || ((old_strtab = malloc (old_strtab_size)) == NULL)
	    || ((new_syms = (struct nlist *)malloc (hdr.a_syms)) == NULL)
	    || ((new_strtab = malloc (old_strtab_size)) == NULL)) {
		fprintf (stderr, "out of memory\n");
		exit (1);
	}

	fseek (f, N_SYMOFF (hdr), 0);
	if (fread ((char *)old_syms, hdr.a_syms, 1, f) != 1) {
		fprintf (stderr, "can't read symbols\n");
		exit (1);
	}

	fseek (f, N_STROFF (hdr), 0);
	if (fread ((char *)old_strtab, old_strtab_size, 1, f) != 1) {
		fprintf (stderr, "can't read string table\n");
		exit (1);
	}

	num_old_syms = hdr.a_syms / sizeof (struct nlist);

	new_strtab_size = 4;

	nsp = new_syms;
	for (i = 0, sp = old_syms; i < num_old_syms; i++, sp++) {
		if (zap_locals && !(sp->n_type & N_EXT))
			continue;

		if (sp->n_type & N_STAB)
			switch (sp->n_type & ~N_EXT) {
				case N_SLINE:
					if (debugging)
						*nsp++ = *sp;
					continue;
				case N_FUN:
				case N_PSYM:
				case N_SO:
					if (!debugging)
						continue;
					goto skip_tests;
					break;
				default:
					continue;
			}

		if ((sp->n_type & ~N_EXT) == N_UNDF)
			continue;
                
                if (!debugging && (sp->n_type & ~N_EXT) == N_FN)
			continue;

		if (sp->n_un.n_strx == 0)
			continue;

		if (sp->n_value < text_adr)
			continue;

		if (sp->n_value > (text_adr + hdr.a_text + hdr.a_data +
				   hdr.a_bss))
			continue;

		skip_tests:

		name = old_strtab + sp->n_un.n_strx;

		len = strlen (name);

		if (len == 0)
			continue;

		if (strcmp (name, "gcc_compiled.") == 0)
			continue;

		if (strcmp (name, "gcc2_compiled.") == 0)
			continue;

                if (strcmp (name, "___gnu_compiled_c") == 0)
			continue;
                        
		*nsp = *sp;

		nsp->n_un.n_strx = new_strtab_size;
		strcpy (new_strtab + new_strtab_size, name);
		new_strtab_size += len + 1;
		nsp++;

		if (strcmp (name, "_db_symtab") == 0)
			db_symtab_adr = sp->n_value;
		if (strcmp (name, "_db_symtabsize") == 0)
			db_symtabsize_adr = sp->n_value;
	}
	
	if (db_symtab_adr == 0 || db_symtabsize_adr == 0)
		if (!force) {
			fprintf (stderr, "couldn't find db_symtab symbols\n");
			exit (1);
		} else
			exit (0);

	*(int *)new_strtab = new_strtab_size;
	num_new_syms = nsp - new_syms;
	new_syms_bytes = num_new_syms * sizeof (struct nlist);

	need = sizeof (int)
		+ num_new_syms * sizeof (struct nlist)
			+ new_strtab_size;

	fseek (f, FILE_OFFSET (db_symtabsize_adr), 0);

	if (fread ((char *)&avail, sizeof (int), 1, f) != 1) {
		fprintf (stderr, "can't read symtabsize\n");
		exit (1);
	}

	printf ("dbsym: need %d; avail %d\n", need, avail);

	if (need > avail) {
		fprintf (stderr, "not enough room in db_symtab array\n");
		exit (1);
	}

	fseek (f, FILE_OFFSET (db_symtab_adr), 0);
	fwrite ((char *)&new_syms_bytes, sizeof (int), 1, f);
	fwrite ((char *)new_syms, new_syms_bytes, 1, f);
	fwrite (new_strtab, new_strtab_size, 1, f);
	fflush (f);

	if (feof (f) || ferror (f)) {
		fprintf (stderr, "write error\n");
		exit (1);
	}
	exit (0);
}

