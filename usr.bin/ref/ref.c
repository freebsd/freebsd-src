/* ref2.c */

/* This is a totally rewritten version of ref.  This version looks for the
 * desired function name in the "tags" file, and then reads the header out
 * from the source file.  There is no longer any need for a "refs" file.
 *
 * Usage:	ref [-t] [-f file] [-c class] tag
 * Options:	-t	   output tag info, not the description
 *		-f file	   default filename for static functions
 *		-c class   default class names for class functions
 */
#ifdef __STDC__
# include <string.h>
# include <stdlib.h>
#endif

#include <stdio.h>
#include "config.h"

extern char	*cktagdir P_((char *, char *));
extern int	getline P_((char *, int, FILE *));
extern int	lookup P_((char *, char *));
extern int	find P_((char *));
extern void	usage P_((void));
extern int	countcolons P_((char *));
extern void	main P_((int, char **));


/* This is the default path that is searched for tags */
#if OSK
# define DEFTAGPATH ".:/dd/defs:/dd/defs/sys:/dd/usr/src/lib:../lib:/dd/usr/lib"
#else
# if ANY_UNIX
#  define DEFTAGPATH ".:/usr/include:/usr/include/sys:/usr/src/lib:../lib:/usr/local/lib"
# else
#  if MSDOS || TOS
#   define DEFTAGPATH ".;C:\\include;C:\\include\\sys;C:\\lib;..\\lib"
#   define SEP ';'
#  else
#   if AMIGA
#    define DEFTAGPATH ".;Include:;Include:sys"
#    define SEP ';'
#   else /* any other OS */
#    define DEFTAGPATH "."
#   endif
#  endif
# endif
#endif

#ifndef SEP
# define SEP ':'
#endif


/* These variables reflect the command-line options given by the user. */
int	taginfo;	/* boolean: give only the tag info? (not header?) */
char	*def_file;	/* default filename for static functions */
char	*def_class;	/* default classname for class members */
int	colons;		/* #colons in tag: 0=normal, 1=static, 2=member */

/* This function checks for a tag in the "tags" file of given directory.
 * If the tag is found, then it returns a pointer to a static buffer which
 * contains the filename, a tab character, and a linespec for finding the
 * the tag.  If the tag is not found in the "tags" file, or if the "tags"
 * file cannot be opened or doesn't exist, then this function returns NULL.
 */
char *cktagdir(tag, dir)
	char	*tag;	/* name of the tag to look for */
	char	*dir;	/* name of the directory to check */
{
	char	buf[BLKSIZE];
	static char found[BLKSIZE];
	FILE	*tfile;
	int	len;

#if AMIGA
	if (dir[strlen(dir) - 1] == COLON)
	    sprintf(buf, "%s%s", dir, TAGS);   /* no slash after colon. */
	else
#endif
	/* construct the name of the "tags" file in this directory */
	sprintf(buf, "%s%c%s", dir, SLASH, TAGS);

	/* Try to open the tags file.  Return NULL if can't open */
#if AMIGA
	if (buf[0] == '.' && buf[1] == SLASH)
	    tfile = fopen(&buf[2], "r");
	else
#endif
	tfile = fopen(buf, "r");
	if (!tfile)
	{
		return (char *)0;
	}

	/* compute the length of the tagname once */
	len = strlen(tag);

	/* read lines until we get the one for this tag */
	found[0] = '\0';
	while (fgets(buf, sizeof buf, tfile))
	{
		/* is this the one we want? */
		if (!strncmp(buf, tag, len) && buf[len] == '\t')
		{
			/* we've found a match -- remember it */
			strcpy(found, buf);

			/* if there is no default file, or this match is in
			 * the default file, then we've definitely found the
			 * one we want.  Break out of the loop now.
			 */
			if (!def_file || !strncmp(&buf[len + 1], def_file, strlen(def_file)))
			{
				break;
			}
		}
	}

	/* we're through reading */
	fclose(tfile);

	/* if there's anything in found[], use it */
	if (found[0])
	{
		return &found[len + 1];
	}

	/* else we didn't find it */
	return (char *)0;
}

/* This function reads a single textline from a binary file.  It returns
 * the number of bytes read, or 0 at EOF.
 */
int getline(buf, limit, fp)
	char	*buf;	/* buffer to read into */
	int	limit;	/* maximum characters to read */
	FILE	*fp;	/* binary stream to read from */
{
	int	bytes;	/* number of bytes read so far */
	int	ch;	/* single character from file */

	for (bytes = 0, ch = 0; ch != '\n' && --limit > 0 && (ch = getc(fp)) != EOF; bytes++)
	{
#if MSDOS || TOS
		/* since this is a binary file, we'll need to manually strip CR's */
		if (ch == '\r')
		{
			continue;
		}
#endif
		*buf++ = ch;
	}
	*buf = '\0';

	return bytes;
}


/* This function reads a source file, looking for a given tag.  If it finds
 * the tag, then it displays it and returns TRUE.  Otherwise it returns FALSE.
 * To display the tag, it attempts to output any introductory comment, the
 * tag line itself, and any arguments.  Arguments are assumed to immediately
 * follow the tag line, and start with whitespace.  Comments are assumed to
 * start with lines that begin with "/*", "//", "(*", or "--", and end at the
 * tag line or at a blank line.
 */
int lookup(dir, entry)
	char	*dir;	/* name of the directory that contains the source */
	char	*entry;	/* source filename, <Tab>, linespec */
{
	char	*name;		/* basename of source file */
	char	buf[BLKSIZE];	/* pathname of source file */
	long	lnum;		/* desired line number */
	long	thislnum;	/* current line number */
	long	here;		/* seek position where current line began */
	long	comment;	/* seek position of introductory comment, or -1L */
	FILE	*sfile;		/* used for reading the source file */
	int	len;		/* length of string */
	int	noargs = 0;	/* boolean: don't show lines after tag line? */
	char	*ptr;


	/* construct the pathname of the source file */
	name = entry;
	strcpy(buf, dir);
	ptr = buf + strlen(buf);
#if AMIGA
	if (ptr[-1] != COLON)
#endif
	*ptr++ = SLASH;
	while (*entry != '\t')
	{
		*ptr++ = *entry++;
	}
	*entry++ = *ptr = '\0';

	/* searching for string or number? */
	if (*entry >= '0' && *entry <= '9')
	{
		/* given a specific line number */
		lnum = atol(entry);
		entry = (char *)0;
		noargs = 1;
	}
	else
	{
		/* given a string -- strip off "/^" and "$/\n" */
		entry += 2;
		len = strlen(entry) - 2;
		if (entry[len - 1] == '$')
		{
			entry[len - 1] = '\n';
		}
		if (!strchr(entry, '('))
		{
			noargs = 1;
		}
		lnum = 0L;
	}

	/* Open the file.  Note that we open the file in binary mode even
	 * though we know it is a text file, because ftell() and fseek()
	 * don't work on text files.
	 */
#if MSDOS || TOS
	sfile = fopen(buf, "rb");
#else
# if AMIGA
	if (buf[0] == '.' && buf[1] == SLASH)
	    sfile = fopen(&buf[2], "r");
	else
# endif
	sfile = fopen(buf, "r");
#endif
	if (!sfile)
	{
		/* can't open the real source file.  Try "refs" instead */
#if AMIGA
		if (dir[strlen(dir) - 1] == COLON)
			sprintf(buf, "%srefs", dir);
		else
#endif
		sprintf(buf, "%s%crefs", dir, SLASH);
#if MSDOS || TOS
		sfile = fopen(buf, "rb");
#else
# if AMIGA
		if (buf[0] == '.' && buf[1] == SLASH)
		    sfile = fopen(&buf[2], "r");
		else
# endif
		sfile = fopen(buf, "r");
#endif
		if (!sfile)
		{
			/* failed! */
			return 0;
		}
		name = "refs";
	}

	/* search the file */
	for (comment = -1L, thislnum = 0; here = ftell(sfile), thislnum++, getline(buf, BLKSIZE, sfile) > 0; )
	{
		/* Is this the start/end of a comment? */
		if (comment == -1L)
		{
			/* starting a comment? */
			if (buf[0] == '/' && buf[1] == '*'
			 || buf[0] == '/' && buf[1] == '/'
			 || buf[0] == '(' && buf[1] == '*'
			 || buf[0] == '-' && buf[1] == '-')
			{
				comment = here;
			}
		}
		else
		{
			/* ending a comment? */
			if (buf[0] == '\n' || buf[0] == '#')
			{
				comment = -1L;
			}
		}

		/* is this the tag line? */
		if (lnum == thislnum || (entry && !strncmp(buf, entry, len)))
		{
			/* display the filename & line number where found */
			if (strcmp(dir, "."))
				printf("%s%c%s, line %ld:\n", dir, SLASH, name, thislnum);
			else
				printf("%s, line %ld:\n", name, thislnum);

			/* if there were introductory comments, show them */
			if (comment != -1L)
			{
				fseek(sfile, comment, 0);
				while (comment != here)
				{
					getline(buf, BLKSIZE, sfile);
					fputs(buf, stdout);
					comment = ftell(sfile);
				}

				/* re-fetch the tag line */
				fgets(buf, BLKSIZE, sfile);
			}

			/* show the tag line */
			fputs(buf, stdout);

			/* are we expected to show argument lines? */
			if (!noargs)
			{
				/* show any argument lines */
				while (getline(buf, BLKSIZE, sfile) > 0
				    && buf[0] != '#'
				    && strchr(buf, '{') == (char *)0)
				{
					fputs(buf, stdout);
				}
			}

			/* Done!  Close the file, and return TRUE */
			fclose(sfile);
			return 1;
		}
	}

	/* not found -- return FALSE */
	return 0;
}

/* This function searches through the entire search path for a given tag.
 * If it finds the tag, then it displays the info and returns TRUE;
 * otherwise it returns FALSE.
 */
int find(tag)
	char	*tag;	/* the tag to look up */
{
	char	*tagpath;
	char	dir[80];
	char	*ptr;
	int	len;

	if (colons == 1)
	{
		/* looking for static function -- only look in current dir */
		tagpath = ".";
	}
	else
	{
		/* get the tagpath from the environment.  Default to DEFTAGPATH */
		tagpath = getenv("TAGPATH");
		if (!tagpath)
		{
			tagpath = DEFTAGPATH;
		}
	}

	/* for each entry in the path... */
	while (*tagpath)
	{
		/* Copy the entry into the dir[] buffer */
		for (ptr = dir; *tagpath && *tagpath != SEP; tagpath++)
		{
			*ptr++ = *tagpath;
		}
		if (*tagpath == SEP)
		{
			tagpath++;
		}

		/* if the entry ended with "/tags", then strip that off */
		len = strlen(TAGS);
		if (&dir[len] < ptr && ptr[-len - 1] == SLASH && !strncmp(&ptr[-len], TAGS, len))
		{
			ptr -= len + 1;
		}

		/* if the entry is now an empty string, then assume "." */
		if (ptr == dir)
		{
			*ptr++ = '.';
		}
		*ptr = '\0';

		/* look for the tag in this path.  If found, then display it
		 * and exit.
		 */
		ptr = cktagdir(tag, dir);
		if (ptr)
		{
			/* just supposed to display tag info? */
			if (taginfo)
			{
				/* then do only that! */
				if (strcmp(dir, "."))
				{
					printf("%s%c%s", dir, SLASH, ptr);
				}
				else
				{
					/* avoid leading "./" if possible */
					fputs(ptr, stdout);
				}
				return 1;
			}
			else
			{
				/* else look up the declaration of the thing */
				return lookup(dir, ptr);
			}
		}
	}

	/* if we get here, then the tag wasn't found anywhere */
	return 0;
}

void usage()
{
	fputs("usage: ref [-t] [-c class] [-f file] tag\n", stderr);
	fputs("   -t        output tag info, instead of the function header\n", stderr);
	fputs("   -f File   tag might be a static function in File\n", stderr);
	fputs("   -c Class  tag might be a member of class Class\n", stderr);
	exit(2);
}


int countcolons(str)
	char	*str;
{
	while (*str != ':' && *str)
	{
		str++;
	}
	if (str[0] != ':')
	{
		return 0;
	}
	else if (str[1] != ':')
	{
		return 1;
	}
	return 2;
}

void main(argc, argv)
	int	argc;
	char	**argv;
{
	char	def_tag[100];	/* used to build tag name with default file/class */
	int	i;

	/* parse flags */
	for (i = 1; i < argc && argv[i][0] == '-'; i++)
	{
		switch (argv[i][1])
		{
		  case 't':
			taginfo = 1;
			break;

		  case 'f':
			if (argv[i][2])
			{
				def_file = &argv[i][2];
			}
			else if (++i < argc)
			{
				def_file = argv[i];
			}
			else
			{
				usage();
			}
			break;

		  case 'c':
			if (argv[i][2])
			{
				def_class = &argv[i][2];
			}
			else if (++i < argc)
			{
				def_class = argv[i];
			}
			else
			{
				usage();
			}
			break;

		  default:
			usage();
		}
	}

	/* if no tag was given, complain */
	if (i + 1 != argc)
	{
		usage();
	}

	/* does the tag have an explicit class or file? */
	colons = countcolons(argv[i]);

	/* if not, then maybe try some defaults */
	if (colons == 0)
	{
		/* try a static function in the file first */
		if (def_file)
		{
			sprintf(def_tag, "%s:%s", def_file, argv[i]);
			colons = 1;
			if (find(def_tag))
			{
				exit(0);
			}
		}

		/* try a member function for a class */
		if (def_class)
		{
			sprintf(def_tag, "%s::%s", def_class, argv[i]);
			colons = 2;
			if (find(def_tag))
			{
				exit(0);
			}
		}

		/* oh, well */
		colons = 0;
	}

	/* find the tag */
	if (find(argv[i]))
	{
		exit(0);
	}

	/* Give up.  If doing tag lookup then exit(0), else exit(1) */
	exit(!taginfo);
	/*NOTREACHED*/
}
