static char junk[] = "\n@(#)LIBF77 VERSION 2.01 6 Sept. 1995\n";

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
	12 March 1993: various tweaks for C++
	 2 June 1994: adjust so abnormal terminations invoke f_exit just once
	16 Sept. 1994: s_cmp: treat characters as unsigned in comparisons.
	19 Sept. 1994: s_paus: flush after end of PAUSE; add -DMSDOS
	12 Jan. 1995:	pow_[dhiqrz][hiq]: adjust x**i to work on machines
			that sign-extend right shifts when i is the most
			negative integer.
	26 Jan. 1995: adjust s_cat.c, s_copy.c to permit the left-hand side
			of character assignments to appear on the right-hand
			side (unless compiled with -DNO_OVERWRITE).
	27 Jan. 1995: minor tweak to s_copy.c: copy forward whenever
			possible (for better cache behavior).
	30 May 1995:  added subroutine exit(rc) integer rc. Version not changed.
	29 Aug. 1995: add F77_aloc.c; use it in s_cat.c and system_.c.
	6 Sept. 1995: fix return type of system_ under -DKR_headers.
*/
