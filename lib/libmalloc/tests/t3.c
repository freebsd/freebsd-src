/*
Path: utstat!helios.physics.utoronto.ca!jarvis.csri.toronto.edu!mailrus!tut.cis.ohio-state.edu!cs.utexas.edu!rice!sun-spots-request
From: munsell!jwf@uunet.uu.net (Jim Franklin)
Newsgroups: comp.sys.sun
Subject: bug in SUN malloc()
Keywords: Miscellaneous
Message-ID: <4193@brazos.Rice.edu>
Date: 4 Jan 90 13:05:06 GMT
Sender: root@rice.edu
Organization: Sun-Spots
Lines: 95
Approved: Sun-Spots@rice.edu
X-Sun-Spots-Digest: Volume 9, Issue 4, message 10 of 12

There is a bug in SUN's malloc() that causes it to sometimes attempt an
sbrk() to grow the current process, even if there is a free block of the
exact size available.  The bug exists in OS 3.5 and 4.0.3, probably
others.

SUN's malloc() maintains the free list in a cartesian tree.  The malloc()
bug occurs when the root block is exactly equal in size to the requested
allocation + alignment + overhead.

malloc.c (416-427):
>	/-*
>	 * ensure that at least one block is big enough to satisfy
>	 *	the request.
>	 *-/
>
>	if (weight(_root) <= nbytes) {
>		/-*
>		 * the largest block is not enough.
>		 *-/
>		if(!morecore(nbytes))
>			return 0;
>	}

The '<=' should be '<'.

The following 'malloc_bug' program illustrates the bug.  Do a 'pstat -s'
to see how much swap space you have, then run malloc_bug, requesting a
chunk of memory at least 1/2 that size.  E.g.,

jwf@fechner #36 pstat -s
8856k used (2120k text), 648872k free, 2776k wasted, 0k missing
max process allocable = 229360k
avail: 77*8192k 2*4096k 2*2048k 3*1024k 2*512k 3*256k 4*128k 3*64k 2*32k 4*16k 104*1k

jwf@fechner #37 malloc_bug 200000000
malloc_bug: requesting 200000000 bytes
malloc_bug: got 200000000 bytes at 00022fd8
malloc_bug: freeing 200000000 bytes
malloc_bug: requesting 200000000 bytes
malloc_bug: Not enough memory


Jim Franklin, EPPS Inc.,        uunet!atexnet ---\
32 Wiggins Ave.,                harvard!adelie ---+-- munsell!jwf
Bedford, MA 01730               decvax!encore ---/
(617) 276-7827


*/
#include <stdio.h>

main (argc, argv)
int     argc;
char    **argv;
{
	char            *p;
	unsigned int    size;
	int             i;
	extern char     *malloc ();

	if ( argc != 2 ) {
		fprintf (stderr, "usage: malloc_bug <chunk_size>\n");
		exit (-1);
	}
	size = atoi (argv[1]);
					/* malloc our large chunk */

	fprintf (stderr, "malloc_bug: requesting %d bytes\n", size);
	p = malloc (size);
	if ( p == NULL ) {
		perror ("malloc_bug");
		exit (-1);
	}
	fprintf (stderr, "malloc_bug: got %d bytes at %08x\n", size, p);

					/* malloc a bunch of small trash to
					   try to use up any free fragments
					   near the large chunk */
	for ( i = 0; i < 2000; i++ )
		(void) malloc (8);
					/* repeatedly free and malloc the
					   large chunk -- if this fails then
					   malloc is broken ... */
	for (;;) {
		fprintf (stderr, "malloc_bug: freeing %d bytes\n", size);
		free (p);
		fprintf (stderr, "malloc_bug: requesting %d bytes\n", size);
		p = malloc (size);
		if ( p == NULL ) {
			perror ("malloc_bug");
			exit (-1);
		}
		fprintf (stderr, "malloc_bug: got %d bytes at %08x\n", size, p);
	}

} /* main */
