.. _sserver(8):

sserver
=======

SYNOPSIS
--------

**sserver**
[ **-p** *port* ]
[ **-S** *keytab* ]
[ *server_port* ]


DESCRIPTION
-----------

sserver and :ref:`sclient(1)` are a simple demonstration client/server
application.  When sclient connects to sserver, it performs a Kerberos
authentication, and then sserver returns to sclient the Kerberos
principal which was used for the Kerberos authentication.  It makes a
good test that Kerberos has been successfully installed on a machine.

The service name used by sserver and sclient is sample.  Hence,
sserver will require that there be a keytab entry for the service
``sample/hostname.domain.name@REALM.NAME``.  This keytab is generated
using the :ref:`kadmin(1)` program.  The keytab file is usually
installed as |keytab|.

The **-S** option allows for a different keytab than the default.

sserver is normally invoked out of inetd(8), using a line in
``/etc/inetd.conf`` that looks like this::

    sample stream tcp nowait root /usr/local/sbin/sserver sserver

Since ``sample`` is normally not a port defined in ``/etc/services``,
you will usually have to add a line to ``/etc/services`` which looks
like this::

    sample          13135/tcp

When using sclient, you will first have to have an entry in the
Kerberos database, by using :ref:`kadmin(1)`, and then you have to get
Kerberos tickets, by using :ref:`kinit(1)`.  Also, if you are running
the sclient program on a different host than the sserver it will be
connecting to, be sure that both hosts have an entry in /etc/services
for the sample tcp port, and that the same port number is in both
files.

When you run sclient you should see something like this::

    sendauth succeeded, reply is:
    reply len 32, contents:
    You are nlgilman@JIMI.MIT.EDU


COMMON ERROR MESSAGES
---------------------

1) kinit returns the error::

       kinit: Client not found in Kerberos database while getting
              initial credentials

   This means that you didn't create an entry for your username in the
   Kerberos database.

2) sclient returns the error::

       unknown service sample/tcp; check /etc/services

   This means that you don't have an entry in /etc/services for the
   sample tcp port.

3) sclient returns the error::

       connect: Connection refused

   This probably means you didn't edit /etc/inetd.conf correctly, or
   you didn't restart inetd after editing inetd.conf.

4) sclient returns the error::

       sclient: Server not found in Kerberos database while using
                sendauth

   This means that the ``sample/hostname@LOCAL.REALM`` service was not
   defined in the Kerberos database; it should be created using
   :ref:`kadmin(1)`, and a keytab file needs to be generated to make
   the key for that service principal available for sclient.

5) sclient returns the error::

       sendauth rejected, error reply is:
           "No such file or directory"

   This probably means sserver couldn't find the keytab file.  It was
   probably not installed in the proper directory.


SEE ALSO
--------

:ref:`sclient(1)`, services(5), inetd(8)
