Application servers
===================

If you need to install the Kerberos V5 programs on an application
server, please refer to the Kerberos V5 Installation Guide.  Once you
have installed the software, you need to add that host to the Kerberos
database (see :ref:`principals`), and generate a keytab for that host,
that contains the host's key.  You also need to make sure the host's
clock is within your maximum clock skew of the KDCs.


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
the **ktadd** command from kadmin.  Here is a sample session, using
configuration files that enable only AES encryption::

    kadmin: ktadd host/daffodil.mit.edu@ATHENA.MIT.EDU
    Entry for principal host/daffodil.mit.edu with kvno 2, encryption type aes256-cts-hmac-sha1-96 added to keytab FILE:/etc/krb5.keytab
    Entry for principal host/daffodil.mit.edu with kvno 2, encryption type aes128-cts-hmac-sha1-96 added to keytab FILE:/etc/krb5.keytab


Removing principals from keytabs
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To remove a principal from an existing keytab, use the kadmin
**ktremove** command::

    kadmin:  ktremove host/daffodil.mit.edu@ATHENA.MIT.EDU
    Entry for principal host/daffodil.mit.edu with kvno 2 removed from keytab FILE:/etc/krb5.keytab.
    Entry for principal host/daffodil.mit.edu with kvno 2 removed from keytab FILE:/etc/krb5.keytab.


Using a keytab to acquire client credentials
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

While keytabs are ordinarily used to accept credentials from clients,
they can also be used to acquire initial credentials, allowing one
service to authenticate to another.

To manually obtain credentials using a keytab, use the :ref:`kinit(1)`
**-k** option, together with the **-t** option if the keytab is not in
the default location.

Beginning with release 1.11, GSSAPI applications can be configured to
automatically obtain initial credentials from a keytab as needed.  The
recommended configuration is as follows:

#. Create a keytab containing a single entry for the desired client
   identity.

#. Place the keytab in a location readable by the service, and set the
   **KRB5_CLIENT_KTNAME** environment variable to its filename.
   Alternatively, use the **default_client_keytab_name** profile
   variable in :ref:`libdefaults`, or use the default location of
   |ckeytab|.

#. Set **KRB5CCNAME** to a filename writable by the service, which
   will not be used for any other purpose.  Do not manually obtain
   credentials at this location.  (Another credential cache type
   besides **FILE** can be used if desired, as long the cache will not
   conflict with another use.  A **MEMORY** cache can be used if the
   service runs as a long-lived process.  See :ref:`ccache_definition`
   for details.)

#. Start the service.  When it authenticates using GSSAPI, it will
   automatically obtain credentials from the client keytab into the
   specified credential cache, and refresh them before they expire.


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
used to name a service, clients may canonicalize the hostname using
forward and possibly reverse name resolution.  The result of this
canonicalization must match the principal entry in the host's keytab,
or authentication will fail.  To work with all client canonicalization
configurations, each host's canonical name must be the fully-qualified
host name (including the domain), and each host's IP address must
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
that you have a replica KDC outside your firewall, or that you
configure your firewall to allow UDP requests into at least one of
your KDCs, on whichever port the KDC is running.  (The default is port
88; other ports may be specified in the KDC's :ref:`kdc.conf(5)`
file.)  Similarly, if you need off-site users to be able to change
their passwords in your realm, they must be able to get to your
Kerberos admin server on the kpasswd port (which defaults to 464).  If
you need off-site users to be able to administer your Kerberos realm,
they must be able to get to your Kerberos admin server on the
administrative port (which defaults to 749).

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
