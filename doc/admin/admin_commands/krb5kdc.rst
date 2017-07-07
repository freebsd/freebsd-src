.. _krb5kdc(8):

krb5kdc
=======

SYNOPSIS
--------

**krb5kdc**
[**-x** *db_args*]
[**-d** *dbname*]
[**-k** *keytype*]
[**-M** *mkeyname*]
[**-p** *portnum*]
[**-m**]
[**-r** *realm*]
[**-n**]
[**-w** *numworkers*]
[**-P** *pid_file*]
[**-T** *time_offset*]


DESCRIPTION
-----------

krb5kdc is the Kerberos version 5 Authentication Service and Key
Distribution Center (AS/KDC).


OPTIONS
-------

The **-r** *realm* option specifies the realm for which the server
should provide service.

The **-d** *dbname* option specifies the name under which the
principal database can be found.  This option does not apply to the
LDAP database.

The **-k** *keytype* option specifies the key type of the master key
to be entered manually as a password when **-m** is given; the default
is ``des-cbc-crc``.

The **-M** *mkeyname* option specifies the principal name for the
master key in the database (usually ``K/M`` in the KDC's realm).

The **-m** option specifies that the master database password should
be fetched from the keyboard rather than from a stash file.

The **-n** option specifies that the KDC does not put itself in the
background and does not disassociate itself from the terminal.  In
normal operation, you should always allow the KDC to place itself in
the background.

The **-P** *pid_file* option tells the KDC to write its PID into
*pid_file* after it starts up.  This can be used to identify whether
the KDC is still running and to allow init scripts to stop the correct
process.

The **-p** *portnum* option specifies the default UDP port numbers
which the KDC should listen on for Kerberos version 5 requests, as a
comma-separated list.  This value overrides the UDP port numbers
specified in the :ref:`kdcdefaults` section of :ref:`kdc.conf(5)`, but
may be overridden by realm-specific values.  If no value is given from
any source, the default port is 88.

The **-w** *numworkers* option tells the KDC to fork *numworkers*
processes to listen to the KDC ports and process requests in parallel.
The top level KDC process (whose pid is recorded in the pid file if
the **-P** option is also given) acts as a supervisor.  The supervisor
will relay SIGHUP signals to the worker subprocesses, and will
terminate the worker subprocess if the it is itself terminated or if
any other worker process exits.

.. note::

          On operating systems which do not have *pktinfo* support,
          using worker processes will prevent the KDC from listening
          for UDP packets on network interfaces created after the KDC
          starts.

The **-x** *db_args* option specifies database-specific arguments.
See :ref:`Database Options <dboptions>` in :ref:`kadmin(1)` for
supported arguments.

The **-T** *offset* option specifies a time offset, in seconds, which
the KDC will operate under.  It is intended only for testing purposes.

EXAMPLE
-------

The KDC may service requests for multiple realms (maximum 32 realms).
The realms are listed on the command line.  Per-realm options that can
be specified on the command line pertain for each realm that follows
it and are superseded by subsequent definitions of the same option.

For example::

    krb5kdc -p 2001 -r REALM1 -p 2002 -r REALM2 -r REALM3

specifies that the KDC listen on port 2001 for REALM1 and on port 2002
for REALM2 and REALM3.  Additionally, per-realm parameters may be
specified in the :ref:`kdc.conf(5)` file.  The location of this file
may be specified by the **KRB5_KDC_PROFILE** environment variable.
Per-realm parameters specified in this file take precedence over
options specified on the command line.  See the :ref:`kdc.conf(5)`
description for further details.


ENVIRONMENT
-----------

krb5kdc uses the following environment variables:

* **KRB5_CONFIG**
* **KRB5_KDC_PROFILE**


SEE ALSO
--------

:ref:`kdb5_util(8)`, :ref:`kdc.conf(5)`, :ref:`krb5.conf(5)`,
:ref:`kdb5_ldap_util(8)`
