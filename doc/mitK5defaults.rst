.. _mitK5defaults:

MIT Kerberos defaults
=====================

General defaults
----------------

========================================== =============================  ====================
Description                                   Default                        Environment
========================================== =============================  ====================
:ref:`keytab_definition` file               |keytab|                       **KRB5_KTNAME**
Client :ref:`keytab_definition` file        |ckeytab|                      **KRB5_CLIENT_KTNAME**
Kerberos config file :ref:`krb5.conf(5)`    |krb5conf|\ ``:``\             **KRB5_CONFIG**
                                            |sysconfdir|\ ``/krb5.conf``
KDC config file :ref:`kdc.conf(5)`          |kdcdir|\ ``/kdc.conf``        **KRB5_KDC_PROFILE**
KDC database path (DB2)                     |kdcdir|\ ``/principal``
Master key :ref:`stash_definition`          |kdcdir|\ ``/.k5.``\ *realm*
Admin server ACL file :ref:`kadm5.acl(5)`   |kdcdir|\ ``/kadm5.acl``
OTP socket directory                        |kdcrundir|
Plugin base directory                       |libdir|\ ``/krb5/plugins``
:ref:`rcache_definition` directory          ``/var/tmp``                   **KRB5RCACHEDIR**
Master key default enctype                  |defmkey|
Default :ref:`keysalt list<Keysalt_lists>`  |defkeysalts|
Permitted enctypes                          |defetypes|
KDC default port                            88
Admin server port                           749
Password change port                        464
========================================== =============================  ====================


Slave KDC propagation defaults
------------------------------

This table shows defaults used by the :ref:`kprop(8)` and
:ref:`kpropd(8)` programs.

==========================  ==============================  ===========
Description                 Default                         Environment
==========================  ==============================  ===========
kprop database dump file    |kdcdir|\ ``/slave_datatrans``
kpropd temporary dump file  |kdcdir|\ ``/from_master``
kdb5_util location          |sbindir|\ ``/kdb5_util``
kprop location              |sbindir|\ ``/kprop``
kpropd ACL file             |kdcdir|\ ``/kpropd.acl``
kprop port                  754                             KPROP_PORT
==========================  ==============================  ===========


.. _paths:

Default paths for Unix-like systems
-----------------------------------

On Unix-like systems, some paths used by MIT krb5 depend on parameters
chosen at build time.  For a custom build, these paths default to
subdirectories of ``/usr/local``.  When MIT krb5 is integrated into an
operating system, the paths are generally chosen to match the
operating system's filesystem layout.

==========================  =============  ===========================  ===========================
Description                 Symbolic name  Custom build path            Typical OS path
==========================  =============  ===========================  ===========================
User programs               BINDIR         ``/usr/local/bin``           ``/usr/bin``
Libraries and plugins       LIBDIR         ``/usr/local/lib``           ``/usr/lib``
Parent of KDC state dir     LOCALSTATEDIR  ``/usr/local/var``           ``/var``
Parent of KDC runtime dir   RUNSTATEDIR    ``/usr/local/var/run``       ``/run``
Administrative programs     SBINDIR        ``/usr/local/sbin``          ``/usr/sbin``
Alternate krb5.conf dir     SYSCONFDIR     ``/usr/local/etc``           ``/etc``
Default ccache name         DEFCCNAME      ``FILE:/tmp/krb5cc_%{uid}``  ``FILE:/tmp/krb5cc_%{uid}``
Default keytab name         DEFKTNAME      ``FILE:/etc/krb5.keytab``    ``FILE:/etc/krb5.keytab``
==========================  =============  ===========================  ===========================

The default client keytab name (DEFCKTNAME) typically defaults to
``FILE:/usr/local/var/krb5/user/%{euid}/client.keytab`` for a custom
build.  A native build will typically use a path which will vary
according to the operating system's layout of ``/var``.
