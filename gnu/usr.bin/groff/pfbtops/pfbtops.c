/* This translates ps fonts in .pfb format to ASCII ps files. */

#include <stdio.h>

/* Binary bytes per output line. */
#define BYTES_PER_LINE (79/2)
#define HEX_DIGITS "0123456789ABCDEF"

static char *program_name;

static void error(s)
     char *s;
{
  fprintf(stderr, "%s: %s\n", program_name, s);
  exit(2);
}

static void usage()
{
  fprintf(stderr, "usage: %s [-v] [pfb_file]\n", program_name);
  exit(1);
}

int main(argc, argv)
     int argc;
     char **argv;
{
  int opt;
  extern int optind;

  program_name = argv[0];

  while ((opt = getopt(argc, argv, "v")) != EOF) {
    switch (opt) {
    case 'v':
      {
	extern char *version_string;
	fprintf(stderr, "pfbtops groff version %s\n", version_string);
	fflush(stderr);
	break;
      }
    case '?':
      usage();
    }
  }

  if (argc - optind > 1)
    usage();
  if (argc > optind && !freopen(argv[optind], "r", stdin))
    {
      perror(argv[optind]);
      exit(1);
    }
  for (;;)
    {
      int type, c, i;
      long n;

      c = getchar();
      if (c != 0x80)
	error("first byte of packet not 0x80");
      type = getchar();
      if (type == 3)
	break;
      if (type != 1 && type != 2)
	error("bad packet type");
      n = 0;
      for (i = 0; i < 4; i++)
	{
	  c = getchar();
	  if (c == EOF)
	    error("end of file in packet header");
	  n |= (long)c << (i << 3);
	}
      if (n < 0)
	error("negative packet length");
      if (type == 1)
	{
	  while (--n >= 0)
	    {
	      c = getchar();
	      if (c == EOF)
		error("end of file in text packet");
	      if (c == '\r')
		c = '\n';
	      putchar(c);
	    }
	  if (c != '\n')
	    putchar('\n');
	}
      else
	{
	  int count = 0;
	  while (--n >= 0)
	    {
	      c = getchar();
	      if (c == EOF)
		error("end of file in binary packet");
	      if (count >= BYTES_PER_LINE)
		{
		  putchar('\n');
		  count = 0;
		}
	      count++;
	      putchar(HEX_DIGITS[(c >> 4) & 0xf]);
	      putchar(HEX_DIGITS[c & 0xf]);
	    }
	  putchar('\n');
	}
    }
  exit(0);
}
