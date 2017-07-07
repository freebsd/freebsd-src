Installing KDCs
===============

When setting up Kerberos in a production environment, it is best to
have multiple slave KDCs alongside with a master KDC to ensure the
continued availability of the Kerberized services.  Each KDC contains
a copy of the Kerberos database.  The master KDC contains the writable
copy of the realm database, which it replicates to the slave KDCs at
regular intervals.  All database changes (such as password changes)
are made on the master KDC.  Slave KDCs provide Kerberos
ticket-granting services, but not database administration, when the
master KDC is unavailable.  MIT recommends that you install all of
your KDCs to be able to function as either the master or one of the
slaves.  This will enable you to easily switch your master KDC with
one of the slaves if necessary (see :ref:`switch_master_slave`).  This
installation procedure is based on that recommendation.

.. warning::

    - The Kerberos system relies on the availability of correct time
      information.  Ensure that the master and all slave KDCs have
      properly synchronized clocks.

    - It is best to install and run KDCs on secured and dedicated
      hardware with limited access.  If your KDC is also a file
      server, FTP server, Web server, or even just a client machine,
      someone who obtained root access through a security hole in any
      of those areas could potentially gain access to the Kerberos
      database.


Install and configure the master KDC
------------------------------------

Install Kerberos either from the OS-provided packages or from the
source (See :ref:`do_build`).

.. note::

          For the purpose of this document we will use the following
          names::

             kerberos.mit.edu    - master KDC
             kerberos-1.mit.edu  - slave KDC
             ATHENA.MIT.EDU      - realm name
             .k5.ATHENA.MIT.EDU  - stash file
             admin/admin         - admin principal

          See :ref:`mitK5defaults` for the default names and locations
          of the relevant to this topic files.  Adjust the names and
          paths to your system environment.


Edit KDC configuration files
----------------------------

Modify the configuration files, :ref:`krb5.conf(5)` and
:ref:`kdc.conf(5)`, to reflect the correct information (such as
domain-realm mappings and Kerberos servers names) for your realm.
(See :ref:`mitK5defaults` for the recommended default locations for
these files).

Most of the tags in the configuration have default values that will
work well for most sites.  There are some tags in the
:ref:`krb5.conf(5)` file whose values must be specified, and this
section will explain those.

If the locations for these configuration files differs from the
default ones, set **KRB5_CONFIG** and **KRB5_KDC_PROFILE** environment
variables to point to the krb5.conf and kdc.conf respectively.  For
example::

    export KRB5_CONFIG=/yourdir/krb5.conf
    export KRB5_KDC_PROFILE=/yourdir/kdc.conf


krb5.conf
~~~~~~~~~

If you are not using DNS TXT records (see :ref:`mapping_hostnames`),
you must specify the **default_realm** in the :ref:`libdefaults`
section.  If you are not using DNS URI or SRV records (see
:ref:`kdc_hostnames` and :ref:`kdc_discovery`), you must include the
**kdc** tag for each *realm* in the :ref:`realms` section.  To
communicate with the kadmin server in each realm, the **admin_server**
tag must be set in the
:ref:`realms` section.

An example krb5.conf file::

    [libdefaults]
        default_realm = ATHENA.MIT.EDU

    [realms]
        ATHENA.MIT.EDU = {
            kdc = kerberos.mit.edu
            kdc = kerberos-1.mit.edu
            admin_server = kerberos.mit.edu
        }


kdc.conf
~~~~~~~~

The kdc.conf file can be used to control the listening ports of the
KDC and kadmind, as well as realm-specific defaults, the database type
and location, and logging.

An example kdc.conf file::

    [kdcdefaults]
        kdc_listen = 88
        kdc_tcp_listen = 88

    [realms]
        ATHENA.MIT.EDU = {
            kadmind_port = 749
            max_life = 12h 0m 0s
            max_renewable_life = 7d 0h 0m 0s
            master_key_type = aes256-cts
            supported_enctypes = aes256-cts:normal aes128-cts:normal
            # If the default location does not suit your setup,
            # explicitly configure the following values:
            #    database_name = /var/krb5kdc/principal
            #    key_stash_file = /var/krb5kdc/.k5.ATHENA.MIT.EDU
            #    acl_file = /var/krb5kdc/kadm5.acl
        }

    [logging]
        # By default, the KDC and kadmind will log output using
        # syslog.  You can instead send log output to files like this:
        kdc = FILE:/var/log/krb5kdc.log
        admin_server = FILE:/var/log/kadmin.log
        default = FILE:/var/log/krb5lib.log

Replace ``ATHENA.MIT.EDU`` and ``kerberos.mit.edu`` with the name of
your Kerberos realm and server respectively.

.. note::

          You have to have write permission on the target directories
          (these directories must exist) used by **database_name**,
          **key_stash_file**, and **acl_file**.


.. _create_db:

Create the KDC database
-----------------------

You will use the :ref:`kdb5_util(8)` command on the master KDC to
create the Kerberos database and the optional :ref:`stash_definition`.

.. note::

          If you choose not to install a stash file, the KDC will
          prompt you for the master key each time it starts up.  This
          means that the KDC will not be able to start automatically,
          such as after a system reboot.

:ref:`kdb5_util(8)` will prompt you for the master password for the
Kerberos database.  This password can be any string.  A good password
is one you can remember, but that no one else can guess.  Examples of
bad passwords are words that can be found in a dictionary, any common
or popular name, especially a famous person (or cartoon character),
your username in any form (e.g., forward, backward, repeated twice,
etc.), and any of the sample passwords that appear in this manual.
One example of a password which might be good if it did not appear in
this manual is "MITiys4K5!", which represents the sentence "MIT is
your source for Kerberos 5!"  (It's the first letter of each word,
substituting the numeral "4" for the word "for", and includes the
punctuation mark at the end.)

The following is an example of how to create a Kerberos database and
stash file on the master KDC, using the :ref:`kdb5_util(8)` command.
Replace ``ATHENA.MIT.EDU`` with the name of your Kerberos realm::

    shell% kdb5_util create -r ATHENA.MIT.EDU -s

    Initializing database '/usr/local/var/krb5kdc/principal' for realm 'ATHENA.MIT.EDU',
    master key name 'K/M@ATHENA.MIT.EDU'
    You will be prompted for the database Master Password.
    It is important that you NOT FORGET this password.
    Enter KDC database master key:  <= Type the master password.
    Re-enter KDC database master key to verify:  <= Type it again.
    shell%

This will create five files in |kdcdir| (or at the locations specified
in :ref:`kdc.conf(5)`):

* two Kerberos database files, ``principal``, and ``principal.ok``
* the Kerberos administrative database file, ``principal.kadm5``
* the administrative database lock file, ``principal.kadm5.lock``
* the stash file, in this example ``.k5.ATHENA.MIT.EDU``.  If you do
  not want a stash file, run the above command without the **-s**
  option.

For more information on administrating Kerberos database see
:ref:`db_operations`.


.. _admin_acl:

Add administrators to the ACL file
----------------------------------

Next, you need create an Access Control List (ACL) file and put the
Kerberos principal of at least one of the administrators into it.
This file is used by the :ref:`kadmind(8)` daemon to control which
principals may view and make privileged modifications to the Kerberos
database files.  The ACL filename is determined by the **acl_file**
variable in :ref:`kdc.conf(5)`; the default is |kdcdir|\
``/kadm5.acl``.

For more information on Kerberos ACL file see :ref:`kadm5.acl(5)`.

.. _addadmin_kdb:

Add administrators to the Kerberos database
-------------------------------------------

Next you need to add administrative principals (i.e., principals who
are allowed to administer Kerberos database) to the Kerberos database.
You *must* add at least one principal now to allow communication
between the Kerberos administration daemon kadmind and the kadmin
program over the network for further administration.  To do this, use
the kadmin.local utility on the master KDC.  kadmin.local is designed
to be run on the master KDC host without using Kerberos authentication
to an admin server; instead, it must have read and write access to the
Kerberos database on the local filesystem.

The administrative principals you create should be the ones you added
to the ACL file (see :ref:`admin_acl`).

In the following example, the administrative principal ``admin/admin``
is created::

    shell% kadmin.local

    kadmin.local: addprinc admin/admin@ATHENA.MIT.EDU

    WARNING: no policy specified for "admin/admin@ATHENA.MIT.EDU";
    assigning "default".
    Enter password for principal admin/admin@ATHENA.MIT.EDU:  <= Enter a password.
    Re-enter password for principal admin/admin@ATHENA.MIT.EDU:  <= Type it again.
    Principal "admin/admin@ATHENA.MIT.EDU" created.
    kadmin.local:

.. _start_kdc_daemons:

Start the Kerberos daemons on the master KDC
--------------------------------------------

At this point, you are ready to start the Kerberos KDC
(:ref:`krb5kdc(8)`) and administrative daemons on the Master KDC.  To
do so, type::

    shell% krb5kdc
    shell% kadmind

Each server daemon will fork and run in the background.

.. note::

          Assuming you want these daemons to start up automatically at
          boot time, you can add them to the KDC's ``/etc/rc`` or
          ``/etc/inittab`` file.  You need to have a
          :ref:`stash_definition` in order to do this.

You can verify that they started properly by checking for their
startup messages in the logging locations you defined in
:ref:`krb5.conf(5)` (see :ref:`logging`).  For example::

    shell% tail /var/log/krb5kdc.log
    Dec 02 12:35:47 beeblebrox krb5kdc[3187](info): commencing operation
    shell% tail /var/log/kadmin.log
    Dec 02 12:35:52 beeblebrox kadmind[3189](info): starting

Any errors the daemons encounter while starting will also be listed in
the logging output.

As an additional verification, check if :ref:`kinit(1)` succeeds
against the principals that you have created on the previous step
(:ref:`addadmin_kdb`).  Run::

    shell% kinit admin/admin@ATHENA.MIT.EDU


Install the slave KDCs
----------------------

You are now ready to start configuring the slave KDCs.

.. note::

          Assuming you are setting the KDCs up so that you can easily
          switch the master KDC with one of the slaves, you should
          perform each of these steps on the master KDC as well as the
          slave KDCs, unless these instructions specify otherwise.


.. _slave_host_key:

Create host keytabs for slave KDCs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Each KDC needs a ``host`` key in the Kerberos database.  These keys
are used for mutual authentication when propagating the database dump
file from the master KDC to the secondary KDC servers.

On the master KDC, connect to administrative interface and create the
host principal for each of the KDCs' ``host`` services.  For example,
if the master KDC were called ``kerberos.mit.edu``, and you had a
slave KDC named ``kerberos-1.mit.edu``, you would type the following::

    shell% kadmin
    kadmin: addprinc -randkey host/kerberos.mit.edu
    NOTICE: no policy specified for "host/kerberos.mit.edu@ATHENA.MIT.EDU"; assigning "default"
    Principal "host/kerberos.mit.edu@ATHENA.MIT.EDU" created.

    kadmin: addprinc -randkey host/kerberos-1.mit.edu
    NOTICE: no policy specified for "host/kerberos-1.mit.edu@ATHENA.MIT.EDU"; assigning "default"
    Principal "host/kerberos-1.mit.edu@ATHENA.MIT.EDU" created.

It is not strictly necessary to have the master KDC server in the
Kerberos database, but it can be handy if you want to be able to swap
the master KDC with one of the slaves.

Next, extract ``host`` random keys for all participating KDCs and
store them in each host's default keytab file.  Ideally, you should
extract each keytab locally on its own KDC.  If this is not feasible,
you should use an encrypted session to send them across the network.
To extract a keytab directly on a slave KDC called
``kerberos-1.mit.edu``, you would execute the following command::

    kadmin: ktadd host/kerberos-1.mit.edu
    Entry for principal host/kerberos-1.mit.edu with kvno 2, encryption
        type aes256-cts-hmac-sha1-96 added to keytab FILE:/etc/krb5.keytab.
    Entry for principal host/kerberos-1.mit.edu with kvno 2, encryption
        type aes128-cts-hmac-sha1-96 added to keytab FILE:/etc/krb5.keytab.
    Entry for principal host/kerberos-1.mit.edu with kvno 2, encryption
        type des3-cbc-sha1 added to keytab FILE:/etc/krb5.keytab.
    Entry for principal host/kerberos-1.mit.edu with kvno 2, encryption
        type arcfour-hmac added to keytab FILE:/etc/krb5.keytab.

If you are instead extracting a keytab for the slave KDC called
``kerberos-1.mit.edu`` on the master KDC, you should use a dedicated
temporary keytab file for that machine's keytab::

    kadmin: ktadd -k /tmp/kerberos-1.keytab host/kerberos-1.mit.edu
    Entry for principal host/kerberos-1.mit.edu with kvno 2, encryption
        type aes256-cts-hmac-sha1-96 added to keytab FILE:/etc/krb5.keytab.
    Entry for principal host/kerberos-1.mit.edu with kvno 2, encryption
        type aes128-cts-hmac-sha1-96 added to keytab FILE:/etc/krb5.keytab.

The file ``/tmp/kerberos-1.keytab`` can then be installed as
``/etc/krb5.keytab`` on the host ``kerberos-1.mit.edu``.


Configure slave KDCs
~~~~~~~~~~~~~~~~~~~~

Database propagation copies the contents of the master's database, but
does not propagate configuration files, stash files, or the kadm5 ACL
file.  The following files must be copied by hand to each slave (see
:ref:`mitK5defaults` for the default locations for these files):

* krb5.conf
* kdc.conf
* kadm5.acl
* master key stash file

Move the copied files into their appropriate directories, exactly as
on the master KDC.  kadm5.acl is only needed to allow a slave to swap
with the master KDC.

The database is propagated from the master KDC to the slave KDCs via
the :ref:`kpropd(8)` daemon.  You must explicitly specify the
principals which are allowed to provide Kerberos dump updates on the
slave machine with a new database.  Create a file named kpropd.acl in
the KDC state directory containing the ``host`` principals for each of
the KDCs::

    host/kerberos.mit.edu@ATHENA.MIT.EDU
    host/kerberos-1.mit.edu@ATHENA.MIT.EDU

.. note::

          If you expect that the master and slave KDCs will be
          switched at some point of time, list the host principals
          from all participating KDC servers in kpropd.acl files on
          all of the KDCs.  Otherwise, you only need to list the
          master KDC's host principal in the kpropd.acl files of the
          slave KDCs.

Then, add the following line to ``/etc/inetd.conf`` on each KDC
(adjust the path to kpropd)::

    krb5_prop stream tcp nowait root /usr/local/sbin/kpropd kpropd

You also need to add the following line to ``/etc/services`` on each
KDC, if it is not already present (assuming that the default port is
used)::

    krb5_prop       754/tcp               # Kerberos slave propagation

Restart inetd daemon.

Alternatively, start :ref:`kpropd(8)` as a stand-alone daemon.  This is
required when incremental propagation is enabled.

Now that the slave KDC is able to accept database propagation, you’ll
need to propagate the database from the master server.

NOTE: Do not start the slave KDC yet; you still do not have a copy of
the master's database.


.. _kprop_to_slaves:

Propagate the database to each slave KDC
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

First, create a dump file of the database on the master KDC, as
follows::

    shell% kdb5_util dump /usr/local/var/krb5kdc/slave_datatrans

Then, manually propagate the database to each slave KDC, as in the
following example::

    shell% kprop -f /usr/local/var/krb5kdc/slave_datatrans kerberos-1.mit.edu

    Database propagation to kerberos-1.mit.edu: SUCCEEDED

You will need a script to dump and propagate the database. The
following is an example of a Bourne shell script that will do this.

.. note::

          Remember that you need to replace ``/usr/local/var/krb5kdc``
          with the name of the KDC state directory.

::

    #!/bin/sh

    kdclist = "kerberos-1.mit.edu kerberos-2.mit.edu"

    kdb5_util dump /usr/local/var/krb5kdc/slave_datatrans

    for kdc in $kdclist
    do
        kprop -f /usr/local/var/krb5kdc/slave_datatrans $kdc
    done

You will need to set up a cron job to run this script at the intervals
you decided on earlier (see :ref:`db_prop`).

Now that the slave KDC has a copy of the Kerberos database, you can
start the krb5kdc daemon::

    shell% krb5kdc

As with the master KDC, you will probably want to add this command to
the KDCs' ``/etc/rc`` or ``/etc/inittab`` files, so they will start
the krb5kdc daemon automatically at boot time.


Propagation failed?
###################

You may encounter the following error messages. For a more detailed
discussion on possible causes and solutions click on the error link
to be redirected to :ref:`troubleshoot` section.

.. include:: ./troubleshoot.rst
   :start-after:  _prop_failed_start:
   :end-before: _prop_failed_end:


Add Kerberos principals to the database
---------------------------------------

Once your KDCs are set up and running, you are ready to use
:ref:`kadmin(1)` to load principals for your users, hosts, and other
services into the Kerberos database.  This procedure is described
fully in :ref:`add_mod_del_princs`.

You may occasionally want to use one of your slave KDCs as the master.
This might happen if you are upgrading the master KDC, or if your
master KDC has a disk crash.  See the following section for the
instructions.


.. _switch_master_slave:

Switching master and slave KDCs
-------------------------------

You may occasionally want to use one of your slave KDCs as the master.
This might happen if you are upgrading the master KDC, or if your
master KDC has a disk crash.

Assuming you have configured all of your KDCs to be able to function
as either the master KDC or a slave KDC (as this document recommends),
all you need to do to make the changeover is:

If the master KDC is still running, do the following on the *old*
master KDC:

#. Kill the kadmind process.
#. Disable the cron job that propagates the database.
#. Run your database propagation script manually, to ensure that the
   slaves all have the latest copy of the database (see
   :ref:`kprop_to_slaves`).

On the *new* master KDC:

#. Start the :ref:`kadmind(8)` daemon (see :ref:`start_kdc_daemons`).
#. Set up the cron job to propagate the database (see
   :ref:`kprop_to_slaves`).
#. Switch the CNAMEs of the old and new master KDCs.  If you can't do
   this, you'll need to change the :ref:`krb5.conf(5)` file on every
   client machine in your Kerberos realm.


Incremental database propagation
--------------------------------

If you expect your Kerberos database to become large, you may wish to
set up incremental propagation to slave KDCs.  See :ref:`incr_db_prop`
for details.
