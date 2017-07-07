.. _krb5.conf(5):

krb5.conf
=========

The krb5.conf file contains Kerberos configuration information,
including the locations of KDCs and admin servers for the Kerberos
realms of interest, defaults for the current realm and for Kerberos
applications, and mappings of hostnames onto Kerberos realms.
Normally, you should install your krb5.conf file in the directory
``/etc``.  You can override the default location by setting the
environment variable **KRB5_CONFIG**.  Multiple colon-separated
filenames may be specified in **KRB5_CONFIG**; all files which are
present will be read.  Starting in release 1.14, directory names can
also be specified in **KRB5_CONFIG**; all files within the directory
whose names consist solely of alphanumeric characters, dashes, or
underscores will be read.


Structure
---------

The krb5.conf file is set up in the style of a Windows INI file.
Sections are headed by the section name, in square brackets.  Each
section may contain zero or more relations, of the form::

    foo = bar

or::

    fubar = {
        foo = bar
        baz = quux
    }

Placing a '\*' at the end of a line indicates that this is the *final*
value for the tag.  This means that neither the remainder of this
configuration file nor any other configuration file will be checked
for any other values for this tag.

For example, if you have the following lines::

    foo = bar*
    foo = baz

then the second value of ``foo`` (``baz``) would never be read.

The krb5.conf file can include other files using either of the
following directives at the beginning of a line::

    include FILENAME
    includedir DIRNAME

*FILENAME* or *DIRNAME* should be an absolute path. The named file or
directory must exist and be readable.  Including a directory includes
all files within the directory whose names consist solely of
alphanumeric characters, dashes, or underscores.  Starting in release
1.15, files with names ending in ".conf" are also included.  Included
profile files are syntactically independent of their parents, so each
included file must begin with a section header.

The krb5.conf file can specify that configuration should be obtained
from a loadable module, rather than the file itself, using the
following directive at the beginning of a line before any section
headers::

    module MODULEPATH:RESIDUAL

*MODULEPATH* may be relative to the library path of the krb5
installation, or it may be an absolute path.  *RESIDUAL* is provided
to the module at initialization time.  If krb5.conf uses a module
directive, :ref:`kdc.conf(5)` should also use one if it exists.


Sections
--------

The krb5.conf file may contain the following sections:

===================  =======================================================
:ref:`libdefaults`   Settings used by the Kerberos V5 library
:ref:`realms`        Realm-specific contact information and settings
:ref:`domain_realm`  Maps server hostnames to Kerberos realms
:ref:`capaths`       Authentication paths for non-hierarchical cross-realm
:ref:`appdefaults`   Settings used by some Kerberos V5 applications
:ref:`plugins`       Controls plugin module registration
===================  =======================================================

Additionally, krb5.conf may include any of the relations described in
:ref:`kdc.conf(5)`, but it is not a recommended practice.

.. _libdefaults:

[libdefaults]
~~~~~~~~~~~~~

The libdefaults section may contain any of the following relations:

**allow_weak_crypto**
    If this flag is set to false, then weak encryption types (as noted
    in :ref:`Encryption_types` in :ref:`kdc.conf(5)`) will be filtered
    out of the lists **default_tgs_enctypes**,
    **default_tkt_enctypes**, and **permitted_enctypes**.  The default
    value for this tag is false, which may cause authentication
    failures in existing Kerberos infrastructures that do not support
    strong crypto.  Users in affected environments should set this tag
    to true until their infrastructure adopts stronger ciphers.

**ap_req_checksum_type**
    An integer which specifies the type of AP-REQ checksum to use in
    authenticators.  This variable should be unset so the appropriate
    checksum for the encryption key in use will be used.  This can be
    set if backward compatibility requires a specific checksum type.
    See the **kdc_req_checksum_type** configuration option for the
    possible values and their meanings.

**canonicalize**
    If this flag is set to true, initial ticket requests to the KDC
    will request canonicalization of the client principal name, and
    answers with different client principals than the requested
    principal will be accepted.  The default value is false.

**ccache_type**
    This parameter determines the format of credential cache types
    created by :ref:`kinit(1)` or other programs.  The default value
    is 4, which represents the most current format.  Smaller values
    can be used for compatibility with very old implementations of
    Kerberos which interact with credential caches on the same host.

**clockskew**
    Sets the maximum allowable amount of clockskew in seconds that the
    library will tolerate before assuming that a Kerberos message is
    invalid.  The default value is 300 seconds, or five minutes.

    The clockskew setting is also used when evaluating ticket start
    and expiration times.  For example, tickets that have reached
    their expiration time can still be used (and renewed if they are
    renewable tickets) if they have been expired for a shorter
    duration than the **clockskew** setting.

**default_ccache_name**
    This relation specifies the name of the default credential cache.
    The default is |ccache|.  This relation is subject to parameter
    expansion (see below).  New in release 1.11.

**default_client_keytab_name**
    This relation specifies the name of the default keytab for
    obtaining client credentials.  The default is |ckeytab|.  This
    relation is subject to parameter expansion (see below).
    New in release 1.11.

**default_keytab_name**
    This relation specifies the default keytab name to be used by
    application servers such as sshd.  The default is |keytab|.  This
    relation is subject to parameter expansion (see below).

**default_realm**
    Identifies the default Kerberos realm for the client.  Set its
    value to your Kerberos realm.  If this value is not set, then a
    realm must be specified with every Kerberos principal when
    invoking programs such as :ref:`kinit(1)`.

**default_tgs_enctypes**
    Identifies the supported list of session key encryption types that
    the client should request when making a TGS-REQ, in order of
    preference from highest to lowest.  The list may be delimited with
    commas or whitespace.  See :ref:`Encryption_types` in
    :ref:`kdc.conf(5)` for a list of the accepted values for this tag.
    The default value is |defetypes|, but single-DES encryption types
    will be implicitly removed from this list if the value of
    **allow_weak_crypto** is false.

    Do not set this unless required for specific backward
    compatibility purposes; stale values of this setting can prevent
    clients from taking advantage of new stronger enctypes when the
    libraries are upgraded.

**default_tkt_enctypes**
    Identifies the supported list of session key encryption types that
    the client should request when making an AS-REQ, in order of
    preference from highest to lowest.  The format is the same as for
    default_tgs_enctypes.  The default value for this tag is
    |defetypes|, but single-DES encryption types will be implicitly
    removed from this list if the value of **allow_weak_crypto** is
    false.

    Do not set this unless required for specific backward
    compatibility purposes; stale values of this setting can prevent
    clients from taking advantage of new stronger enctypes when the
    libraries are upgraded.

**dns_canonicalize_hostname**
    Indicate whether name lookups will be used to canonicalize
    hostnames for use in service principal names.  Setting this flag
    to false can improve security by reducing reliance on DNS, but
    means that short hostnames will not be canonicalized to
    fully-qualified hostnames.  The default value is true.

**dns_lookup_kdc**
    Indicate whether DNS SRV records should be used to locate the KDCs
    and other servers for a realm, if they are not listed in the
    krb5.conf information for the realm.  (Note that the admin_server
    entry must be in the krb5.conf realm information in order to
    contact kadmind, because the DNS implementation for kadmin is
    incomplete.)

    Enabling this option does open up a type of denial-of-service
    attack, if someone spoofs the DNS records and redirects you to
    another server.  However, it's no worse than a denial of service,
    because that fake KDC will be unable to decode anything you send
    it (besides the initial ticket request, which has no encrypted
    data), and anything the fake KDC sends will not be trusted without
    verification using some secret that it won't know.

**dns_uri_lookup**
    Indicate whether DNS URI records should be used to locate the KDCs
    and other servers for a realm, if they are not listed in the
    krb5.conf information for the realm.  SRV records are used as a
    fallback if no URI records were found.  The default value is true.
    New in release 1.15.

**err_fmt**
    This relation allows for custom error message formatting.  If a
    value is set, error messages will be formatted by substituting a
    normal error message for %M and an error code for %C in the value.

**extra_addresses**
    This allows a computer to use multiple local addresses, in order
    to allow Kerberos to work in a network that uses NATs while still
    using address-restricted tickets.  The addresses should be in a
    comma-separated list.  This option has no effect if
    **noaddresses** is true.

**forwardable**
    If this flag is true, initial tickets will be forwardable by
    default, if allowed by the KDC.  The default value is false.

**ignore_acceptor_hostname**
    When accepting GSSAPI or krb5 security contexts for host-based
    service principals, ignore any hostname passed by the calling
    application, and allow clients to authenticate to any service
    principal in the keytab matching the service name and realm name
    (if given).  This option can improve the administrative
    flexibility of server applications on multihomed hosts, but could
    compromise the security of virtual hosting environments.  The
    default value is false.  New in release 1.10.

**k5login_authoritative**
    If this flag is true, principals must be listed in a local user's
    k5login file to be granted login access, if a :ref:`.k5login(5)`
    file exists.  If this flag is false, a principal may still be
    granted login access through other mechanisms even if a k5login
    file exists but does not list the principal.  The default value is
    true.

**k5login_directory**
    If set, the library will look for a local user's k5login file
    within the named directory, with a filename corresponding to the
    local username.  If not set, the library will look for k5login
    files in the user's home directory, with the filename .k5login.
    For security reasons, .k5login files must be owned by
    the local user or by root.

**kcm_mach_service**
    On OS X only, determines the name of the bootstrap service used to
    contact the KCM daemon for the KCM credential cache type.  If the
    value is ``-``, Mach RPC will not be used to contact the KCM
    daemon.  The default value is ``org.h5l.kcm``.

**kcm_socket**
    Determines the path to the Unix domain socket used to access the
    KCM daemon for the KCM credential cache type.  If the value is
    ``-``, Unix domain sockets will not be used to contact the KCM
    daemon.  The default value is
    ``/var/run/.heim_org.h5l.kcm-socket``.

**kdc_default_options**
    Default KDC options (Xored for multiple values) when requesting
    initial tickets.  By default it is set to 0x00000010
    (KDC_OPT_RENEWABLE_OK).

**kdc_timesync**
    Accepted values for this relation are 1 or 0.  If it is nonzero,
    client machines will compute the difference between their time and
    the time returned by the KDC in the timestamps in the tickets and
    use this value to correct for an inaccurate system clock when
    requesting service tickets or authenticating to services.  This
    corrective factor is only used by the Kerberos library; it is not
    used to change the system clock.  The default value is 1.

**kdc_req_checksum_type**
    An integer which specifies the type of checksum to use for the KDC
    requests, for compatibility with very old KDC implementations.
    This value is only used for DES keys; other keys use the preferred
    checksum type for those keys.

    The possible values and their meanings are as follows.

    ======== ===============================
    1        CRC32
    2        RSA MD4
    3        RSA MD4 DES
    4        DES CBC
    7        RSA MD5
    8        RSA MD5 DES
    9        NIST SHA
    12       HMAC SHA1 DES3
    -138     Microsoft MD5 HMAC checksum type
    ======== ===============================

**noaddresses**
    If this flag is true, requests for initial tickets will not be
    made with address restrictions set, allowing the tickets to be
    used across NATs.  The default value is true.

**permitted_enctypes**
    Identifies all encryption types that are permitted for use in
    session key encryption.  The default value for this tag is
    |defetypes|, but single-DES encryption types will be implicitly
    removed from this list if the value of **allow_weak_crypto** is
    false.

**plugin_base_dir**
    If set, determines the base directory where krb5 plugins are
    located.  The default value is the ``krb5/plugins`` subdirectory
    of the krb5 library directory.

**preferred_preauth_types**
    This allows you to set the preferred preauthentication types which
    the client will attempt before others which may be advertised by a
    KDC.  The default value for this setting is "17, 16, 15, 14",
    which forces libkrb5 to attempt to use PKINIT if it is supported.

**proxiable**
    If this flag is true, initial tickets will be proxiable by
    default, if allowed by the KDC.  The default value is false.

**rdns**
    If this flag is true, reverse name lookup will be used in addition
    to forward name lookup to canonicalizing hostnames for use in
    service principal names.  If **dns_canonicalize_hostname** is set
    to false, this flag has no effect.  The default value is true.

**realm_try_domains**
    Indicate whether a host's domain components should be used to
    determine the Kerberos realm of the host.  The value of this
    variable is an integer: -1 means not to search, 0 means to try the
    host's domain itself, 1 means to also try the domain's immediate
    parent, and so forth.  The library's usual mechanism for locating
    Kerberos realms is used to determine whether a domain is a valid
    realm, which may involve consulting DNS if **dns_lookup_kdc** is
    set.  The default is not to search domain components.

**renew_lifetime**
    (:ref:`duration` string.)  Sets the default renewable lifetime
    for initial ticket requests.  The default value is 0.

**safe_checksum_type**
    An integer which specifies the type of checksum to use for the
    KRB-SAFE requests.  By default it is set to 8 (RSA MD5 DES).  For
    compatibility with applications linked against DCE version 1.1 or
    earlier Kerberos libraries, use a value of 3 to use the RSA MD4
    DES instead.  This field is ignored when its value is incompatible
    with the session key type.  See the **kdc_req_checksum_type**
    configuration option for the possible values and their meanings.

**ticket_lifetime**
    (:ref:`duration` string.)  Sets the default lifetime for initial
    ticket requests.  The default value is 1 day.

**udp_preference_limit**
    When sending a message to the KDC, the library will try using TCP
    before UDP if the size of the message is above
    **udp_preference_limit**.  If the message is smaller than
    **udp_preference_limit**, then UDP will be tried before TCP.
    Regardless of the size, both protocols will be tried if the first
    attempt fails.

**verify_ap_req_nofail**
    If this flag is true, then an attempt to verify initial
    credentials will fail if the client machine does not have a
    keytab.  The default value is false.

.. _realms:

[realms]
~~~~~~~~

Each tag in the [realms] section of the file is the name of a Kerberos
realm.  The value of the tag is a subsection with relations that
define the properties of that particular realm.  For each realm, the
following tags may be specified in the realm's subsection:

**admin_server**
    Identifies the host where the administration server is running.
    Typically, this is the master Kerberos server.  This tag must be
    given a value in order to communicate with the :ref:`kadmind(8)`
    server for the realm.

**auth_to_local**
    This tag allows you to set a general rule for mapping principal
    names to local user names.  It will be used if there is not an
    explicit mapping for the principal name that is being
    translated. The possible values are:

    **RULE:**\ *exp*
        The local name will be formulated from *exp*.

        The format for *exp* is **[**\ *n*\ **:**\ *string*\ **](**\
        *regexp*\ **)s/**\ *pattern*\ **/**\ *replacement*\ **/g**.
        The integer *n* indicates how many components the target
        principal should have.  If this matches, then a string will be
        formed from *string*, substituting the realm of the principal
        for ``$0`` and the *n*'th component of the principal for
        ``$n`` (e.g., if the principal was ``johndoe/admin`` then
        ``[2:$2$1foo]`` would result in the string
        ``adminjohndoefoo``).  If this string matches *regexp*, then
        the ``s//[g]`` substitution command will be run over the
        string.  The optional **g** will cause the substitution to be
        global over the *string*, instead of replacing only the first
        match in the *string*.

    **DEFAULT**
        The principal name will be used as the local user name.  If
        the principal has more than one component or is not in the
        default realm, this rule is not applicable and the conversion
        will fail.

    For example::

        [realms]
            ATHENA.MIT.EDU = {
                auth_to_local = RULE:[2:$1](johndoe)s/^.*$/guest/
                auth_to_local = RULE:[2:$1;$2](^.*;admin$)s/;admin$//
                auth_to_local = RULE:[2:$2](^.*;root)s/^.*$/root/
                auto_to_local = DEFAULT
            }

    would result in any principal without ``root`` or ``admin`` as the
    second component to be translated with the default rule.  A
    principal with a second component of ``admin`` will become its
    first component.  ``root`` will be used as the local name for any
    principal with a second component of ``root``.  The exception to
    these two rules are any principals ``johndoe/*``, which will
    always get the local name ``guest``.

**auth_to_local_names**
    This subsection allows you to set explicit mappings from principal
    names to local user names.  The tag is the mapping name, and the
    value is the corresponding local user name.

**default_domain**
    This tag specifies the domain used to expand hostnames when
    translating Kerberos 4 service principals to Kerberos 5 principals
    (for example, when converting ``rcmd.hostname`` to
    ``host/hostname.domain``).

**http_anchors**
    When KDCs and kpasswd servers are accessed through HTTPS proxies, this tag
    can be used to specify the location of the CA certificate which should be
    trusted to issue the certificate for a proxy server.  If left unspecified,
    the system-wide default set of CA certificates is used.

    The syntax for values is similar to that of values for the
    **pkinit_anchors** tag:

    **FILE:** *filename*

    *filename* is assumed to be the name of an OpenSSL-style ca-bundle file.

    **DIR:** *dirname*

    *dirname* is assumed to be an directory which contains CA certificates.
    All files in the directory will be examined; if they contain certificates
    (in PEM format), they will be used.

    **ENV:** *envvar*

    *envvar* specifies the name of an environment variable which has been set
    to a value conforming to one of the previous values.  For example,
    ``ENV:X509_PROXY_CA``, where environment variable ``X509_PROXY_CA`` has
    been set to ``FILE:/tmp/my_proxy.pem``.

**kdc**
    The name or address of a host running a KDC for that realm.  An
    optional port number, separated from the hostname by a colon, may
    be included.  If the name or address contains colons (for example,
    if it is an IPv6 address), enclose it in square brackets to
    distinguish the colon from a port separator.  For your computer to
    be able to communicate with the KDC for each realm, this tag must
    be given a value in each realm subsection in the configuration
    file, or there must be DNS SRV records specifying the KDCs.

**kpasswd_server**
    Points to the server where all the password changes are performed.
    If there is no such entry, the port 464 on the **admin_server**
    host will be tried.

**master_kdc**
    Identifies the master KDC(s).  Currently, this tag is used in only
    one case: If an attempt to get credentials fails because of an
    invalid password, the client software will attempt to contact the
    master KDC, in case the user's password has just been changed, and
    the updated database has not been propagated to the slave servers
    yet.

**v4_instance_convert**
    This subsection allows the administrator to configure exceptions
    to the **default_domain** mapping rule.  It contains V4 instances
    (the tag name) which should be translated to some specific
    hostname (the tag value) as the second component in a Kerberos V5
    principal name.

**v4_realm**
    This relation is used by the krb524 library routines when
    converting a V5 principal name to a V4 principal name.  It is used
    when the V4 realm name and the V5 realm name are not the same, but
    still share the same principal names and passwords. The tag value
    is the Kerberos V4 realm name.


.. _domain_realm:

[domain_realm]
~~~~~~~~~~~~~~

The [domain_realm] section provides a translation from a domain name
or hostname to a Kerberos realm name.  The tag name can be a host name
or domain name, where domain names are indicated by a prefix of a
period (``.``).  The value of the relation is the Kerberos realm name
for that particular host or domain.  A host name relation implicitly
provides the corresponding domain name relation, unless an explicit domain
name relation is provided.  The Kerberos realm may be
identified either in the realms_ section or using DNS SRV records.
Host names and domain names should be in lower case.  For example::

    [domain_realm]
        crash.mit.edu = TEST.ATHENA.MIT.EDU
	.dev.mit.edu = TEST.ATHENA.MIT.EDU
        mit.edu = ATHENA.MIT.EDU

maps the host with the name ``crash.mit.edu`` into the
``TEST.ATHENA.MIT.EDU`` realm.  The second entry maps all hosts under the
domain ``dev.mit.edu`` into the ``TEST.ATHENA.MIT.EDU`` realm, but not
the host with the name ``dev.mit.edu``.  That host is matched
by the third entry, which maps the host ``mit.edu`` and all hosts
under the domain ``mit.edu`` that do not match a preceding rule
into the realm ``ATHENA.MIT.EDU``.

If no translation entry applies to a hostname used for a service
principal for a service ticket request, the library will try to get a
referral to the appropriate realm from the client realm's KDC.  If
that does not succeed, the host's realm is considered to be the
hostname's domain portion converted to uppercase, unless the
**realm_try_domains** setting in [libdefaults] causes a different
parent domain to be used.


.. _capaths:

[capaths]
~~~~~~~~~

In order to perform direct (non-hierarchical) cross-realm
authentication, configuration is needed to determine the
authentication paths between realms.

A client will use this section to find the authentication path between
its realm and the realm of the server.  The server will use this
section to verify the authentication path used by the client, by
checking the transited field of the received ticket.

There is a tag for each participating client realm, and each tag has
subtags for each of the server realms.  The value of the subtags is an
intermediate realm which may participate in the cross-realm
authentication.  The subtags may be repeated if there is more then one
intermediate realm.  A value of "." means that the two realms share
keys directly, and no intermediate realms should be allowed to
participate.

Only those entries which will be needed on the client or the server
need to be present.  A client needs a tag for its local realm with
subtags for all the realms of servers it will need to authenticate to.
A server needs a tag for each realm of the clients it will serve, with
a subtag of the server realm.

For example, ``ANL.GOV``, ``PNL.GOV``, and ``NERSC.GOV`` all wish to
use the ``ES.NET`` realm as an intermediate realm.  ANL has a sub
realm of ``TEST.ANL.GOV`` which will authenticate with ``NERSC.GOV``
but not ``PNL.GOV``.  The [capaths] section for ``ANL.GOV`` systems
would look like this::

    [capaths]
        ANL.GOV = {
            TEST.ANL.GOV = .
            PNL.GOV = ES.NET
            NERSC.GOV = ES.NET
            ES.NET = .
        }
        TEST.ANL.GOV = {
            ANL.GOV = .
        }
        PNL.GOV = {
            ANL.GOV = ES.NET
        }
        NERSC.GOV = {
            ANL.GOV = ES.NET
        }
        ES.NET = {
            ANL.GOV = .
        }

The [capaths] section of the configuration file used on ``NERSC.GOV``
systems would look like this::

    [capaths]
        NERSC.GOV = {
            ANL.GOV = ES.NET
            TEST.ANL.GOV = ES.NET
            TEST.ANL.GOV = ANL.GOV
            PNL.GOV = ES.NET
            ES.NET = .
        }
        ANL.GOV = {
            NERSC.GOV = ES.NET
        }
        PNL.GOV = {
            NERSC.GOV = ES.NET
        }
        ES.NET = {
            NERSC.GOV = .
        }
        TEST.ANL.GOV = {
            NERSC.GOV = ANL.GOV
            NERSC.GOV = ES.NET
        }

When a subtag is used more than once within a tag, clients will use
the order of values to determine the path.  The order of values is not
important to servers.


.. _appdefaults:

[appdefaults]
~~~~~~~~~~~~~

Each tag in the [appdefaults] section names a Kerberos V5 application
or an option that is used by some Kerberos V5 application[s].  The
value of the tag defines the default behaviors for that application.

For example::

    [appdefaults]
        telnet = {
            ATHENA.MIT.EDU = {
                option1 = false
            }
        }
        telnet = {
            option1 = true
            option2 = true
        }
        ATHENA.MIT.EDU = {
            option2 = false
        }
        option2 = true

The above four ways of specifying the value of an option are shown in
order of decreasing precedence. In this example, if telnet is running
in the realm EXAMPLE.COM, it should, by default, have option1 and
option2 set to true.  However, a telnet program in the realm
``ATHENA.MIT.EDU`` should have ``option1`` set to false and
``option2`` set to true.  Any other programs in ATHENA.MIT.EDU should
have ``option2`` set to false by default.  Any programs running in
other realms should have ``option2`` set to true.

The list of specifiable options for each application may be found in
that application's man pages.  The application defaults specified here
are overridden by those specified in the realms_ section.


.. _plugins:

[plugins]
~~~~~~~~~

    * pwqual_ interface
    * kadm5_hook_ interface
    * clpreauth_ and kdcpreauth_ interfaces

Tags in the [plugins] section can be used to register dynamic plugin
modules and to turn modules on and off.  Not every krb5 pluggable
interface uses the [plugins] section; the ones that do are documented
here.

New in release 1.9.

Each pluggable interface corresponds to a subsection of [plugins].
All subsections support the same tags:

**disable**
    This tag may have multiple values. If there are values for this
    tag, then the named modules will be disabled for the pluggable
    interface.

**enable_only**
    This tag may have multiple values. If there are values for this
    tag, then only the named modules will be enabled for the pluggable
    interface.

**module**
    This tag may have multiple values.  Each value is a string of the
    form ``modulename:pathname``, which causes the shared object
    located at *pathname* to be registered as a dynamic module named
    *modulename* for the pluggable interface.  If *pathname* is not an
    absolute path, it will be treated as relative to the
    **plugin_base_dir** value from :ref:`libdefaults`.

For pluggable interfaces where module order matters, modules
registered with a **module** tag normally come first, in the order
they are registered, followed by built-in modules in the order they
are documented below.  If **enable_only** tags are used, then the
order of those tags overrides the normal module order.

The following subsections are currently supported within the [plugins]
section:

.. _ccselect:

ccselect interface
##################

The ccselect subsection controls modules for credential cache
selection within a cache collection.  In addition to any registered
dynamic modules, the following built-in modules exist (and may be
disabled with the disable tag):

**k5identity**
    Uses a .k5identity file in the user's home directory to select a
    client principal

**realm**
    Uses the service realm to guess an appropriate cache from the
    collection

.. _pwqual:

pwqual interface
################

The pwqual subsection controls modules for the password quality
interface, which is used to reject weak passwords when passwords are
changed.  The following built-in modules exist for this interface:

**dict**
    Checks against the realm dictionary file

**empty**
    Rejects empty passwords

**hesiod**
    Checks against user information stored in Hesiod (only if Kerberos
    was built with Hesiod support)

**princ**
    Checks against components of the principal name

.. _kadm5_hook:

kadm5_hook interface
####################

The kadm5_hook interface provides plugins with information on
principal creation, modification, password changes and deletion.  This
interface can be used to write a plugin to synchronize MIT Kerberos
with another database such as Active Directory.  No plugins are built
in for this interface.

.. _clpreauth:

.. _kdcpreauth:

clpreauth and kdcpreauth interfaces
###################################

The clpreauth and kdcpreauth interfaces allow plugin modules to
provide client and KDC preauthentication mechanisms.  The following
built-in modules exist for these interfaces:

**pkinit**
    This module implements the PKINIT preauthentication mechanism.

**encrypted_challenge**
    This module implements the encrypted challenge FAST factor.

**encrypted_timestamp**
    This module implements the encrypted timestamp mechanism.

.. _hostrealm:

hostrealm interface
###################

The hostrealm section (introduced in release 1.12) controls modules
for the host-to-realm interface, which affects the local mapping of
hostnames to realm names and the choice of default realm.  The following
built-in modules exist for this interface:

**profile**
    This module consults the [domain_realm] section of the profile for
    authoritative host-to-realm mappings, and the **default_realm**
    variable for the default realm.

**dns**
    This module looks for DNS records for fallback host-to-realm
    mappings and the default realm.  It only operates if the
    **dns_lookup_realm** variable is set to true.

**domain**
    This module applies heuristics for fallback host-to-realm
    mappings.  It implements the **realm_try_domains** variable, and
    uses the uppercased parent domain of the hostname if that does not
    produce a result.

.. _localauth:

localauth interface
###################

The localauth section (introduced in release 1.12) controls modules
for the local authorization interface, which affects the relationship
between Kerberos principals and local system accounts.  The following
built-in modules exist for this interface:

**default**
    This module implements the **DEFAULT** type for **auth_to_local**
    values.

**rule**
    This module implements the **RULE** type for **auth_to_local**
    values.

**names**
    This module looks for an **auth_to_local_names** mapping for the
    principal name.

**auth_to_local**
    This module processes **auth_to_local** values in the default
    realm's section, and applies the default method if no
    **auth_to_local** values exist.

**k5login**
    This module authorizes a principal to a local account according to
    the account's :ref:`.k5login(5)` file.

**an2ln**
    This module authorizes a principal to a local account if the
    principal name maps to the local account name.


PKINIT options
--------------

.. note::

          The following are PKINIT-specific options.  These values may
          be specified in [libdefaults] as global defaults, or within
          a realm-specific subsection of [libdefaults], or may be
          specified as realm-specific values in the [realms] section.
          A realm-specific value overrides, not adds to, a generic
          [libdefaults] specification.  The search order is:

1. realm-specific subsection of [libdefaults]::

       [libdefaults]
           EXAMPLE.COM = {
               pkinit_anchors = FILE:/usr/local/example.com.crt
           }

2. realm-specific value in the [realms] section::

       [realms]
           OTHERREALM.ORG = {
               pkinit_anchors = FILE:/usr/local/otherrealm.org.crt
           }

3. generic value in the [libdefaults] section::

       [libdefaults]
           pkinit_anchors = DIR:/usr/local/generic_trusted_cas/


.. _pkinit_identity:

Specifying PKINIT identity information
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The syntax for specifying Public Key identity, trust, and revocation
information for PKINIT is as follows:

**FILE:**\ *filename*\ [**,**\ *keyfilename*]
    This option has context-specific behavior.

    In **pkinit_identity** or **pkinit_identities**, *filename*
    specifies the name of a PEM-format file containing the user's
    certificate.  If *keyfilename* is not specified, the user's
    private key is expected to be in *filename* as well.  Otherwise,
    *keyfilename* is the name of the file containing the private key.

    In **pkinit_anchors** or **pkinit_pool**, *filename* is assumed to
    be the name of an OpenSSL-style ca-bundle file.

**DIR:**\ *dirname*
    This option has context-specific behavior.

    In **pkinit_identity** or **pkinit_identities**, *dirname*
    specifies a directory with files named ``*.crt`` and ``*.key``
    where the first part of the file name is the same for matching
    pairs of certificate and private key files.  When a file with a
    name ending with ``.crt`` is found, a matching file ending with
    ``.key`` is assumed to contain the private key.  If no such file
    is found, then the certificate in the ``.crt`` is not used.

    In **pkinit_anchors** or **pkinit_pool**, *dirname* is assumed to
    be an OpenSSL-style hashed CA directory where each CA cert is
    stored in a file named ``hash-of-ca-cert.#``.  This infrastructure
    is encouraged, but all files in the directory will be examined and
    if they contain certificates (in PEM format), they will be used.

    In **pkinit_revoke**, *dirname* is assumed to be an OpenSSL-style
    hashed CA directory where each revocation list is stored in a file
    named ``hash-of-ca-cert.r#``.  This infrastructure is encouraged,
    but all files in the directory will be examined and if they
    contain a revocation list (in PEM format), they will be used.

**PKCS12:**\ *filename*
    *filename* is the name of a PKCS #12 format file, containing the
    user's certificate and private key.

**PKCS11:**\ [**module_name=**]\ *modname*\ [**:slotid=**\ *slot-id*][**:token=**\ *token-label*][**:certid=**\ *cert-id*][**:certlabel=**\ *cert-label*]
    All keyword/values are optional.  *modname* specifies the location
    of a library implementing PKCS #11.  If a value is encountered
    with no keyword, it is assumed to be the *modname*.  If no
    module-name is specified, the default is ``opensc-pkcs11.so``.
    ``slotid=`` and/or ``token=`` may be specified to force the use of
    a particular smard card reader or token if there is more than one
    available.  ``certid=`` and/or ``certlabel=`` may be specified to
    force the selection of a particular certificate on the device.
    See the **pkinit_cert_match** configuration option for more ways
    to select a particular certificate to use for PKINIT.

**ENV:**\ *envvar*
    *envvar* specifies the name of an environment variable which has
    been set to a value conforming to one of the previous values.  For
    example, ``ENV:X509_PROXY``, where environment variable
    ``X509_PROXY`` has been set to ``FILE:/tmp/my_proxy.pem``.


PKINIT krb5.conf options
~~~~~~~~~~~~~~~~~~~~~~~~

**pkinit_anchors**
    Specifies the location of trusted anchor (root) certificates which
    the client trusts to sign KDC certificates.  This option may be
    specified multiple times.  These values from the config file are
    not used if the user specifies X509_anchors on the command line.

**pkinit_cert_match**
    Specifies matching rules that the client certificate must match
    before it is used to attempt PKINIT authentication.  If a user has
    multiple certificates available (on a smart card, or via other
    media), there must be exactly one certificate chosen before
    attempting PKINIT authentication.  This option may be specified
    multiple times.  All the available certificates are checked
    against each rule in order until there is a match of exactly one
    certificate.

    The Subject and Issuer comparison strings are the :rfc:`2253`
    string representations from the certificate Subject DN and Issuer
    DN values.

    The syntax of the matching rules is:

        [*relation-operator*\ ]\ *component-rule* ...

    where:

    *relation-operator*
        can be either ``&&``, meaning all component rules must match,
        or ``||``, meaning only one component rule must match.  The
        default is ``&&``.

    *component-rule*
        can be one of the following.  Note that there is no
        punctuation or whitespace between component rules.

            | **<SUBJECT>**\ *regular-expression*
            | **<ISSUER>**\ *regular-expression*
            | **<SAN>**\ *regular-expression*
            | **<EKU>**\ *extended-key-usage-list*
	    | **<KU>**\ *key-usage-list*

        *extended-key-usage-list* is a comma-separated list of
        required Extended Key Usage values.  All values in the list
        must be present in the certificate.  Extended Key Usage values
        can be:

        * pkinit
        * msScLogin
        * clientAuth
        * emailProtection

        *key-usage-list* is a comma-separated list of required Key
        Usage values.  All values in the list must be present in the
        certificate.  Key Usage values can be:

        * digitalSignature
        * keyEncipherment

    Examples::

        pkinit_cert_match = ||<SUBJECT>.*DoE.*<SAN>.*@EXAMPLE.COM
        pkinit_cert_match = &&<EKU>msScLogin,clientAuth<ISSUER>.*DoE.*
        pkinit_cert_match = <EKU>msScLogin,clientAuth<KU>digitalSignature

**pkinit_eku_checking**
    This option specifies what Extended Key Usage value the KDC
    certificate presented to the client must contain.  (Note that if
    the KDC certificate has the pkinit SubjectAlternativeName encoded
    as the Kerberos TGS name, EKU checking is not necessary since the
    issuing CA has certified this as a KDC certificate.)  The values
    recognized in the krb5.conf file are:

    **kpKDC**
        This is the default value and specifies that the KDC must have
        the id-pkinit-KPKdc EKU as defined in :rfc:`4556`.

    **kpServerAuth**
        If **kpServerAuth** is specified, a KDC certificate with the
        id-kp-serverAuth EKU will be accepted.  This key usage value
        is used in most commercially issued server certificates.

    **none**
        If **none** is specified, then the KDC certificate will not be
        checked to verify it has an acceptable EKU.  The use of this
        option is not recommended.

**pkinit_dh_min_bits**
    Specifies the size of the Diffie-Hellman key the client will
    attempt to use.  The acceptable values are 1024, 2048, and 4096.
    The default is 2048.

**pkinit_identities**
    Specifies the location(s) to be used to find the user's X.509
    identity information.  This option may be specified multiple
    times.  Each value is attempted in order until identity
    information is found and authentication is attempted.  Note that
    these values are not used if the user specifies
    **X509_user_identity** on the command line.

**pkinit_kdc_hostname**
    The presense of this option indicates that the client is willing
    to accept a KDC certificate with a dNSName SAN (Subject
    Alternative Name) rather than requiring the id-pkinit-san as
    defined in :rfc:`4556`.  This option may be specified multiple
    times.  Its value should contain the acceptable hostname for the
    KDC (as contained in its certificate).

**pkinit_pool**
    Specifies the location of intermediate certificates which may be
    used by the client to complete the trust chain between a KDC
    certificate and a trusted anchor.  This option may be specified
    multiple times.

**pkinit_require_crl_checking**
    The default certificate verification process will always check the
    available revocation information to see if a certificate has been
    revoked.  If a match is found for the certificate in a CRL,
    verification fails.  If the certificate being verified is not
    listed in a CRL, or there is no CRL present for its issuing CA,
    and **pkinit_require_crl_checking** is false, then verification
    succeeds.

    However, if **pkinit_require_crl_checking** is true and there is
    no CRL information available for the issuing CA, then verification
    fails.

    **pkinit_require_crl_checking** should be set to true if the
    policy is such that up-to-date CRLs must be present for every CA.

**pkinit_revoke**
    Specifies the location of Certificate Revocation List (CRL)
    information to be used by the client when verifying the validity
    of the KDC certificate presented.  This option may be specified
    multiple times.


.. _parameter_expansion:

Parameter expansion
-------------------

Starting with release 1.11, several variables, such as
**default_keytab_name**, allow parameters to be expanded.
Valid parameters are:

    =================  ===================================================
    %{TEMP}            Temporary directory
    %{uid}             Unix real UID or Windows SID
    %{euid}            Unix effective user ID or Windows SID
    %{USERID}          Same as %{uid}
    %{null}            Empty string
    %{LIBDIR}          Installation library directory
    %{BINDIR}          Installation binary directory
    %{SBINDIR}         Installation admin binary directory
    %{username}        (Unix) Username of effective user ID
    %{APPDATA}         (Windows) Roaming application data for current user
    %{COMMON_APPDATA}  (Windows) Application data for all users
    %{LOCAL_APPDATA}   (Windows) Local application data for current user
    %{SYSTEM}          (Windows) Windows system folder
    %{WINDOWS}         (Windows) Windows folder
    %{USERCONFIG}      (Windows) Per-user MIT krb5 config file directory
    %{COMMONCONFIG}    (Windows) Common MIT krb5 config file directory
    =================  ===================================================

Sample krb5.conf file
---------------------

Here is an example of a generic krb5.conf file::

    [libdefaults]
        default_realm = ATHENA.MIT.EDU
        dns_lookup_kdc = true
        dns_lookup_realm = false

    [realms]
        ATHENA.MIT.EDU = {
            kdc = kerberos.mit.edu
            kdc = kerberos-1.mit.edu
            kdc = kerberos-2.mit.edu
            admin_server = kerberos.mit.edu
            master_kdc = kerberos.mit.edu
        }
        EXAMPLE.COM = {
            kdc = kerberos.example.com
            kdc = kerberos-1.example.com
            admin_server = kerberos.example.com
        }

    [domain_realm]
        mit.edu = ATHENA.MIT.EDU

    [capaths]
        ATHENA.MIT.EDU = {
               EXAMPLE.COM = .
        }
        EXAMPLE.COM = {
               ATHENA.MIT.EDU = .
        }

FILES
-----

|krb5conf|


SEE ALSO
--------

syslog(3)
