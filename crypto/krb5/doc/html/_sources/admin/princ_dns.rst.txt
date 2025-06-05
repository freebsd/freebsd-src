Principal names and DNS
=======================

Kerberos clients can do DNS lookups to canonicalize service principal
names.  This can cause difficulties when setting up Kerberos
application servers, especially when the client's name for the service
is different from what the service thinks its name is.


Service principal names
-----------------------

A frequently used kind of principal name is the host-based service
principal name.  This kind of principal name has two components: a
service name and a hostname.  For example, ``imap/imap.example.com``
is the principal name of the "imap" service on the host
"imap.example.com".  Other possible service names for the first
component include "host" (remote login services such as ssh), "HTTP",
and "nfs" (Network File System).

Service administrators often publish well-known hostname aliases that
they would prefer users to use instead of the canonical name of the
service host.  This gives service administrators more flexibility in
deploying services.  For example, a shell login server might be named
"long-vanity-hostname.example.com", but users will naturally prefer to
type something like "login.example.com".  Hostname aliases also allow
for administrators to set up load balancing for some sorts of services
based on rotating ``CNAME`` records in DNS.


Service principal canonicalization
----------------------------------

In the MIT krb5 client library, canonicalization of host-based service
principals is controlled by the **dns_canonicalize_hostname**,
**rnds**, and **qualify_shortname** variables in :ref:`libdefaults`.

If **dns_canonicalize_hostname** is set to ``true`` (the default
value), the client performs forward resolution by looking up the IPv4
and/or IPv6 addresses of the hostname using ``getaddrinfo()``.  This
process will typically add a domain suffix to the hostname if needed,
and follow CNAME records in the DNS.  If **rdns** is also set to
``true`` (the default), the client will then perform a reverse lookup
of the first returned Internet address using ``getnameinfo()``,
finding the name associated with the PTR record.

If **dns_canonicalize_hostname** is set to ``false``, the hostname is
not canonicalized using DNS.  If the hostname has only one component
(i.e. it contains no "." characters), the host's primary DNS search
domain will be appended, if there is one.  The **qualify_shortname**
variable can be used to override or disable this suffix.

If **dns_canonicalize_hostname** is set to ``fallback`` (added in
release 1.18), the hostname is initially treated according to the
rules for ``dns_canonicalize_hostname=false``.  If a ticket request
fails because the service principal is unknown, the hostname will be
canonicalized according to the rules for
``dns_canonicalize_hostname=true`` and the request will be retried.

In all cases, the hostname is converted to lowercase, and any trailing
dot is removed.



Reverse DNS mismatches
----------------------

Sometimes, an enterprise will have control over its forward DNS but
not its reverse DNS.  The reverse DNS is sometimes under the control
of the Internet service provider of the enterprise, and the enterprise
may not have much influence in setting up reverse DNS records for its
address space.  If there are difficulties with getting forward and
reverse DNS to match, it is best to set ``rdns = false`` on client
machines.


Overriding application behavior
-------------------------------

Applications can choose to use a default hostname component in their
service principal name when accepting authentication, which avoids
some sorts of hostname mismatches.  Because not all relevant
applications do this yet, using the :ref:`krb5.conf(5)` setting::

    [libdefaults]
        ignore_acceptor_hostname = true

will allow the Kerberos library to override the application's choice
of service principal hostname and will allow a server program to
accept incoming authentications using any key in its keytab that
matches the service name and realm name (if given).  This setting
defaults to "false" and is available in releases krb5-1.10 and later.


Provisioning keytabs
--------------------

One service principal entry that should be in the keytab is a
principal whose hostname component is the canonical hostname that
``getaddrinfo()`` reports for all known aliases for the host.  If the
reverse DNS information does not match this canonical hostname, an
additional service principal entry should be in the keytab for this
different hostname.


Specific application advice
---------------------------

Secure shell (ssh)
~~~~~~~~~~~~~~~~~~

Setting ``GSSAPIStrictAcceptorCheck = no`` in the configuration file
of modern versions of the openssh daemon will allow the daemon to try
any key in its keytab when accepting a connection, rather than looking
for the keytab entry that matches the host's own idea of its name
(typically the name that ``gethostname()`` returns).  This requires
krb5-1.10 or later.

OpenLDAP (ldapsearch, etc.)
~~~~~~~~~~~~~~~~~~~~~~~~~~~

OpenLDAP's SASL implementation performs reverse DNS lookup in order to
canonicalize service principal names, even if **rdns** is set to
``false`` in the Kerberos configuration.  To disable this behavior,
add ``SASL_NOCANON on`` to ``ldap.conf``, or set the
``LDAPSASL_NOCANON`` environment variable.
