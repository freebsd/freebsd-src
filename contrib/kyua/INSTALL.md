Installation instructions
=========================

Kyua uses the GNU Automake, GNU Autoconf and GNU Libtool utilities as
its build system.  These are used only when compiling the application
from the source code package.  If you want to install Kyua from a binary
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
------------

To build and use Kyua successfully you need:

* A standards-compliant C and C++ complier.
* Lutok 0.4.
* pkg-config.
* SQLite 3.6.22.

To build the Kyua tests, you optionally need:

* The Automated Testing Framework (ATF), version 0.15 or greater.  This
  is required if you want to create a distribution file.

If you are building Kyua from the code on the repository, you will also
need the following tools:

* GNU Autoconf.
* GNU Automake.
* GNU Libtool.


Regenerating the build system
-----------------------------

This is not necessary if you are building from a formal release
distribution file.

On the other hand, if you are building Kyua from code extracted from the
repository, you must first regenerate the files used by the build
system.  You will also need to do this if you modify `configure.ac`,
`Makefile.am` or any of the other build system files.  To do this, simply
run:

    $ autoreconf -i -s

If ATF is installed in a different prefix than Autoconf, you will also
need to tell autoreconf where the ATF M4 macros are located.  Otherwise,
the configure script will be incomplete and will show confusing syntax
errors mentioning, for example, `ATF_CHECK_SH`.  To fix this, you have
to run autoreconf in the following manner, replacing `<atf-prefix>` with
the appropriate path:

    $ autoreconf -i -s -I <atf-prefix>/share/aclocal


General build procedure
-----------------------

To build and install the source package, you must follow these steps:

1. Configure the sources to adapt to your operating system.  This is
   done using the `configure` script located on the sources' top
   directory, and it is usually invoked without arguments unless you
   want to change the installation prefix.  More details on this
   procedure are given on a later section.

2. Build the sources to generate the binaries and scripts.  Simply run
   `make` on the sources' top directory after configuring them.  No
   problems should arise.

3. Check that the built programs work by running `make check`.  You do
   not need to be root to do this, but if you are not, some checks will
   be skipped.

4. Install the program by running `make install`.  You may need to
   become root to issue this step.

5. Issue any manual installation steps that may be required.  These are
   described later in their own section.

6. Check that the installed programs work by running `make
   installcheck`.  You do not need to be root to do this, but if you are
   not, some checks will be skipped.


Configuration flags
-------------------

The most common, standard flags given to `configure` are:

* `--prefix=directory`:
  **Possible values:** Any path.
  **Default:** `/usr/local`.

  Specifies where the program (binaries and all associated files) will
  be installed.

* `--sysconfdir=directory`:
  **Possible values:** Any path.
  **Default:** `/usr/local/etc`.

  Specifies where the installed programs will look for configuration
  files.  `/kyua` will be appended to the given path unless
  `KYUA_CONFSUBDIR` is redefined as explained later on.

* `--help`:

  Shows information about all available flags and exits immediately,
  without running any configuration tasks.

The following environment variables are specific to Kyua's `configure`
script:

* `GDB`:
  **Possible values:** empty, absolute path to GNU GDB.
  **Default:** empty.

  Specifies the path to the GNU GDB binary that Kyua will use to gather a
  stack trace of a crashing test program.  If empty, the configure script
  will try to find a suitable binary for you and, if not found, Kyua will
  attempt to do the search at run time.

* `KYUA_ARCHITECTURE`:
  **Possible values:** name of a CPU architecture (e.g. `x86_64`, `powerpc`).
  **Default:** autodetected; typically the output of `uname -p`.

  Specifies the name of the CPU architecture on which Kyua will run.
  This value is used at run-time to determine tests that are not
  applicable to the host system.

* `KYUA_CONFSUBDIR`:
  **Possible values:** empty, a relative path.
  **Default:** `kyua`.

  Specifies the subdirectory of the configuration directory (given by
  the `--sysconfdir` argument) under which Kyua will search for its
  configuration files.

* `KYUA_CONFIG_FILE_FOR_CHECK`:
  **Possible values:** none, an absolute path to an existing file.
  **Default:** none.

  Specifies the `kyua.conf` configuration file to use when running any
  of the `check`, `installcheck` or `distcheck` targets on this source
  tree.  This setting is exclusively used to customize the test runs of
  Kyua itself and has no effect whatsoever on the built product.

* `KYUA_MACHINE`:
  **Possible values:** name of a machine type (e.g. `amd64`, `macppc`).
  **Default:** autodetected; typically the output of `uname -m`.

  Specifies the name of the machine type on which Kyua will run.  This
  value is used at run-time to determine tests that are not applicable
  to the host system.

* `KYUA_TMPDIR`:
  **Possible values:** an absolute path to a temporary directory.
  **Default:** `/tmp`.

  Specifies the path that Kyua will use to create temporary directories
  in by default.

The following flags are specific to Kyua's `configure` script:

* `--enable-developer`:
  **Possible values:** `yes`, `no`.
  **Default:** `yes` in Git `HEAD` builds; `no` in formal releases.

  Enables several features useful for development, such as the inclusion
  of debugging symbols in all objects or the enforcement of compilation
  warnings.

  The compiler will be executed with an exhaustive collection of warning
  detection features regardless of the value of this flag.  However, such
  warnings are only fatal when `--enable-developer` is `yes`.

* `--with-atf`:
  **Possible values:** `yes`, `no`, `auto`.
  **Default:** `auto`.

  Enables usage of ATF to build (and later install) the tests.

  Setting this to `yes` causes the configure script to look for ATF
  unconditionally and abort if not found.  Setting this to `auto` lets
  configure perform the best decision based on availability of ATF.
  Setting this to `no` explicitly disables ATF usage.

  When support for tests is enabled, the build process will generate the
  test programs and will later install them into the tests tree.
  Running `make check` or `make installcheck` from within the source
  directory will cause these tests to be run with Kyua.

* `--with-doxygen`:
  **Possible values:** `yes`, `no`, `auto` or a path.
  **Default:** `auto`.

  Enables usage of Doxygen to generate documentation for internal APIs.
  This documentation is *not* installed and is only provided to help the
  developer of this package.  Therefore, enabling or disabling Doxygen
  causes absolutely no differences on the files installed by this
  package.

  Setting this to `yes` causes the configure script to look for Doxygen
  unconditionally and abort if not found.  Setting this to `auto` lets
  configure perform the best decision based on availability of Doxygen.
  Setting this to `no` explicitly disables Doxygen usage.  And, lastly,
  setting this to a path forces configure to use a specific Doxygen
  binary, which must exist.


Post-installation steps
-----------------------

Copy the `Kyuafile.top` file installed in the examples directory to the
root of your tests hierarchy and name it `Kyuafile`.  For example:

    # cp /usr/local/share/kyua/examples/Kyuafile.top \
         /usr/local/tests/Kyuafile

This will allow you to simply go into `/usr/tests` and run the tests
from there.


Run the tests!
--------------

Lastly, after a successful installation, you should periodically run the
tests from the final location to ensure things remain stable.  Do so as
follows:

    $ cd /usr/local/kyua && kyua test

The following configuration variables are specific to the 'kyua' test
suite and can be given to Kyua with arguments of the form
`-v test_suites.kyua.<variable_name>=<value>`:

* `run_coredump_tests`:
  **Possible values:** `true` or `false`.
  **Default:** `true`.

  Avoids running tests that crash subprocesses on purpose to make them
  dump core.  Such tests are particularly slow on macOS, and it is
  sometimes handy to disable them for quicker development iteration.

If you see any tests fail, do not hesitate to report them in:

    https://github.com/jmmv/kyua/issues/

Thank you!
