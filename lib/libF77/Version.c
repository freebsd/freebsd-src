static char junk[] = "\n@(#)LIBF77 VERSION 2.01 12 March 1993\n";

/*
2.00	11 June 1980.  File version.c added to library.
2.01	31 May 1988.  s_paus() flushes stderr; names of hl_* fixed
	[ d]erf[c ] added
	 8 Aug. 1989: #ifdefs for f2c -i2 added to s_cat.c
	29 Nov. 1989: s_cmp returns long (for f2c)
	30 Nov. 1989: arg types from f2c.h
	12 Dec. 1989: s_rnge allows long names
	19 Dec. 1989: getenv_ allows unsorted environment
	28 Mar. 1990: add exit(0) to end of main()
	 2 Oct. 1990: test signal(...) == SIG_IGN rather than & 01 in main
	17 Oct. 1990: abort() calls changed to sig_die(...,1)
	22 Oct. 1990: separate sig_die from main
	25 Apr. 1991: minor, theoretically invisible tweaks to s_cat, sig_die
	31 May  1991: make system_ return status
	18 Dec. 1991: change long to ftnlen (for -i2) many places
	28 Feb. 1992: repair z_sqrt.c (scribbled on input, gave wrong answer)
	18 July 1992: for n < 0, repair handling of 0**n in pow_[dr]i.c
			and m**n in pow_hh.c and pow_ii.c;
			catch SIGTRAP in main() for error msg before abort
	23 July 1992: switch to ANSI prototypes unless KR_headers is #defined
	23 Oct. 1992: fix botch in signal_.c (erroneous deref of 2nd arg);
			change Cabs to f__cabs.
	12 March 1993: various tweaks for C++.
*/
