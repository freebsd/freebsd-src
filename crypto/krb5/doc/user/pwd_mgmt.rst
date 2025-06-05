Password management
===================

Your password is the only way Kerberos has of verifying your identity.
If someone finds out your password, that person can masquerade as
you---send email that comes from you, read, edit, or delete your files,
or log into other hosts as you---and no one will be able to tell the
difference.  For this reason, it is important that you choose a good
password, and keep it secret.  If you need to give access to your
account to someone else, you can do so through Kerberos (see
:ref:`grant_access`).  You should never tell your password to anyone,
including your system administrator, for any reason.  You should
change your password frequently, particularly any time you think
someone may have found out what it is.


Changing your password
----------------------

To change your Kerberos password, use the :ref:`kpasswd(1)` command.
It will ask you for your old password (to prevent someone else from
walking up to your computer when you're not there and changing your
password), and then prompt you for the new one twice.  (The reason you
have to type it twice is to make sure you have typed it correctly.)
For example, user ``david`` would do the following::

    shell% kpasswd
    Password for david:    <- Type your old password.
    Enter new password:    <- Type your new password.
    Enter it again:  <- Type the new password again.
    Password changed.
    shell%

If ``david`` typed the incorrect old password, he would get the
following message::

    shell% kpasswd
    Password for david:  <- Type the incorrect old password.
    kpasswd: Password incorrect while getting initial ticket
    shell%

If you make a mistake and don't type the new password the same way
twice, kpasswd will ask you to try again::

    shell% kpasswd
    Password for david:  <- Type the old password.
    Enter new password:  <- Type the new password.
    Enter it again: <- Type a different new password.
    kpasswd: Password mismatch while reading password
    shell%

Once you change your password, it takes some time for the change to
propagate through the system.  Depending on how your system is set up,
this might be anywhere from a few minutes to an hour or more.  If you
need to get new Kerberos tickets shortly after changing your password,
try the new password.  If the new password doesn't work, try again
using the old one.


.. _grant_access:

Granting access to your account
-------------------------------

If you need to give someone access to log into your account, you can
do so through Kerberos, without telling the person your password.
Simply create a file called :ref:`.k5login(5)` in your home directory.
This file should contain the Kerberos principal of each person to whom
you wish to give access.  Each principal must be on a separate line.
Here is a sample .k5login file::

    jennifer@ATHENA.MIT.EDU
    david@EXAMPLE.COM

This file would allow the users ``jennifer`` and ``david`` to use your
user ID, provided that they had Kerberos tickets in their respective
realms.  If you will be logging into other hosts across a network, you
will want to include your own Kerberos principal in your .k5login file
on each of these hosts.

Using a .k5login file is much safer than giving out your password,
because:

* You can take access away any time simply by removing the principal
  from your .k5login file.

* Although the user has full access to your account on one particular
  host (or set of hosts if your .k5login file is shared, e.g., over
  NFS), that user does not inherit your network privileges.

* Kerberos keeps a log of who obtains tickets, so a system
  administrator could find out, if necessary, who was capable of using
  your user ID at a particular time.

One common application is to have a .k5login file in root's home
directory, giving root access to that machine to the Kerberos
principals listed.  This allows system administrators to allow users
to become root locally, or to log in remotely as root, without their
having to give out the root password, and without anyone having to
type the root password over the network.


Password quality verification
-----------------------------

TODO
