# Compiling tcpdump on NetBSD

NetBSD has two libpcap libraries: one that is always installed as a part of the
OS and another that can be installed as a package from pkgsrc.  Also the usual
method of compiling with the upstream libpcap in `../libpcap` is available.

GCC, Clang, Autoconf and CMake are presumed to work, if this is not the case,
please report a bug as explained in the
[guidelines for contributing](../CONTRIBUTING.md).

## NetBSD 9.3

* Upstream libpcap works.
* OS libpcap works.
* pkgsrc libpcap works.

## NetBSD 9.2, 9.1 and 9.0

* Upstream libpcap works.
* OS libpcap cannot be used due to
  [this bug](https://gnats.netbsd.org/cgi-bin/query-pr-single.pl?number=55901).

