Organization of the source directory
====================================

Below is a brief overview of the organization of the complete source
directory.  More detailed descriptions follow.

=============== ==============================================
appl             Kerberos application client and server programs
ccapi            Credential cache services
clients          Kerberos V5 user programs (See :ref:`user_commands`)
config           Configure scripts
config-files     Sample Kerberos configuration files
include          include files needed to build the Kerberos system
kadmin           Administrative interface to the Kerberos master database: :ref:`kadmin(1)`, :ref:`kdb5_util(8)`, :ref:`ktutil(1)`.
kdc              Kerberos V5 Authentication Service and Key Distribution Center
lib_             Libraries for use with/by Kerberos V5
plugins          Kerberos plugins directory
po               Localization infrastructure
prototype        Templates files containing the MIT copyright message and a placeholder for the title and description of the file.
slave            Utilities for propagating the database to slave KDCs :ref:`kprop(8)` and :ref:`kpropd(8)`
tests            Test suite
util_            Various utilities for building/configuring the code, sending bug reports, etc.
windows          Source code for building Kerberos V5 on Windows (see windows/README)
=============== ==============================================


.. _lib:

lib
---

The lib directory contain several subdirectories as well as some
definition and glue files.

  - The apputils directory contains the code for the generic network
    servicing.
  - The crypto subdirectory contains the Kerberos V5 encryption
    library.
  - The gssapi library contains the Generic Security Services API,
    which is a library of commands to be used in secure client-server
    communication.
  - The kadm5 directory contains the libraries for the KADM5
    administration utilities.
  - The Kerberos 5 database libraries are contained in kdb.
  - The krb5 directory contains Kerberos 5 API.
  - The rpc directory contains the API for the Kerberos Remote
    Procedure Call protocol.


.. _util:

util
----

The util directory contains several utility programs and libraries.
  - the programs used to configure and build the code, such as
    autoconf, lndir, kbuild, reconf, and makedepend, are in this
    directory.
  - the profile directory contains most of the functions which parse
    the Kerberos configuration files (krb5.conf and kdc.conf).
  - the Kerberos error table library and utilities (et);
  - the Sub-system library and utilities (ss);
  - database utilities (db2);
  - pseudo-terminal utilities (pty);
  - bug-reporting program send-pr;
  - a generic support library support used by several of our other
    libraries;
  - the build infrastructure for building lightweight Kerberos client
    (collected-client-lib)
  - the tool for validating Kerberos configuration files
    (confvalidator);
  - the toolkit for kernel integrators for building krb5 code subsets
    (gss-kernel-lib);
  - source code for building Kerberos V5 on MacOS (mac)
  - Windows getopt operations (windows)
