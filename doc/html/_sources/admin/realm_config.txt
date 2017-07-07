Realm configuration decisions
=============================

Before installing Kerberos V5, it is necessary to consider the
following issues:

* The name of your Kerberos realm (or the name of each realm, if you
  need more than one).
* How you will assign your hostnames to Kerberos realms.
* Which ports your KDC and and kadmind services will use, if they will
  not be using the default ports.
* How many slave KDCs you need and where they should be located.
* The hostnames of your master and slave KDCs.
* How frequently you will propagate the database from the master KDC
  to the slave KDCs.


Realm name
----------

Although your Kerberos realm can be any ASCII string, convention is to
make it the same as your domain name, in upper-case letters.

For example, hosts in the domain ``example.com`` would be in the
Kerberos realm::

    EXAMPLE.COM

If you need multiple Kerberos realms, MIT recommends that you use
descriptive names which end with your domain name, such as::

    BOSTON.EXAMPLE.COM
    HOUSTON.EXAMPLE.COM


.. _mapping_hostnames:

Mapping hostnames onto Kerberos realms
--------------------------------------

Mapping hostnames onto Kerberos realms is done in one of three ways.

The first mechanism works through a set of rules in the
:ref:`domain_realm` section of :ref:`krb5.conf(5)`.  You can specify
mappings for an entire domain or on a per-hostname basis.  Typically
you would do this by specifying the mappings for a given domain or
subdomain and listing the exceptions.

The second mechanism is to use KDC host-based service referrals.  With
this method, the KDC's krb5.conf has a full [domain_realm] mapping for
hosts, but the clients do not, or have mappings for only a subset of
the hosts they might contact.  When a client needs to contact a server
host for which it has no mapping, it will ask the client realm's KDC
for the service ticket, and will receive a referral to the appropriate
service realm.

To use referrals, clients must be running MIT krb5 1.6 or later, and
the KDC must be running MIT krb5 1.7 or later.  The
**host_based_services** and **no_host_referral** variables in the
:ref:`kdc_realms` section of :ref:`kdc.conf(5)` can be used to
fine-tune referral behavior on the KDC.

It is also possible for clients to use DNS TXT records, if
**dns_lookup_realm** is enabled in :ref:`krb5.conf(5)`.  Such lookups
are disabled by default because DNS is an insecure protocol and security
holes could result if DNS records are spoofed.  If enabled, the client
will try to look up a TXT record formed by prepending the prefix
``_kerberos`` to the hostname in question.  If that record is not
found, the client will attempt a lookup by prepending ``_kerberos`` to the
host's domain name, then its parent domain, up to the top-level domain.
For the hostname ``boston.engineering.example.com``, the names looked up
would be::

    _kerberos.boston.engineering.example.com
    _kerberos.engineering.example.com
    _kerberos.example.com
    _kerberos.com

The value of the first TXT record found is taken as the realm name.

Even if you do not choose to use this mechanism within your site,
you may wish to set it up anyway, for use when interacting with other sites.


Ports for the KDC and admin services
------------------------------------

The default ports used by Kerberos are port 88 for the KDC and port
749 for the admin server.  You can, however, choose to run on other
ports, as long as they are specified in each host's
:ref:`krb5.conf(5)` files or in DNS SRV records, and the
:ref:`kdc.conf(5)` file on each KDC.  For a more thorough treatment of
port numbers used by the Kerberos V5 programs, refer to the
:ref:`conf_firewall`.


Slave KDCs
----------

Slave KDCs provide an additional source of Kerberos ticket-granting
services in the event of inaccessibility of the master KDC.  The
number of slave KDCs you need and the decision of where to place them,
both physically and logically, depends on the specifics of your
network.

Kerberos authentication requires that each client be able to contact a
KDC.  Therefore, you need to anticipate any likely reason a KDC might
be unavailable and have a slave KDC to take up the slack.

Some considerations include:

* Have at least one slave KDC as a backup, for when the master KDC is
  down, is being upgraded, or is otherwise unavailable.
* If your network is split such that a network outage is likely to
  cause a network partition (some segment or segments of the network
  to become cut off or isolated from other segments), have a slave KDC
  accessible to each segment.
* If possible, have at least one slave KDC in a different building
  from the master, in case of power outages, fires, or other localized
  disasters.


.. _kdc_hostnames:

Hostnames for KDCs
------------------

MIT recommends that your KDCs have a predefined set of CNAME records
(DNS hostname aliases), such as ``kerberos`` for the master KDC and
``kerberos-1``, ``kerberos-2``, ... for the slave KDCs.  This way, if
you need to swap a machine, you only need to change a DNS entry,
rather than having to change hostnames.

As of MIT krb5 1.4, clients can locate a realm's KDCs through DNS
using SRV records (:rfc:`2782`), assuming the Kerberos realm name is
also a DNS domain name.  These records indicate the hostname and port
number to contact for that service, optionally with weighting and
prioritization.  The domain name used in the SRV record name is the
realm name.  Several different Kerberos-related service names are
used:

_kerberos._udp
    This is for contacting any KDC by UDP.  This entry will be used
    the most often.  Normally you should list port 88 on each of your
    KDCs.
_kerberos._tcp
    This is for contacting any KDC by TCP.  The MIT KDC by default
    will not listen on any TCP ports, so unless you've changed the
    configuration or you're running another KDC implementation, you
    should leave this unspecified.  If you do enable TCP support,
    normally you should use port 88.
_kerberos-master._udp
    This entry should refer to those KDCs, if any, that will
    immediately see password changes to the Kerberos database.  If a
    user is logging in and the password appears to be incorrect, the
    client will retry with the master KDC before failing with an
    "incorrect password" error given.

    If you have only one KDC, or for whatever reason there is no
    accessible KDC that would get database changes faster than the
    others, you do not need to define this entry.
_kerberos-adm._tcp
    This should list port 749 on your master KDC.  Support for it is
    not complete at this time, but it will eventually be used by the
    :ref:`kadmin(1)` program and related utilities.  For now, you will
    also need the **admin_server** variable in :ref:`krb5.conf(5)`.
_kpasswd._udp
    This should list port 464 on your master KDC.  It is used when a
    user changes her password.  If this entry is not defined but a
    _kerberos-adm._tcp entry is defined, the client will use the
    _kerberos-adm._tcp entry with the port number changed to 749.

The DNS SRV specification requires that the hostnames listed be the
canonical names, not aliases.  So, for example, you might include the
following records in your (BIND-style) zone file::

    $ORIGIN foobar.com.
    _kerberos               TXT       "FOOBAR.COM"
    kerberos                CNAME     daisy
    kerberos-1              CNAME     use-the-force-luke
    kerberos-2              CNAME     bunny-rabbit
    _kerberos._udp          SRV       0 0 88 daisy
                            SRV       0 0 88 use-the-force-luke
                            SRV       0 0 88 bunny-rabbit
    _kerberos-master._udp   SRV       0 0 88 daisy
    _kerberos-adm._tcp      SRV       0 0 749 daisy
    _kpasswd._udp           SRV       0 0 464 daisy

Clients can also be configured with the explicit location of services
using the **kdc**, **master_kdc**, **admin_server**, and
**kpasswd_server** variables in the :ref:`realms` section of
:ref:`krb5.conf(5)`.  Even if some clients will be configured with
explicit server locations, providing SRV records will still benefit
unconfigured clients, and be useful for other sites.


.. _kdc_discovery:

KDC Discovery
-------------

As of MIT krb5 1.15, clients can also locate KDCs in DNS through URI
records (:rfc:`7553`).  Limitations with the SRV record format may
result in extra DNS queries in situations where a client must failover
to other transport types, or find a master server.  The URI record can
convey more information about a realm's KDCs with a single query.

The client performs a query for the following URI records:

* ``_kerberos.REALM`` for fiding KDCs.
* ``_kerberos-adm.REALM`` for finding kadmin services.
* ``_kpasswd.REALM`` for finding password services.

The URI record includes a priority, weight, and a URI string that
consists of case-insensitive colon separated fields, in the form
``scheme:[flags]:transport:residual``.

* *scheme* defines the registered URI type.  It should always be
  ``krb5srv``.
* *flags* contains zero or more flag characters.  Currently the only
  valid flag is ``m``, which indicates that the record is for a master
  server.
* *transport* defines the transport type of the residual URL or
  address.  Accepted values are ``tcp``, ``udp``, or ``kkdcp`` for the
  MS-KKDCP type.
* *residual* contains the hostname, IP address, or URL to be
  contacted using the specified transport, with an optional port
  extension.  The MS-KKDCP transport type uses a HTTPS URL, and can
  include a port and/or path extension.

An example of URI records in a zone file::

  _kerberos.EXAMPLE.COM  URI  10 1 krb5srv:m:tcp:kdc1.example.com
                         URI  20 1 krb5srv:m:udp:kdc2.example.com:89
                         URI  40 1 krb5srv::udp:10.10.0.23
                         URI  30 1 krb5srv::kkdcp:https://proxy:89/auth

URI lookups are enabled by default, and can be disabled by setting
**dns_uri_lookup** in the :ref:`libdefaults` section of
:ref:`krb5.conf(5)` to False.  When enabled, URI lookups take
precedence over SRV lookups, falling back to SRV lookups if no URI
records are found.


.. _db_prop:

Database propagation
--------------------

The Kerberos database resides on the master KDC, and must be
propagated regularly (usually by a cron job) to the slave KDCs.  In
deciding how frequently the propagation should happen, you will need
to balance the amount of time the propagation takes against the
maximum reasonable amount of time a user should have to wait for a
password change to take effect.

If the propagation time is longer than this maximum reasonable time
(e.g., you have a particularly large database, you have a lot of
slaves, or you experience frequent network delays), you may wish to
cut down on your propagation delay by performing the propagation in
parallel.  To do this, have the master KDC propagate the database to
one set of slaves, and then have each of these slaves propagate the
database to additional slaves.

See also :ref:`incr_db_prop`
