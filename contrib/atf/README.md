# Welcome to the ATF project!

ATF, or Automated Testing Framework, is a **collection of libraries** to
write test programs in **C, C++ and POSIX shell**.

The ATF libraries offer a simple API.  The API is orthogonal through the
various bindings, allowing developers to quickly learn how to write test
programs in different languages.

ATF-based test programs offer a **consistent end-user command-line
interface** to allow both humans and automation to run the tests.

ATF-based test programs **rely on an execution engine** to be run and
this execution engine is *not* shipped with ATF.
**[Kyua](https://github.com/jmmv/kyua/) is the engine of choice.**

## Download

Formal releases for source files are available for download from GitHub:

* [atf 0.20](../../releases/tag/atf-0.20), released on February 7th, 2014.

## Installation

You are encouraged to install binary packages for your operating system
wherever available:

* Fedora 20 and above: install the `atf` package with `yum install atf`.

* FreeBSD 10.0 and above: install the `atf` package with `pkg install atf`.

* NetBSD with pkgsrc: install the `pkgsrc/devel/atf` package.

* OpenBSD: install the `devel/atf` package with `pkg_add atf`.

Should you want to build and install ATF from the source tree provided
here, follow the instructions in the [INSTALL file](INSTALL).

## Support

Please use the
[atf-discuss mailing list](https://groups.google.com/forum/#!forum/atf-discuss)
for any support inquiries related to `atf-c`, `atf-c++` or `atf-sh`.

If you have any questions on Kyua proper, please use the
[kyua-discuss mailing list](https://groups.google.com/forum/#!forum/kyua-discuss)
instead.
