# The One True Awk

This is the version of `awk` described in _The AWK Programming Language_,
by Al Aho, Brian Kernighan, and Peter Weinberger
(Addison-Wesley, 1988, ISBN 0-201-07981-X).

## Copyright

Copyright (C) Lucent Technologies 1997<br/>
All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that the copyright notice and this
permission notice and warranty disclaimer appear in supporting
documentation, and that the name Lucent Technologies or any of
its entities not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.

LUCENT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
IN NO EVENT SHALL LUCENT OR ANY OF ITS ENTITIES BE LIABLE FOR ANY
SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
THIS SOFTWARE.

## Distribution and Reporting Problems

Changes, mostly bug fixes and occasional enhancements, are listed
in `FIXES`.  If you distribute this code further, please please please
distribute `FIXES` with it.

If you find errors, please report them
to bwk@cs.princeton.edu.
Please _also_ open an issue in the GitHub issue tracker, to make
it easy to track issues.
Thanks.

## Submitting Pull Requests

Pull requests are welcome. Some guidelines:

* Please do not use functions or facilities that are not standard (e.g.,
`strlcpy()`, `fpurge()`).

* Please run the test suite and make sure that your changes pass before
posting the pull request. To do so:

  1. Save the previous version of `awk` somewhere in your path. Call it `nawk` (for example).
  1. Run `oldawk=nawk make check > check.out 2>&1`.
  1. Search for `BAD` or `error` in the result. In general, look over it manually to make sure there are no errors.

* Please create the pull request with a request
to merge into the `staging` branch instead of into the `master` branch.
This allows us to do testing, and to make any additional edits or changes
after the merge but before merging to `master`.

## Building

The program itself is created by

	make

which should produce a sequence of messages roughly like this:

	yacc -d awkgram.y
	conflicts: 43 shift/reduce, 85 reduce/reduce
	mv y.tab.c ytab.c
	mv y.tab.h ytab.h
	cc -c ytab.c
	cc -c b.c
	cc -c main.c
	cc -c parse.c
	cc maketab.c -o maketab
	./maketab >proctab.c
	cc -c proctab.c
	cc -c tran.c
	cc -c lib.c
	cc -c run.c
	cc -c lex.c
	cc ytab.o b.o main.o parse.o proctab.o tran.o lib.o run.o lex.o -lm

This produces an executable `a.out`; you will eventually want to
move this to some place like `/usr/bin/awk`.

If your system does not have `yacc` or `bison` (the GNU
equivalent), you need to install one of them first.

NOTE: This version uses ANSI C (C 99), as you should also.  We have
compiled this without any changes using `gcc -Wall` and/or local C
compilers on a variety of systems, but new systems or compilers
may raise some new complaint; reports of difficulties are
welcome.

This compiles without change on Macintosh OS X using `gcc` and
the standard developer tools.

You can also use `make CC=g++` to build with the GNU C++ compiler,
should you choose to do so.

The version of `malloc` that comes with some systems is sometimes
astonishly slow.  If `awk` seems slow, you might try fixing that.
More generally, turning on optimization can significantly improve
`awk`'s speed, perhaps by 1/3 for highest levels.

## A Note About Releases

We don't do releases. 

## A Note About Maintenance

NOTICE! Maintenance of this program is on a ''best effort''
basis.  We try to get to issues and pull requests as quickly
as we can.  Unfortunately, however, keeping this program going
is not at the top of our priority list.

#### Last Updated

Sat Jul 25 14:00:07 EDT 2021
