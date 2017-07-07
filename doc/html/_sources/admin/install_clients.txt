Installing and configuring UNIX client machines
===============================================

The Kerberized client programs include :ref:`kinit(1)`,
:ref:`klist(1)`, :ref:`kdestroy(1)`, and :ref:`kpasswd(1)`.  All of
these programs are in the directory |bindir|.

You can often integrate Kerberos with the login system on client
machines, typically through the use of PAM.  The details vary by
operating system, and should be covered in your operating system's
documentation.  If you do this, you will need to make sure your users
know to use their Kerberos passwords when they log in.

You will also need to educate your users to use the ticket management
programs kinit, klist, and kdestroy.  If you do not have Kerberos
password changing integrated into the native password program (again,
typically through PAM), you will need to educate users to use kpasswd
in place of its non-Kerberos counterparts passwd.


Client machine configuration files
----------------------------------

Each machine running Kerberos should have a :ref:`krb5.conf(5)` file.
At a minimum, it should define a **default_realm** setting in
:ref:`libdefaults`.  If you are not using DNS SRV records
(:ref:`kdc_hostnames`) or URI records (:ref:`kdc_discovery`), it must
also contain a :ref:`realms` section containing information for your
realm's KDCs.

Consider setting **rdns** to false in order to reduce your dependence
on precisely correct DNS information for service hostnames.  Turning
this flag off means that service hostnames will be canonicalized
through forward name resolution (which adds your domain name to
unqualified hostnames, and resolves CNAME records in DNS), but not
through reverse address lookup.  The default value of this flag is
true for historical reasons only.

If you anticipate users frequently logging into remote hosts
(e.g., using ssh) using forwardable credentials, consider setting
**forwardable** to true so that users obtain forwardable tickets by
default.  Otherwise users will need to use ``kinit -f`` to get
forwardable tickets.

Consider adjusting the **ticket_lifetime** setting to match the likely
length of sessions for your users.  For instance, if most of your
users will be logging in for an eight-hour workday, you could set the
default to ten hours so that tickets obtained in the morning expire
shortly after the end of the workday.  Users can still manually
request longer tickets when necessary, up to the maximum allowed by
each user's principal record on the KDC.

If a client host may access services in different realms, it may be
useful to define a :ref:`domain_realm` mapping so that clients know
which hosts belong to which realms.  However, if your clients and KDC
are running release 1.7 or later, it is also reasonable to leave this
section out on client machines and just define it in the KDC's
krb5.conf.
