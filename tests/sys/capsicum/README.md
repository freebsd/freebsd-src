# Capsicum User Space Tests

This directory holds unit tests for [Capsicum](https://man.freebsd.org/cgi/man.cgi?query=capsicum)
object-capabilities. The tests exercise the syscall interface to a Capsicum-enabled operating system,
Currently, [FreeBSD >=10.x](http://www.freebsd.org) is the only such operating system.

The tests are written in C++11 and use the [Google Test](https://code.google.com/p/googletest/)
framework, with some additions to fork off particular tests (because a process that enters capability
mode cannot leave it again).

## Provenance

The original basis for these tests was:

 - [unit tests](https://github.com/freebsd/freebsd/tree/master/tools/regression/security/cap_test)
   written by Robert Watson and Jonathan Anderson for the original FreeBSD 9.x Capsicum implementation
 - [unit tests](http://git.chromium.org/gitweb/?p=chromiumos/third_party/kernel-capsicum.git;a=tree;f=tools/testing/capsicum_tests;hb=refs/heads/capsicum) written by Meredydd Luff for the original Capsicum-Linux port.

These tests were coalesced and moved into an [independent repository](https://github.com/google/capsicum-test)
to enable comparative testing across multiple OSes, and then substantially extended.

Subsequently, the [capsicum-linux port](https://github.com/google/capsicum-linux) was abandoned by
its maintainers, rendering the independent repository obsolete.  So the tests were copied back into
the FreeBSD source tree in time for 16.0-RELEASE.

## Configuration

The following kernel configuration options are needed so that all tests can run:

  - `options P1003_1B_MQUEUE`: Enable POSIX message queues (or `kldload mqueuefs`)
