.. _options2configure:

Options to *configure*
======================

There are a number of options to configure which you can use to
control how the Kerberos distribution is built.

Most commonly used options
--------------------------

**-**\ **-help**
    Provides help to configure.  This will list the set of commonly
    used options for building Kerberos.

**-**\ **-prefix=**\ *PREFIX*
    By default, Kerberos will install the package's files rooted at
    ``/usr/local``.  If you desire to place the binaries into the
    directory *PREFIX*, use this option.

**-**\ **-exec-prefix=**\ *EXECPREFIX*
    This option allows one to separate the architecture independent
    programs from the host-dependent files (configuration files,
    manual pages).  Use this option to install architecture-dependent
    programs in *EXECPREFIX*.  The default location is the value of
    specified by **-**\ **-prefix** option.

**-**\ **-localstatedir=**\ *LOCALSTATEDIR*
    This option sets the directory for locally modifiable
    single-machine data.  In Kerberos, this mostly is useful for
    setting a location for the KDC data files, as they will be
    installed in ``LOCALSTATEDIR/krb5kdc``, which is by default
    ``PREFIX/var/krb5kdc``.

**-**\ **-with-netlib**\ [=\ *libs*]
    Allows for suppression of or replacement of network libraries.  By
    default, Kerberos V5 configuration will look for ``-lnsl`` and
    ``-lsocket``.  If your operating system has a broken resolver
    library or fails to pass the tests in ``src/tests/resolv``, you
    will need to use this option.

**-**\ **-enable-dns-for-realm**
    Enable the use of DNS to look up a host's Kerberos realm,
    if the information is not provided in
    :ref:`krb5.conf(5)`.  See :ref:`mapping_hostnames`
    for information about using DNS to determine the default realm.
    DNS lookups for realm names are disabled by default.

**-**\ **-with-system-et**
    Use an installed version of the error-table (et) support software,
    the compile_et program, the com_err.h header file and the com_err
    library.  If these are not in the default locations, you may wish
    to specify ``CPPFLAGS=-I/some/dir`` and
    ``LDFLAGS=-L/some/other/dir`` options at configuration time as
    well.

    If this option is not given, a version supplied with the Kerberos
    sources will be built and installed along with the rest of the
    Kerberos tree, for Kerberos applications to link against.

**-**\ **-with-system-ss**
    Use an installed version of the subsystem command-line interface
    software, the mk_cmds program, the ``ss/ss.h`` header file and the
    ss library.  If these are not in the default locations, you may
    wish to specify ``CPPFLAGS=-I/some/dir`` and
    ``LDFLAGS=-L/some/other/dir`` options at configuration time as
    well.  See also the **SS_LIB** option.

    If this option is not given, the ss library supplied with the
    Kerberos sources will be compiled and linked into those programs
    that need it; it will not be installed separately.

**-**\ **-with-system-db**
    Use an installed version of the Berkeley DB package, which must
    provide an API compatible with version 1.85.  This option is
    unsupported and untested.  In particular, we do not know if the
    database-rename code used in the dumpfile load operation will
    behave properly.

    If this option is not given, a version supplied with the Kerberos
    sources will be built and installed.  (We are not updating this
    version at this time because of licensing issues with newer
    versions that we haven't investigated sufficiently yet.)


Environment variables
---------------------

**CC=**\ *COMPILER*
    Use *COMPILER* as the C compiler.

**CFLAGS=**\ *FLAGS*
    Use *FLAGS* as the default set of C compiler flags.

**CPP=**\ *CPP*
    C preprocessor to use. (e.g., ``CPP='gcc -E'``)

**CPPFLAGS=**\ *CPPOPTS*
    Use *CPPOPTS* as the default set of C preprocessor flags.  The
    most common use of this option is to select certain #define's for
    use with the operating system's include files.


**DB_HEADER=**\ *headername*
    If db.h is not the correct header file to include to compile
    against the Berkeley DB 1.85 API, specify the correct header file
    name with this option. For example, ``DB_HEADER=db3/db_185.h``.

**DB_LIB=**\ *libs*...
    If ``-ldb`` is not the correct library specification for the
    Berkeley DB library version to be used, override it with this
    option. For example, ``DB_LIB=-ldb-3.3``.

**DEFCCNAME=**\ *ccachename*
    Override the built-in default credential cache name.
    For example, ``DEFCCNAME=DIR:/var/run/user/%{USERID}/ccache``
    See :ref:`parameter_expansion` for information about supported
    parameter expansions.

**DEFCKTNAME=**\ *keytabname*
    Override the built-in default client keytab name.
    The format is the same as for *DEFCCNAME*.

**DEFKTNAME=**\ *keytabname*
    Override the built-in default keytab name.
    The format is the same as for *DEFCCNAME*.

**LD=**\ *LINKER*
    Use *LINKER* as the default loader if it should be different from
    C compiler as specified above.

**LDFLAGS=**\ *LDOPTS*
    This option informs the linker where to get additional libraries
    (e.g., ``-L<lib dir>``).

**LIBS=**\ *LDNAME*
    This option allows one to specify libraries to be passed to the
    linker (e.g., ``-l<library>``)

**PKCS11_MODNAME=**\ *library*
    Override the built-in default PKCS11 library name.

**SS_LIB=**\ *libs*...
    If ``-lss`` is not the correct way to link in your installed ss
    library, for example if additional support libraries are needed,
    specify the correct link options here.  Some variants of this
    library are around which allow for Emacs-like line editing, but
    different versions require different support libraries to be
    explicitly specified.

    This option is ignored if **-**\ **-with-system-ss** is not specified.

**YACC**
     The 'Yet Another C Compiler' implementation to use. Defaults to
     the first program found out of: '`bison -y`', '`byacc`',
     '`yacc`'.

**YFLAGS**
     The list of arguments that will be passed by default to $YACC.
     This script will default YFLAGS to the empty string to avoid a
     default value of ``-d`` given by some make applications.


Fine tuning of the installation directories
-------------------------------------------

**-**\ **-bindir=**\ *DIR*
    User executables.  Defaults to ``EXECPREFIX/bin``, where
    *EXECPREFIX* is the path specified by **-**\ **-exec-prefix**
    configuration option.

**-**\ **-sbindir=**\ *DIR*
    System admin executables.  Defaults to ``EXECPREFIX/sbin``, where
    *EXECPREFIX* is the path specified by **-**\ **-exec-prefix**
    configuration option.

**-**\ **-sysconfdir=**\ *DIR*
    Read-only single-machine data such as krb5.conf.
    Defaults to ``PREFIX/etc``, where
    *PREFIX* is the path specified by **-**\ **-prefix** configuration
    option.

**-**\ **-libdir=**\ *DIR*
    Object code libraries.  Defaults to ``EXECPREFIX/lib``, where
    *EXECPREFIX* is the path specified by **-**\ **-exec-prefix**
    configuration option.

**-**\ **-includedir=**\ *DIR*
    C header files.  Defaults to ``PREFIX/include``, where *PREFIX* is
    the path specified by **-**\ **-prefix** configuration option.

**-**\ **-datarootdir=**\ *DATAROOTDIR*
    Read-only architecture-independent data root.  Defaults to
    ``PREFIX/share``, where *PREFIX* is the path specified by
    **-**\ **-prefix** configuration option.

**-**\ **-datadir=**\ *DIR*
    Read-only architecture-independent data.  Defaults to path
    specified by **-**\ **-datarootdir** configuration option.

**-**\ **-localedir=**\ *DIR*
    Locale-dependent data.  Defaults to ``DATAROOTDIR/locale``, where
    *DATAROOTDIR* is the path specified by **-**\ **-datarootdir**
    configuration option.

**-**\ **-mandir=**\ *DIR*
    Man documentation.  Defaults to ``DATAROOTDIR/man``, where
    *DATAROOTDIR* is the path specified by **-**\ **-datarootdir**
    configuration option.


Program names
-------------

**-**\ **-program-prefix=**\ *PREFIX*
    Prepend *PREFIX* to the names of the programs when installing
    them. For example, specifying ``--program-prefix=mit-`` at the
    configure time will cause the program named ``abc`` to be
    installed as ``mit-abc``.

**-**\ **-program-suffix=**\ *SUFFIX*
    Append *SUFFIX* to the names of the programs when installing them.
    For example, specifying ``--program-suffix=-mit`` at the configure
    time will cause the program named ``abc`` to be installed as
    ``abc-mit``.

**-**\ **-program-transform-name=**\ *PROGRAM*
    Run ``sed -e PROGRAM`` on installed program names. (*PROGRAM* is a
    sed script).


System types
------------

**-**\ **-build=**\ *BUILD*
    Configure for building on *BUILD*
    (e.g., ``--build=x86_64-linux-gnu``).

**-**\ **-host=**\ *HOST*
    Cross-compile to build programs to run on *HOST*
    (e.g., ``--host=x86_64-linux-gnu``).  By default, Kerberos V5
    configuration will look for "build" option.


Optional features
-----------------

**-**\ **-disable-option-checking**
    Ignore unrecognized --enable/--with options.

**-**\ **-disable-**\ *FEATURE*
    Do not include *FEATURE* (same as --enable-FEATURE=no).

**-**\ **-enable-**\ *FEATURE*\ [=\ *ARG*]
    Include *FEATURE* [ARG=yes].

**-**\ **-enable-maintainer-mode**
    Enable rebuilding of source files, Makefiles, etc.

**-**\ **-disable-delayed-initialization**
    Initialize library code when loaded.  Defaults to delay until
    first use.

**-**\ **-disable-thread-support**
    Don't enable thread support.  Defaults to enabled.

**-**\ **-disable-rpath**
    Suppress run path flags in link lines.

**-**\ **-enable-athena**
    Build with MIT Project Athena configuration.

**-**\ **-disable-kdc-lookaside-cache**
    Disable the cache which detects client retransmits.

**-**\ **-disable-pkinit**
    Disable PKINIT plugin support.

**-**\ **-disable-aesni**
    Disable support for using AES instructions on x86 platforms.

**-**\ **-enable-asan**\ [=\ *ARG*]
    Enable building with asan memory error checking.  If *ARG* is
    given, it controls the -fsanitize compilation flag value (the
    default is "address").


Optional packages
-----------------

**-**\ **-with-**\ *PACKAGE*\ [=ARG\]
    Use *PACKAGE* (e.g., ``--with-imap``).  The default value of *ARG*
    is ``yes``.

**-**\ **-without-**\ *PACKAGE*
    Do not use *PACKAGE* (same as ``--with-PACKAGE=no``)
    (e.g., ``--without-libedit``).

**-**\ **-with-size-optimizations**
    Enable a few optimizations to reduce code size possibly at some
    run-time cost.

**-**\ **-with-system-et**
    Use the com_err library and compile_et utility that are already
    installed on the system, instead of building and installing
    local versions.

**-**\ **-with-system-ss**
    Use the ss library and mk_cmds utility that are already installed
    on the system, instead of building and using private versions.

**-**\ **-with-system-db**
    Use the berkeley db utility already installed on the system,
    instead of using a private version.  This option is not
    recommended; enabling it may result in incompatibility with key
    databases originating on other systems.

**-**\ **-with-netlib=**\ *LIBS*
    Use the resolver library specified in *LIBS*.  Use this variable
    if the C library resolver is insufficient or broken.

**-**\ **-with-hesiod=**\ *path*
    Compile with Hesiod support.  The *path* points to the Hesiod
    directory.  By default Hesiod is unsupported.

**-**\ **-with-ldap**
    Compile OpenLDAP database backend module.

**-**\ **-with-lmdb**
    Compile LMDB database backend module.

**-**\ **-with-vague-errors**
    Do not send helpful errors to client.  For example, if the KDC
    should return only vague error codes to clients.

**-**\ **-with-crypto-impl=**\ *IMPL*
    Use specified crypto implementation (e.g., **-**\
    **-with-crypto-impl=**\ *openssl*).  The default is the native MIT
    Kerberos implementation ``builtin``.  The other currently
    implemented crypto backend is ``openssl``.  (See
    :ref:`mitK5features`)

**-**\ **-without-libedit**
    Do not compile and link against libedit.  Some utilities will no
    longer offer command history or completion in interactive mode if
    libedit is disabled.

**-**\ **-with-readline**
    Compile and link against GNU readline, as an alternative to libedit.

**-**\ **-with-system-verto**
    Use an installed version of libverto.  If the libverto header and
    library are not in default locations, you may wish to specify
    ``CPPFLAGS=-I/some/dir`` and ``LDFLAGS=-L/some/other/dir`` options
    at configuration time as well.

    If this option is not given, the build system will try to detect
    an installed version of libverto and use it if it is found.
    Otherwise, a version supplied with the Kerberos sources will be
    built and installed.  The built-in version does not contain the
    full set of back-end modules and is not a suitable general
    replacement for the upstream version, but will work for the
    purposes of Kerberos.

    Specifying **-**\ **-without-system-verto** will cause the built-in
    version of libverto to be used unconditionally.

**-**\ **-with-krb5-config=**\ *PATH*
    Use the krb5-config program at *PATH* to obtain the build-time
    default credential cache, keytab, and client keytab names.  The
    default is to use ``krb5-config`` from the program path.  Specify
    ``--without-krb5-config`` to disable the use of krb5-config and
    use the usual built-in defaults.

**-**\ **-without-keyutils**
    Build without libkeyutils support.  This disables the KEYRING
    credential cache type.


Examples
--------

For example, in order to configure Kerberos on a Solaris machine using
the suncc compiler with the optimizer turned on, run the configure
script with the following options::

    % ./configure CC=suncc CFLAGS=-O

For a slightly more complicated example, consider a system where
several packages to be used by Kerberos are installed in
``/usr/foobar``, including Berkeley DB 3.3, and an ss library that
needs to link against the curses library.  The configuration of
Kerberos might be done thus::

    ./configure CPPFLAGS=-I/usr/foobar/include LDFLAGS=-L/usr/foobar/lib \
    --with-system-et --with-system-ss --with-system-db  \
    SS_LIB='-lss -lcurses'  DB_HEADER=db3/db_185.h DB_LIB=-ldb-3.3
