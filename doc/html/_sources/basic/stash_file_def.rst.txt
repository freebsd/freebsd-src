.. _stash_definition:


stash file
============

The stash file is a local copy of the master key that resides in
encrypted form on the KDC's local disk.  The stash file is used to
authenticate the KDC to itself automatically before starting the
:ref:`kadmind(8)` and :ref:`krb5kdc(8)` daemons (e.g., as part of the
machine's boot sequence).  The stash file, like the keytab file (see
:ref:`keytab_file`) is a potential point-of-entry for a break-in, and
if compromised, would allow unrestricted access to the Kerberos
database.  If you choose to install a stash file, it should be
readable only by root, and should exist only on the KDC's local disk.
The file should not be part of any backup of the machine, unless
access to the backup data is secured as tightly as access to the
master password itself.

.. note::

          If you choose not to install a stash file, the KDC will prompt you for the master key each time it starts up.
          This means that the KDC will not be able to start automatically, such as after a system reboot.


