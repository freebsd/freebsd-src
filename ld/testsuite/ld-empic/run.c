/* Load and run a MIPS position independent ECOFF file.
   Written by Ian Lance Taylor <ian@cygnus.com>
   Public domain.  */

/* This program will load an ECOFF file into memory and execute it.
   The file must have been compiled using the GNU -membedded-pic
   switch to produce position independent code.  This will only work
   if this program is run on a MIPS system with the same endianness as
   the ECOFF file.  The ECOFF file must be complete.  System calls may
   not work correctly.

   There are further restrictions on the file (they could be removed
   by doing some additional programming).  The file must be aligned
   such that it does not require any gaps introduced in the data
   segment; the GNU linker produces such files by default.  However,
   the file must not assume that the text or data segment is aligned
   on a page boundary.  The start address must be at the start of the
   text segment.

   The ECOFF file is run by calling it as though it were a function.
   The address of the data segment is passed as the only argument.
   The file is expected to return an integer value, which will be
   printed.  */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

/* Structures used in ECOFF files.  We assume that a short is two
   bytes and an int is four bytes.  This is not much of an assumption,
   since we already assume that we are running on a MIPS host with the
   same endianness as the file we are examining.  */

struct ecoff_filehdr {
  unsigned short f_magic;	/* magic number                 */
  unsigned short f_nscns;	/* number of sections           */
  unsigned int f_timdat;	/* time & date stamp            */
  unsigned int f_symptr;	/* file pointer to symtab       */
  unsigned int f_nsyms;		/* number of symtab entries     */
  unsigned short f_opthdr;	/* sizeof(optional hdr)         */
  unsigned short f_flags;	/* flags                        */
};

struct ecoff_aouthdr
{
  unsigned short magic;		/* type of file				*/
  unsigned short vstamp;	/* version stamp			*/
  unsigned int tsize;		/* text size in bytes, padded to FW bdry*/
  unsigned int dsize;		/* initialized data "  "		*/
  unsigned int bsize;		/* uninitialized data "   "		*/
  unsigned int entry;		/* entry pt.				*/
  unsigned int text_start;	/* base of text used for this file */
  unsigned int data_start;	/* base of data used for this file */
  unsigned int bss_start;	/* base of bss used for this file */
  unsigned int gprmask;		/* ?? */
  unsigned int cprmask[4];	/* ?? */
  unsigned int gp_value;	/* value for gp register */
};

#define ECOFF_SCNHDR_SIZE (40)

static void
die (s)
     char *s;
{
  perror (s);
  exit (1);
}

int
main (argc, argv)
     int argc;
     char **argv;
{
  FILE *f;
  struct stat s;
  char *z;
  struct ecoff_filehdr *fh;
  struct ecoff_aouthdr *ah;
  unsigned int toff;
  char *t, *d;
  int (*pfn) ();
  int ret;

  if (argc != 2)
    {
      fprintf (stderr, "Usage: %s file\n", argv[0]);
      exit (1);
    }

  f = fopen (argv[1], "r");
  if (f == NULL)
    die (argv[1]);

  if (stat (argv[1], &s) < 0)
    die ("stat");

  z = (char *) malloc (s.st_size);
  if (z == NULL)
    die ("malloc");

  if (fread (z, 1, s.st_size, f) != s.st_size)
    die ("fread");

  /* We need to figure out the start of the text segment, which is the
     location we are going to call, and the start of the data segment,
     which we are going to pass as an argument.  We also need the size
     and start address of the bss segment.  This information is all in
     the ECOFF a.out header.  */

  fh = (struct ecoff_filehdr *) z;
  if (fh->f_opthdr != sizeof (struct ecoff_aouthdr))
    {
      fprintf (stderr, "%s: unexpected opthdr size: is %u, want %u\n",
	       argv[1], (unsigned int) fh->f_opthdr,
	       (unsigned int) sizeof (struct ecoff_aouthdr));
      exit (1);
    }

  ah = (struct ecoff_aouthdr *) (z + sizeof (struct ecoff_filehdr));
  if (ah->magic != 0413)
    {
      fprintf (stderr, "%s: bad aouthdr magic number 0%o (want 0413)\n",
	       argv[1], (unsigned int) ah->magic);
      exit (1);
    }

  /* We should clear the bss segment at this point.  This is the
     ah->bsize bytes starting at ah->bss_start, To do this correctly,
     we would have to make sure our memory block is large enough.  It
     so happens that our test case does not have any additional pages
     for the bss segment--it is contained within the data segment.
     So, we don't bother.  */
  if (ah->bsize != 0)
    {
      fprintf (stderr,
	       "%s: bss segment is %u bytes; non-zero sizes not supported\n",
	       argv[1], ah->bsize);
      exit (1);
    }

  /* The text section starts just after all the headers, rounded to a
     16 byte boundary.  */
  toff = (sizeof (struct ecoff_filehdr) + sizeof (struct ecoff_aouthdr)
	  + fh->f_nscns * ECOFF_SCNHDR_SIZE);
  toff += 15;
  toff &=~ 15;
  t = z + toff;

  /* The tsize field gives us the start of the data segment.  */
  d = z + ah->tsize;

  /* Call the code as a function.  */
  pfn = (int (*) ()) t;
  ret = (*pfn) (d);

  printf ("%s ran and returned %d\n", argv[1], ret);

  exit (0);
}
