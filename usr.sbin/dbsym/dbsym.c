/* Written by Pace Willisson (pace@blitz.com)
 * and placed in the public domain.
 */
#include <stdio.h>
#include <a.out.h>

char *malloc ();

#define FILE_OFFSET(vadr) (((vadr) & ~0xff000000)-N_DATADDR(hdr)+N_DATOFF(hdr))

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


usage ()
{
	fprintf (stderr, "usage: dbsym file\n");
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


	while ((c = getopt (argc, argv, "")) != EOF) {
		switch (c) {
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
		if (sp->n_type & N_STAB)
			continue;
		if (sp->n_un.n_strx == 0)
			continue;

		if (sp->n_value < 0xfe000000)
			continue;

		if (sp->n_value >= 0xff000000)
			continue;

		name = old_strtab + sp->n_un.n_strx;

		len = strlen (name);

		if (len == 0)
			continue;

		if (len >= 2 && name[len - 2] == '.' && name[len - 1] == 'o')
			continue;

		if (strcmp (name, "gcc_compiled.") == 0)
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
	
	if (db_symtab_adr == 0 || db_symtabsize_adr == 0) {
		fprintf (stderr, "couldn't find db_symtab symbols\n");
		exit (1);
	}

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

