# Release Checklist

This is the checklist for cutting a release.

For a lot of these steps, they are only needed if the code that would be
affected was changed. For example, I don't need to run the `scripts/randmath.py`
test if I did not change any of the math code.

1.	Update the README.
2.	Update the manuals.
3.	Test history manually.
4.	Test with POSIX test suite.
5.	Run the `scripts/randmath.py` script an excessive amount and add failing
	tests to test suite.
	* debug
	* release
	* minrelease
6.	Fuzz with AFL.
	* reldebug
7.	Fix AFL crashes.
8.	Find ASan crashes on AFL test cases.
9.	Fix ASan crashes.
10.	Build on Windows, no errors or warnings.
	* Debug/`x64`.
	* Debug/`x86`.
	* Release{MD,MT}/`x64`.
	* Release{MD,MT}/`x86`.
11.	Run and pass the `scripts/release.sh` script on my own machine.
12.	Run and pass the `scripts/release.sh` script, without generated tests and
	sanitizers, on FreeBSD.
13.	Run and pass the `scripts/release.sh` script, without generated tests,
	sanitizers, and 64-bit, on an ARM server.
14.	Run and pass the release script, with no generated tests, no clang, no
	sanitizers, and no valgrind, on NetBSD.
15.	Run and pass the release script, with no generated tests, no sanitizers, and
	no valgrind, on OpenBSD.
16.	Run `scan-build make`.
17.	Repeat steps 3-16 again and repeat until nothing is found.
18.	Update the benchmarks.
19.	Update the version and `NEWS.md` and commit.
20. Boot into Windows.
21. Build all release versions of everything.
	* Release/`x64` for `bc`.
	* Release/`x64` for `dc`.
	* Release{MD,MT}/`x64` for `bcl`.
	* Release/`x86` for `bc`.
	* Release/`x86` for `dc`.
	* Release{MD,MT}/`x86` for `bcl`.
22.	Put the builds where Linux can access them.
23. Boot back into Linux.
24.	Run `make clean_tests`.
25.	Run the `scripts/package.sh` script.
26.	Upload the custom tarball and Windows builds to Yzena Gitea.
27.	Add output from `scripts/package.sh` to Yzena Gitea release notes.
28.	Edit Yzena Gitea release notes for the changelog.
29.	Upload the custom tarball to GitHub.
30.	Add output from `scripts/package.sh` to GitHub release notes.
31.	Edit GitHub release notes for the changelog.
32.	Notify the following:
	* FreeBSD
	* Adelie Linux
	* Ataraxia Linux
	* Sabotage
	* xstatic
	* OpenBSD
	* NetBSD
33.	Submit new packages for the following:
	* Alpine Linux
	* Void Linux
	* Gentoo Linux
	* Linux from Scratch
	* Arch Linux
