/*	@(#)line.c	1.1	*/
/*
	This program reads a single line from the standard input
	and writes it on the standard output. It is probably most useful
	in conjunction with the Bourne shell.
*/
#define LSIZE 512
int EOF;
char nl = '\n';
main()
{
	register char c;
	char line[LSIZE];
	register char *linep, *linend;

EOF = 0;
linep = line;
linend = line + LSIZE;

while ((c = readc()) != nl)
	{
	if (linep == linend)
		{
		write (1, line, LSIZE);
		linep = line;
		}
	*linep++ = c;
	}
write (1, line, linep-line);
write(1,&nl,1);
if (EOF == 1) exit(1);
exit (0);
}
readc()
{
	char c;
if (read (0, &c, 1) != 1) {
	EOF = 1;
	return(nl);
	}
else
	return (c);
}
