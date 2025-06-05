Backups of secure hosts
=======================

When you back up a secure host, you should exclude the host's keytab
file from the backup.  If someone obtained a copy of the keytab from a
backup, that person could make any host masquerade as the host whose
keytab was compromised.  In many configurations, knowledge of the
host's keytab also allows root access to the host.  This could be
particularly dangerous if the compromised keytab was from one of your
KDCs.  If the machine has a disk crash and the keytab file is lost, it
is easy to generate another keytab file.  (See :ref:`add_princ_kt`.)
If you are unable to exclude particular files from backups, you should
ensure that the backups are kept as secure as the host's root
password.


Backing up the Kerberos database
--------------------------------

As with any file, it is possible that your Kerberos database could
become corrupted.  If this happens on one of the replica KDCs, you
might never notice, since the next automatic propagation of the
database would install a fresh copy.  However, if it happens to the
primary KDC, the corrupted database would be propagated to all of the
replicas during the next propagation.  For this reason, MIT recommends
that you back up your Kerberos database regularly.  Because the primary
KDC is continuously dumping the database to a file in order to
propagate it to the replica KDCs, it is a simple matter to have a cron
job periodically copy the dump file to a secure machine elsewhere on
your network.  (Of course, it is important to make the host where
these backups are stored as secure as your KDCs, and to encrypt its
transmission across your network.)  Then if your database becomes
corrupted, you can load the most recent dump onto the primary KDC.
(See :ref:`restore_from_dump`.)
