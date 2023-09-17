Introduction
============

Lutok uses the GNU Automake, GNU Autoconf and GNU Libtool utilities as
its build system.  These are used only when compiling the library from
the source code package.  If you want to install Lutok from a binary
package, you do not need to read this document.

For the impatient:

    $ ./configure
    $ make
    $ make check
    Gain root privileges
    # make install
    Drop root privileges
    $ make installcheck

Or alternatively, install as a regular user into your home directory:

    $ ./configure --prefix ~/local
    $ make
    $ make check
    $ make install
    $ make installcheck


Dependencies
============

To build and use Lutok successfully you need:

* A standards-compliant C++ complier.
* Lua 5.1 or greater.
* pkg-config.

Optionally, if you want to build and run the tests (recommended), you
need:

* Kyua 0.5 or greater.
* ATF 0.15 or greater.

If you are building Lutok from the code on the repository, you will also
need the following tools:

* GNU Autoconf.
* GNU Automake.
* GNU Libtool.


Regenerating the build system
=============================

This is not necessary if you are building from a formal release
distribution file.

On the other hand, if you are building Lutok from code extracted from
the repository, you must first regenerate the files used by the build
system.  You will also need to do this if you modify configure.ac,
Makefile.am or any of the other build system files.  To do this, simply
run:

    $ autoreconf -i -s

If ATF is installed in a different prefix than Autoconf, you will also
need to tell autoreconf where the ATF M4 macros are located.  Otherwise,
the configure script will be incomplete and will show confusing syntax
errors mentioning, for example, ATF_CHECK_SH.  To fix this, you have
to run autoreconf in the following manner, replacing '<atf-prefix>' with
the appropriate path:

    $ autoreconf -i -s -I <atf-prefix>/share/aclocal


General build procedure
=======================

To build and install the source package, you must follow these steps:

1. Configure the sources to adapt to your operating system.  This is
   done using the 'configure' script located on the sources' top
   directory, and it is usually invoked without arguments unless you
   want to change the installation prefix.  More details on this
   procedure are given on a later section.

2. Build the sources to generate the binaries and scripts.  Simply run
   'make' on the sources' top directory after configuring them.  No
   problems should arise.

3. Install the library by running 'make install'.  You may need to
   become root to issue this step.

4. Issue any manual installation steps that may be required.  These are
   described later in their own section.

5. Check that the installed library works by running 'make
   installcheck'.  You do not need to be root to do this.


Configuration flags
===================

The most common, standard flags given to 'configure' are:

* --prefix=directory
  Possible values: Any path
  Default: /usr/local

  Specifies where the library (binaries and all associated files) will
  be installed.

* --help
  Shows information about all available flags and exits immediately,
  without running any configuration tasks.

The following flags are specific to Lutok's 'configure' script:

* --enable-developer
  Possible values: yes, no
  Default: 'yes' in Git HEAD builds; 'no' in formal releases.

  Enables several features useful for development, such as the inclusion
  of debugging symbols in all objects or the enforcement of compilation
  warnings.

  The compiler will be executed with an exhaustive collection of warning
  detection features regardless of the value of this flag.  However, such
  warnings are only fatal when --enable-developer is 'yes'.

* --with-atf
  Possible values: yes, no, auto.
  Default: auto.

  Enables usage of ATF to build (and later install) the tests.

  Setting this to 'yes' causes the configure script to look for ATF
  unconditionally and abort if not found.  Setting this to 'auto' lets
  configure perform the best decision based on availability of ATF.
  Setting this to 'no' explicitly disables ATF usage.

  When support for tests is enabled, the build process will generate the
  test programs and will later install them into the tests tree.
  Running 'make check' or 'make installcheck' from within the source
  directory will cause these tests to be run with Kyua (assuming it is
  also installed).

* --with-doxygen
  Possible values: yes, no, auto or a path.
  Default: auto.

  Enables usage of Doxygen to generate documentation for internal APIs.

  Setting this to 'yes' causes the configure script to look for Doxygen
  unconditionally and abort if not found.  Setting this to 'auto' lets
  configure perform the best decision based on availability of Doxygen.
  Setting this to 'no' explicitly disables Doxygen usage.  And, lastly,
  setting this to a path forces configure to use a specific Doxygen
  binary, which must exist.

  When support for Doxygen is enabled, the build process will generate
  HTML documentation for the Lutok API.  This documentation will later
  be installed in the HTML directory specified by the configure script.
  You can change the location of the HTML documents by providing your
  desired override with the '--htmldir' flag to the configure script.


Run the tests!
==============

Lastly, after a successful installation (and assuming you built the
sources with support for ATF), you should periodically run the tests
from the final location to ensure things remain stable.  Do so as
follows:

    $ kyua test -k /usr/local/tests/lutok/Kyuafile

And if you see any tests fail, do not hesitate to report them in:

    https://github.com/jmmv/lutok/issues/

Thank you!
