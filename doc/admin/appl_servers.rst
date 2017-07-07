Application servers
===================

If you need to install the Kerberos V5 programs on an application
server, please refer to the Kerberos V5 Installation Guide.  Once you
have installed the software, you need to add that host to the Kerberos
database (see :ref:`add_mod_del_princs`), and generate a keytab for
that host, that contains the host's key.  You also need to make sure
the host's clock is within your maximum clock skew of the KDCs.


Keytabs
-------

A keytab is a host's copy of its own keylist, which is analogous to a
user's password.  An application server that needs to authenticate
itself to the KDC has to have a keytab that contains its own principal
and key.  Just as it is important for users to protect their
passwords, it is equally important for hosts to protect their keytabs.
You should always store keytab files on local disk, and make them
readable only by root, and you should never send a keytab file over a
network in the clear.  Ideally, you should run the :ref:`kadmin(1)`
command to extract a keytab on the host on which the keytab is to
reside.


.. _add_princ_kt:

Adding principals to keytabs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To generate a keytab, or to add a principal to an existing keytab, use
the **ktadd** command from kadmin.

.. include:: admin_commands/kadmin_local.rst
   :start-after:  _ktadd:
   :end-before: _ktadd_end:


Examples
########

Here is a sample session, using configuration files that enable only
AES encryption::

    kadmin: ktadd host/daffodil.mit.edu@ATHENA.MIT.EDU
    Entry for principal host/daffodil.mit.edu with kvno 2, encryption type aes256-cts-hmac-sha1-96 added to keytab FILE:/etc/krb5.keytab
    Entry for principal host/daffodil.mit.edu with kvno 2, encryption type aes128-cts-hmac-sha1-96 added to keytab FILE:/etc/krb5.keytab
    kadmin:


Removing principals from keytabs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To remove a principal from an existing keytab, use the kadmin
**ktremove** command.

.. include:: admin_commands/kadmin_local.rst
   :start-after:  _ktremove:
   :end-before: _ktremove_end:


Clock Skew
----------

A Kerberos application server host must keep its clock synchronized or
it will reject authentication requests from clients.  Modern operating
systems typically provide a facility to maintain the correct time;
make sure it is enabled.  This is especially important on virtual
machines, where clocks tend to drift more rapidly than normal machine
clocks.

The default allowable clock skew is controlled by the **clockskew**
variable in :ref:`libdefaults`.


Getting DNS information correct
-------------------------------

Several aspects of Kerberos rely on name service.  When a hostname is
used to name a service, the Kerberos library canonicalizes the
hostname using forward and reverse name resolution.  (The reverse name
resolution step can be turned off using the **rdns** variable in
:ref:`libdefaults`.)  The result of this canonicalization must match
the principal entry in the host's keytab, or authentication will fail.

Each host's canonical name must be the fully-qualified host name
(including the domain), and each host's IP address must
reverse-resolve to the canonical name.

Configuration of hostnames varies by operating system.  On the
application server itself, canonicalization will typically use the
``/etc/hosts`` file rather than the DNS.  Ensure that the line for the
server's hostname is in the following form::

    IP address      fully-qualified hostname        aliases

Here is a sample ``/etc/hosts`` file::

    # this is a comment
    127.0.0.1      localhost localhost.mit.edu
    10.0.0.6       daffodil.mit.edu daffodil trillium wake-robin

The output of ``klist -k`` for this example host should look like::

    viola# klist -k
    Keytab name: /etc/krb5.keytab
    KVNO Principal
    ---- ------------------------------------------------------------
       2 host/daffodil.mit.edu@ATHENA.MIT.EDU

If you were to ssh to this host with a fresh credentials cache (ticket
file), and then :ref:`klist(1)`, the output should list a service
principal of ``host/daffodil.mit.edu@ATHENA.MIT.EDU``.


.. _conf_firewall:

Configuring your firewall to work with Kerberos V5
--------------------------------------------------

If you need off-site users to be able to get Kerberos tickets in your
realm, they must be able to get to your KDC.  This requires either
that you have a slave KDC outside your firewall, or that you configure
your firewall to allow UDP requests into at least one of your KDCs, on
whichever port the KDC is running.  (The default is port 88; other
ports may be specified in the KDC's :ref:`kdc.conf(5)` file.)
Similarly, if you need off-site users to be able to change their
passwords in your realm, they must be able to get to your Kerberos
admin server on the kpasswd port (which defaults to 464).  If you need
off-site users to be able to administer your Kerberos realm, they must
be able to get to your Kerberos admin server on the administrative
port (which defaults to 749).

If your on-site users inside your firewall will need to get to KDCs in
other realms, you will also need to configure your firewall to allow
outgoing TCP and UDP requests to port 88, and to port 464 to allow
password changes.  If your on-site users inside your firewall will
need to get to Kerberos admin servers in other realms, you will also
need to allow outgoing TCP and UDP requests to port 749.

If any of your KDCs are outside your firewall, you will need to allow
kprop requests to get through to the remote KDC.  :ref:`kprop(8)` uses
the ``krb5_prop`` service on port 754 (tcp).

The book *UNIX System Security*, by David Curry, is a good starting
point for learning to configure firewalls.
