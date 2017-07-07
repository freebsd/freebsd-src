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

MIT Kerberos clients currently always do forward resolution (looking
up the IPv4 and possibly IPv6 addresses using ``getaddrinfo()``) of
the hostname part of a host-based service principal to canonicalize
the hostname.  They obtain the "canonical" name of the host when doing
so.  By default, MIT Kerberos clients will also then do reverse DNS
resolution (looking up the hostname associated with the IPv4 or IPv6
address using ``getnameinfo()``) of the hostname.  Using the
:ref:`krb5.conf(5)` setting::

    [libdefaults]
        rdns = false

will disable reverse DNS lookup on clients.  The default setting is
"true".

Operating system bugs may prevent a setting of ``rdns = false`` from
disabling reverse DNS lookup.  Some versions of GNU libc have a bug in
``getaddrinfo()`` that cause them to look up ``PTR`` records even when
not required.  MIT Kerberos releases krb5-1.10.2 and newer have a
workaround for this problem, as does the krb5-1.9.x series as of
release krb5-1.9.4.


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
