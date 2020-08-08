# Release Checklist

This is the checklist for cutting a release.

1.	Update the README.
2.	Update the manuals.
3.	Test history manually.
4.	Test with POSIX test suite.
5.	Run the randmath.py script an excessive amount and add failing tests to
	test suite.
	* debug
	* release
	* minrelease
6.	Fuzz with AFL.
	* reldebug
7.	Fix AFL crashes.
8.	Find ASan crashes on AFL test cases.
9.	Fix ASan crashes.
10.	Build with xstatic.
11.	Run and pass the `release.sh` script on my own machine.
12.	Run and pass the `release.sh` script, without generated tests and
	sanitizers, on FreeBSD.
13.	Run and pass the `release.sh` script, without generated tests, sanitizers,
	and 64-bit, on Thalheim's ARM server.
14.	Run and pass the release script, with no generated tests, no clang, no
	sanitizers, and no valgrind, on NetBSD.
15.	Run and pass the release script, with no generated tests, no clang, no
	sanitizers, and no valgrind, on OpenBSD.
16.	Run Coverity Scan and eliminate warnings, if possible (both only).
	* debug
17.	Run `scan-build make`.
18.	Repeat steps 3-14 again and repeat until nothing is found.
19.	Update the benchmarks.
20.	Change the version (remove "-dev") and commit.
21.	Run `make clean_tests`.
22.	Run the release script.
23.	Upload the custom tarball to GitHub.
24.	Add sha's to release notes.
25.	Edit release notes for the changelog.
26.	Increment to the next version (with "-dev").
27.	Notify the following:
	* FreeBSD
	* Adelie Linux
	* Ataraxia Linux
	* Sabotage
	* xstatic
	* OpenBSD
	* NetBSD
28.	Submit new packages for the following:
	* Alpine Linux
	* Void Linux
	* Gentoo Linux
	* Linux from Scratch
	* Arch Linux
