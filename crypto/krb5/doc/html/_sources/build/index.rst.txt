.. _build_V5:

Building Kerberos V5
====================

This section details how to build and install MIT Kerberos software
from the source.

Prerequisites
-------------

In order to build Kerberos V5, you will need approximately 60-70
megabytes of disk space.  The exact amount will vary depending on the
platform and whether the distribution is compiled with debugging
symbol tables or not.

Your C compiler must conform to ANSI C (ISO/IEC 9899:1990, "c89").
Some operating systems do not have an ANSI C compiler, or their
default compiler requires extra command-line options to enable ANSI C
conformance.

If you wish to keep a separate build tree, which contains the compiled
\*.o file and executables, separate from your source tree, you will
need a make program which supports **VPATH**, or you will need to use
a tool such as lndir to produce a symbolic link tree for your build
tree.

Obtaining the software
----------------------

The source code can be obtained from MIT Kerberos Distribution page,
at https://kerberos.org/dist/index.html.
The MIT Kerberos distribution comes in an archive file, generally
named krb5-VERSION-signed.tar, where *VERSION* is a placeholder for
the major and minor versions of MIT Kerberos.  (For example, MIT
Kerberos 1.9 has major version "1" and minor version "9".)

The krb5-VERSION-signed.tar contains a compressed tar file consisting
of the sources for all of Kerberos (generally named
krb5-VERSION.tar.gz) and a PGP signature file for this source tree
(generally named krb5-VERSION.tar.gz.asc).  MIT highly recommends that
you verify the integrity of the source code using this signature,
e.g., by running::

    tar xf krb5-VERSION-signed.tar
    gpg --verify krb5-VERSION.tar.gz.asc

Unpack krb5-VERSION.tar.gz in some directory. In this section we will assume
that you have chosen the top directory of the distribution the directory
``/u1/krb5-VERSION``.

Review the README file for the license, copyright and other sprecific to the
distribution information.

Contents
--------
.. toctree::
   :maxdepth: 1

   directory_org.rst
   doing_build.rst
   options2configure.rst
   osconf.rst
