.. _kdc.conf(5):

kdc.conf
========

The kdc.conf file supplements :ref:`krb5.conf(5)` for programs which
are typically only used on a KDC, such as the :ref:`krb5kdc(8)` and
:ref:`kadmind(8)` daemons and the :ref:`kdb5_util(8)` program.
Relations documented here may also be specified in krb5.conf; for the
KDC programs mentioned, krb5.conf and kdc.conf will be merged into a
single configuration profile.

Normally, the kdc.conf file is found in the KDC state directory,
|kdcdir|.  You can override the default location by setting the
environment variable **KRB5_KDC_PROFILE**.

Please note that you need to restart the KDC daemon for any configuration
changes to take effect.

Structure
---------

The kdc.conf file is set up in the same format as the
:ref:`krb5.conf(5)` file.


Sections
--------

The kdc.conf file may contain the following sections:

==================== =================================================
:ref:`kdcdefaults`   Default values for KDC behavior
:ref:`kdc_realms`    Realm-specific database configuration and settings
:ref:`dbdefaults`    Default database settings
:ref:`dbmodules`     Per-database settings
:ref:`logging`       Controls how Kerberos daemons perform logging
==================== =================================================


.. _kdcdefaults:

[kdcdefaults]
~~~~~~~~~~~~~

Some relations in the [kdcdefaults] section specify default values for
realm variables, to be used if the [realms] subsection does not
contain a relation for the tag.  See the :ref:`kdc_realms` section for
the definitions of these relations.

* **host_based_services**
* **kdc_listen**
* **kdc_ports**
* **kdc_tcp_listen**
* **kdc_tcp_ports**
* **no_host_referral**
* **restrict_anonymous_to_tgt**

The following [kdcdefaults] variables have no per-realm equivalent:

**kdc_max_dgram_reply_size**
    Specifies the maximum packet size that can be sent over UDP.  The
    default value is 4096 bytes.

**kdc_tcp_listen_backlog**
    (Integer.)  Set the size of the listen queue length for the KDC
    daemon.  The value may be limited by OS settings.  The default
    value is 5.

**spake_preauth_kdc_challenge**
    (String.)  Specifies the group for a SPAKE optimistic challenge.
    See the **spake_preauth_groups** variable in :ref:`libdefaults`
    for possible values.  The default is not to issue an optimistic
    challenge.  (New in release 1.17.)


.. _kdc_realms:

[realms]
~~~~~~~~

Each tag in the [realms] section is the name of a Kerberos realm.  The
value of the tag is a subsection where the relations define KDC
parameters for that particular realm.  The following example shows how
to define one parameter for the ATHENA.MIT.EDU realm::

    [realms]
        ATHENA.MIT.EDU = {
            max_renewable_life = 7d 0h 0m 0s
        }

The following tags may be specified in a [realms] subsection:

**acl_file**
    (String.)  Location of the access control list file that
    :ref:`kadmind(8)` uses to determine which principals are allowed
    which permissions on the Kerberos database.  To operate without an
    ACL file, set this relation to the empty string with ``acl_file =
    ""``.  The default value is |kdcdir|\ ``/kadm5.acl``.  For more
    information on Kerberos ACL file see :ref:`kadm5.acl(5)`.

**database_module**
    (String.)  This relation indicates the name of the configuration
    section under :ref:`dbmodules` for database-specific parameters
    used by the loadable database library.  The default value is the
    realm name.  If this configuration section does not exist, default
    values will be used for all database parameters.

**database_name**
    (String, deprecated.)  This relation specifies the location of the
    Kerberos database for this realm, if the DB2 module is being used
    and the :ref:`dbmodules` configuration section does not specify a
    database name.  The default value is |kdcdir|\ ``/principal``.

**default_principal_expiration**
    (:ref:`abstime` string.)  Specifies the default expiration date of
    principals created in this realm.  The default value is 0, which
    means no expiration date.

**default_principal_flags**
    (Flag string.)  Specifies the default attributes of principals
    created in this realm.  The format for this string is a
    comma-separated list of flags, with '+' before each flag that
    should be enabled and '-' before each flag that should be
    disabled.  The **postdateable**, **forwardable**, **tgt-based**,
    **renewable**, **proxiable**, **dup-skey**, **allow-tickets**, and
    **service** flags default to enabled.

    There are a number of possible flags:

    **allow-tickets**
        Enabling this flag means that the KDC will issue tickets for
        this principal.  Disabling this flag essentially deactivates
        the principal within this realm.

    **dup-skey**
        Enabling this flag allows the KDC to issue user-to-user
        service tickets for this principal.

    **forwardable**
        Enabling this flag allows the principal to obtain forwardable
        tickets.

    **hwauth**
        If this flag is enabled, then the principal is required to
        preauthenticate using a hardware device before receiving any
        tickets.

    **no-auth-data-required**
        Enabling this flag prevents PAC or AD-SIGNEDPATH data from
        being added to service tickets for the principal.

    **ok-as-delegate**
        If this flag is enabled, it hints the client that credentials
        can and should be delegated when authenticating to the
        service.

    **ok-to-auth-as-delegate**
        Enabling this flag allows the principal to use S4USelf tickets.

    **postdateable**
        Enabling this flag allows the principal to obtain postdateable
        tickets.

    **preauth**
        If this flag is enabled on a client principal, then that
        principal is required to preauthenticate to the KDC before
        receiving any tickets.  On a service principal, enabling this
        flag means that service tickets for this principal will only
        be issued to clients with a TGT that has the preauthenticated
        bit set.

    **proxiable**
        Enabling this flag allows the principal to obtain proxy
        tickets.

    **pwchange**
        Enabling this flag forces a password change for this
        principal.

    **pwservice**
        If this flag is enabled, it marks this principal as a password
        change service.  This should only be used in special cases,
        for example, if a user's password has expired, then the user
        has to get tickets for that principal without going through
        the normal password authentication in order to be able to
        change the password.

    **renewable**
        Enabling this flag allows the principal to obtain renewable
        tickets.

    **service**
        Enabling this flag allows the the KDC to issue service tickets
        for this principal.  In release 1.17 and later, user-to-user
        service tickets are still allowed if the **dup-skey** flag is
        set.

    **tgt-based**
        Enabling this flag allows a principal to obtain tickets based
        on a ticket-granting-ticket, rather than repeating the
        authentication process that was used to obtain the TGT.

**dict_file**
    (String.)  Location of the dictionary file containing strings that
    are not allowed as passwords.  The file should contain one string
    per line, with no additional whitespace.  If none is specified or
    if there is no policy assigned to the principal, no dictionary
    checks of passwords will be performed.

**disable_pac**
    (Boolean value.)  If true, the KDC will not issue PACs for this
    realm, and S4U2Self and S4U2Proxy operations will be disabled.
    The default is false, which will permit the KDC to issue PACs.
    New in release 1.20.

**encrypted_challenge_indicator**
    (String.)  Specifies the authentication indicator value that the KDC
    asserts into tickets obtained using FAST encrypted challenge
    pre-authentication.  New in 1.16.

**host_based_services**
    (Whitespace- or comma-separated list.)  Lists services which will
    get host-based referral processing even if the server principal is
    not marked as host-based by the client.

**iprop_enable**
    (Boolean value.)  Specifies whether incremental database
    propagation is enabled.  The default value is false.

**iprop_ulogsize**
    (Integer.)  Specifies the maximum number of log entries to be
    retained for incremental propagation.  The default value is 1000.
    Prior to release 1.11, the maximum value was 2500.  New in release
    1.19.

**iprop_master_ulogsize**
    The name for **iprop_ulogsize** prior to release 1.19.  Its value is
    used as a fallback if **iprop_ulogsize** is not specified.

**iprop_replica_poll**
    (Delta time string.)  Specifies how often the replica KDC polls
    for new updates from the primary.  The default value is ``2m``
    (that is, two minutes).  New in release 1.17.

**iprop_slave_poll**
    (Delta time string.)  The name for **iprop_replica_poll** prior to
    release 1.17.  Its value is used as a fallback if
    **iprop_replica_poll** is not specified.

**iprop_listen**
    (Whitespace- or comma-separated list.)  Specifies the iprop RPC
    listening addresses and/or ports for the :ref:`kadmind(8)` daemon.
    Each entry may be an interface address, a port number, or an
    address and port number separated by a colon.  If the address
    contains colons, enclose it in square brackets.  If no address is
    specified, the wildcard address is used.  If kadmind fails to bind
    to any of the specified addresses, it will fail to start.  The
    default (when **iprop_enable** is true) is to bind to the wildcard
    address at the port specified in **iprop_port**.  New in release
    1.15.

**iprop_port**
    (Port number.)  Specifies the port number to be used for
    incremental propagation.  When **iprop_enable** is true, this
    relation is required in the replica KDC configuration file, and
    this relation or **iprop_listen** is required in the primary
    configuration file, as there is no default port number.  Port
    numbers specified in **iprop_listen** entries will override this
    port number for the :ref:`kadmind(8)` daemon.

**iprop_resync_timeout**
    (Delta time string.)  Specifies the amount of time to wait for a
    full propagation to complete.  This is optional in configuration
    files, and is used by replica KDCs only.  The default value is 5
    minutes (``5m``).  New in release 1.11.

**iprop_logfile**
    (File name.)  Specifies where the update log file for the realm
    database is to be stored.  The default is to use the
    **database_name** entry from the realms section of the krb5 config
    file, with ``.ulog`` appended.  (NOTE: If **database_name** isn't
    specified in the realms section, perhaps because the LDAP database
    back end is being used, or the file name is specified in the
    [dbmodules] section, then the hard-coded default for
    **database_name** is used.  Determination of the **iprop_logfile**
    default value will not use values from the [dbmodules] section.)

**kadmind_listen**
    (Whitespace- or comma-separated list.)  Specifies the kadmin RPC
    listening addresses and/or ports for the :ref:`kadmind(8)` daemon.
    Each entry may be an interface address, a port number, or an
    address and port number separated by a colon.  If the address
    contains colons, enclose it in square brackets.  If no address is
    specified, the wildcard address is used.  If kadmind fails to bind
    to any of the specified addresses, it will fail to start.  The
    default is to bind to the wildcard address at the port specified
    in **kadmind_port**, or the standard kadmin port (749).  New in
    release 1.15.

**kadmind_port**
    (Port number.)  Specifies the port on which the :ref:`kadmind(8)`
    daemon is to listen for this realm.  Port numbers specified in
    **kadmind_listen** entries will override this port number.  The
    assigned port for kadmind is 749, which is used by default.

**key_stash_file**
    (String.)  Specifies the location where the master key has been
    stored (via kdb5_util stash).  The default is |kdcdir|\
    ``/.k5.REALM``, where *REALM* is the Kerberos realm.

**kdc_listen**
    (Whitespace- or comma-separated list.)  Specifies the UDP
    listening addresses and/or ports for the :ref:`krb5kdc(8)` daemon.
    Each entry may be an interface address, a port number, or an
    address and port number separated by a colon.  If the address
    contains colons, enclose it in square brackets.  If no address is
    specified, the wildcard address is used.  If no port is specified,
    the standard port (88) is used.  If the KDC daemon fails to bind
    to any of the specified addresses, it will fail to start.  The
    default is to bind to the wildcard address on the standard port.
    New in release 1.15.

**kdc_ports**
    (Whitespace- or comma-separated list, deprecated.)  Prior to
    release 1.15, this relation lists the ports for the
    :ref:`krb5kdc(8)` daemon to listen on for UDP requests.  In
    release 1.15 and later, it has the same meaning as **kdc_listen**
    if that relation is not defined.

**kdc_tcp_listen**
    (Whitespace- or comma-separated list.)  Specifies the TCP
    listening addresses and/or ports for the :ref:`krb5kdc(8)` daemon.
    Each entry may be an interface address, a port number, or an
    address and port number separated by a colon.  If the address
    contains colons, enclose it in square brackets.  If no address is
    specified, the wildcard address is used.  If no port is specified,
    the standard port (88) is used.  To disable listening on TCP, set
    this relation to the empty string with ``kdc_tcp_listen = ""``.
    If the KDC daemon fails to bind to any of the specified addresses,
    it will fail to start.  The default is to bind to the wildcard
    address on the standard port.  New in release 1.15.

**kdc_tcp_ports**
    (Whitespace- or comma-separated list, deprecated.)  Prior to
    release 1.15, this relation lists the ports for the
    :ref:`krb5kdc(8)` daemon to listen on for UDP requests.  In
    release 1.15 and later, it has the same meaning as
    **kdc_tcp_listen** if that relation is not defined.

**kpasswd_listen**
    (Comma-separated list.)  Specifies the kpasswd listening addresses
    and/or ports for the :ref:`kadmind(8)` daemon.  Each entry may be
    an interface address, a port number, or an address and port number
    separated by a colon.  If the address contains colons, enclose it
    in square brackets.  If no address is specified, the wildcard
    address is used.  If kadmind fails to bind to any of the specified
    addresses, it will fail to start.  The default is to bind to the
    wildcard address at the port specified in **kpasswd_port**, or the
    standard kpasswd port (464).  New in release 1.15.

**kpasswd_port**
    (Port number.)  Specifies the port on which the :ref:`kadmind(8)`
    daemon is to listen for password change requests for this realm.
    Port numbers specified in **kpasswd_listen** entries will override
    this port number.  The assigned port for password change requests
    is 464, which is used by default.

**master_key_name**
    (String.)  Specifies the name of the principal associated with the
    master key.  The default is ``K/M``.

**master_key_type**
    (Key type string.)  Specifies the master key's key type.  The
    default value for this is |defmkey|.  For a list of all possible
    values, see :ref:`Encryption_types`.

**max_life**
    (:ref:`duration` string.)  Specifies the maximum time period for
    which a ticket may be valid in this realm.  The default value is
    24 hours.

**max_renewable_life**
    (:ref:`duration` string.)  Specifies the maximum time period
    during which a valid ticket may be renewed in this realm.
    The default value is 0.

**no_host_referral**
    (Whitespace- or comma-separated list.)  Lists services to block
    from getting host-based referral processing, even if the client
    marks the server principal as host-based or the service is also
    listed in **host_based_services**.  ``no_host_referral = *`` will
    disable referral processing altogether.

**reject_bad_transit**
    (Boolean value.)  If set to true, the KDC will check the list of
    transited realms for cross-realm tickets against the transit path
    computed from the realm names and the capaths section of its
    :ref:`krb5.conf(5)` file; if the path in the ticket to be issued
    contains any realms not in the computed path, the ticket will not
    be issued, and an error will be returned to the client instead.
    If this value is set to false, such tickets will be issued
    anyways, and it will be left up to the application server to
    validate the realm transit path.

    If the disable-transited-check flag is set in the incoming
    request, this check is not performed at all.  Having the
    **reject_bad_transit** option will cause such ticket requests to
    be rejected always.

    This transit path checking and config file option currently apply
    only to TGS requests.

    The default value is true.

**restrict_anonymous_to_tgt**
    (Boolean value.)  If set to true, the KDC will reject ticket
    requests from anonymous principals to service principals other
    than the realm's ticket-granting service.  This option allows
    anonymous PKINIT to be enabled for use as FAST armor tickets
    without allowing anonymous authentication to services.  The
    default value is false.  New in release 1.9.

**spake_preauth_indicator**
    (String.)  Specifies an authentication indicator value that the
    KDC asserts into tickets obtained using SPAKE pre-authentication.
    The default is not to add any indicators.  This option may be
    specified multiple times.  New in release 1.17.

**supported_enctypes**
    (List of *key*:*salt* strings.)  Specifies the default key/salt
    combinations of principals for this realm.  Any principals created
    through :ref:`kadmin(1)` will have keys of these types.  The
    default value for this tag is |defkeysalts|.  For lists of
    possible values, see :ref:`Keysalt_lists`.


.. _dbdefaults:

[dbdefaults]
~~~~~~~~~~~~

The [dbdefaults] section specifies default values for some database
parameters, to be used if the [dbmodules] subsection does not contain
a relation for the tag.  See the :ref:`dbmodules` section for the
definitions of these relations.

* **ldap_kerberos_container_dn**
* **ldap_kdc_dn**
* **ldap_kdc_sasl_authcid**
* **ldap_kdc_sasl_authzid**
* **ldap_kdc_sasl_mech**
* **ldap_kdc_sasl_realm**
* **ldap_kadmind_dn**
* **ldap_kadmind_sasl_authcid**
* **ldap_kadmind_sasl_authzid**
* **ldap_kadmind_sasl_mech**
* **ldap_kadmind_sasl_realm**
* **ldap_service_password_file**
* **ldap_conns_per_server**


.. _dbmodules:

[dbmodules]
~~~~~~~~~~~

The [dbmodules] section contains parameters used by the KDC database
library and database modules.  Each tag in the [dbmodules] section is
the name of a Kerberos realm or a section name specified by a realm's
**database_module** parameter.  The following example shows how to
define one database parameter for the ATHENA.MIT.EDU realm::

    [dbmodules]
        ATHENA.MIT.EDU = {
            disable_last_success = true
        }

The following tags may be specified in a [dbmodules] subsection:

**database_name**
    This DB2-specific tag indicates the location of the database in
    the filesystem.  The default is |kdcdir|\ ``/principal``.

**db_library**
    This tag indicates the name of the loadable database module.  The
    value should be ``db2`` for the DB2 module, ``klmdb`` for the LMDB
    module, or ``kldap`` for the LDAP module.

**disable_last_success**
    If set to ``true``, suppresses KDC updates to the "Last successful
    authentication" field of principal entries requiring
    preauthentication.  Setting this flag may improve performance.
    (Principal entries which do not require preauthentication never
    update the "Last successful authentication" field.).  First
    introduced in release 1.9.

**disable_lockout**
    If set to ``true``, suppresses KDC updates to the "Last failed
    authentication" and "Failed password attempts" fields of principal
    entries requiring preauthentication.  Setting this flag may
    improve performance, but also disables account lockout.  First
    introduced in release 1.9.

**ldap_conns_per_server**
    This LDAP-specific tag indicates the number of connections to be
    maintained per LDAP server.

**ldap_kdc_dn** and **ldap_kadmind_dn**
    These LDAP-specific tags indicate the default DN for binding to
    the LDAP server.  The :ref:`krb5kdc(8)` daemon uses
    **ldap_kdc_dn**, while the :ref:`kadmind(8)` daemon and other
    administrative programs use **ldap_kadmind_dn**.  The kadmind DN
    must have the rights to read and write the Kerberos data in the
    LDAP database.  The KDC DN must have the same rights, unless
    **disable_lockout** and **disable_last_success** are true, in
    which case it only needs to have rights to read the Kerberos data.
    These tags are ignored if a SASL mechanism is set with
    **ldap_kdc_sasl_mech** or **ldap_kadmind_sasl_mech**.

**ldap_kdc_sasl_mech** and **ldap_kadmind_sasl_mech**
    These LDAP-specific tags specify the SASL mechanism (such as
    ``EXTERNAL``) to use when binding to the LDAP server.  New in
    release 1.13.

**ldap_kdc_sasl_authcid** and **ldap_kadmind_sasl_authcid**
    These LDAP-specific tags specify the SASL authentication identity
    to use when binding to the LDAP server.  Not all SASL mechanisms
    require an authentication identity.  If the SASL mechanism
    requires a secret (such as the password for ``DIGEST-MD5``), these
    tags also determine the name within the
    **ldap_service_password_file** where the secret is stashed.  New
    in release 1.13.

**ldap_kdc_sasl_authzid** and **ldap_kadmind_sasl_authzid**
    These LDAP-specific tags specify the SASL authorization identity
    to use when binding to the LDAP server.  In most circumstances
    they do not need to be specified.  New in release 1.13.

**ldap_kdc_sasl_realm** and **ldap_kadmind_sasl_realm**
    These LDAP-specific tags specify the SASL realm to use when
    binding to the LDAP server.  In most circumstances they do not
    need to be set.  New in release 1.13.

**ldap_kerberos_container_dn**
    This LDAP-specific tag indicates the DN of the container object
    where the realm objects will be located.

**ldap_servers**
    This LDAP-specific tag indicates the list of LDAP servers that the
    Kerberos servers can connect to.  The list of LDAP servers is
    whitespace-separated.  The LDAP server is specified by a LDAP URI.
    It is recommended to use ``ldapi:`` or ``ldaps:`` URLs to connect
    to the LDAP server.

**ldap_service_password_file**
    This LDAP-specific tag indicates the file containing the stashed
    passwords (created by ``kdb5_ldap_util stashsrvpw``) for the
    **ldap_kdc_dn** and **ldap_kadmind_dn** objects, or for the
    **ldap_kdc_sasl_authcid** or **ldap_kadmind_sasl_authcid** names
    for SASL authentication.  This file must be kept secure.

**mapsize**
    This LMDB-specific tag indicates the maximum size of the two
    database environments in megabytes.  The default value is 128.
    Increase this value to address "Environment mapsize limit reached"
    errors.  New in release 1.17.

**max_readers**
    This LMDB-specific tag indicates the maximum number of concurrent
    reading processes for the databases.  The default value is 128.
    New in release 1.17.

**nosync**
    This LMDB-specific tag can be set to improve the throughput of
    kadmind and other administrative agents, at the expense of
    durability (recent database changes may not survive a power outage
    or other sudden reboot).  It does not affect the throughput of the
    KDC.  The default value is false.  New in release 1.17.

**unlockiter**
    If set to ``true``, this DB2-specific tag causes iteration
    operations to release the database lock while processing each
    principal.  Setting this flag to ``true`` can prevent extended
    blocking of KDC or kadmin operations when dumps of large databases
    are in progress.  First introduced in release 1.13.

The following tag may be specified directly in the [dbmodules]
section to control where database modules are loaded from:

**db_module_dir**
    This tag controls where the plugin system looks for database
    modules.  The value should be an absolute path.

.. _logging:

[logging]
~~~~~~~~~

The [logging] section indicates how :ref:`krb5kdc(8)` and
:ref:`kadmind(8)` perform logging.  It may contain the following
relations:

**admin_server**
    Specifies how :ref:`kadmind(8)` performs logging.

**kdc**
    Specifies how :ref:`krb5kdc(8)` performs logging.

**default**
    Specifies how either daemon performs logging in the absence of
    relations specific to the daemon.

**debug**
    (Boolean value.)  Specifies whether debugging messages are
    included in log outputs other than SYSLOG.  Debugging messages are
    always included in the system log output because syslog performs
    its own priority filtering.  The default value is false.  New in
    release 1.15.

Logging specifications may have the following forms:

**FILE=**\ *filename* or **FILE:**\ *filename*
    This value causes the daemon's logging messages to go to the
    *filename*.  If the ``=`` form is used, the file is overwritten.
    If the ``:`` form is used, the file is appended to.

**STDERR**
    This value causes the daemon's logging messages to go to its
    standard error stream.

**CONSOLE**
    This value causes the daemon's logging messages to go to the
    console, if the system supports it.

**DEVICE=**\ *<devicename>*
    This causes the daemon's logging messages to go to the specified
    device.

**SYSLOG**\ [\ **:**\ *severity*\ [\ **:**\ *facility*\ ]]
    This causes the daemon's logging messages to go to the system log.

    For backward compatibility, a severity argument may be specified,
    and must be specified in order to specify a facility.  This
    argument will be ignored.

    The facility argument specifies the facility under which the
    messages are logged.  This may be any of the following facilities
    supported by the syslog(3) call minus the LOG\_ prefix: **KERN**,
    **USER**, **MAIL**, **DAEMON**, **AUTH**, **LPR**, **NEWS**,
    **UUCP**, **CRON**, and **LOCAL0** through **LOCAL7**.  If no
    facility is specified, the default is **AUTH**.

In the following example, the logging messages from the KDC will go to
the console and to the system log under the facility LOG_DAEMON, and
the logging messages from the administrative server will be appended
to the file ``/var/adm/kadmin.log`` and sent to the device
``/dev/tty04``. ::

    [logging]
        kdc = CONSOLE
        kdc = SYSLOG:INFO:DAEMON
        admin_server = FILE:/var/adm/kadmin.log
        admin_server = DEVICE=/dev/tty04

If no logging specification is given, the default is to use syslog.
To disable logging entirely, specify ``default = DEVICE=/dev/null``.


.. _otp:

[otp]
~~~~~

Each subsection of [otp] is the name of an OTP token type.  The tags
within the subsection define the configuration required to forward a
One Time Password request to a RADIUS server.

For each token type, the following tags may be specified:

**server**
    This is the server to send the RADIUS request to.  It can be a
    hostname with optional port, an ip address with optional port, or
    a Unix domain socket address.  The default is
    |kdcdir|\ ``/<name>.socket``.

**secret**
    This tag indicates a filename (which may be relative to |kdcdir|)
    containing the secret used to encrypt the RADIUS packets.  The
    secret should appear in the first line of the file by itself;
    leading and trailing whitespace on the line will be removed.  If
    the value of **server** is a Unix domain socket address, this tag
    is optional, and an empty secret will be used if it is not
    specified.  Otherwise, this tag is required.

**timeout**
    An integer which specifies the time in seconds during which the
    KDC should attempt to contact the RADIUS server.  This tag is the
    total time across all retries and should be less than the time
    which an OTP value remains valid for.  The default is 5 seconds.

**retries**
    This tag specifies the number of retries to make to the RADIUS
    server.  The default is 3 retries (4 tries).

**strip_realm**
    If this tag is ``true``, the principal without the realm will be
    passed to the RADIUS server.  Otherwise, the realm will be
    included.  The default value is ``true``.

**indicator**
    This tag specifies an authentication indicator to be included in
    the ticket if this token type is used to authenticate.  This
    option may be specified multiple times.  (New in release 1.14.)

In the following example, requests are sent to a remote server via UDP::

    [otp]
        MyRemoteTokenType = {
            server = radius.mydomain.com:1812
            secret = SEmfiajf42$
            timeout = 15
            retries = 5
            strip_realm = true
        }

An implicit default token type named ``DEFAULT`` is defined for when
the per-principal configuration does not specify a token type.  Its
configuration is shown below.  You may override this token type to
something applicable for your situation::

    [otp]
        DEFAULT = {
            strip_realm = false
        }

PKINIT options
--------------

.. note::

          The following are pkinit-specific options.  These values may
          be specified in [kdcdefaults] as global defaults, or within
          a realm-specific subsection of [realms].  Also note that a
          realm-specific value over-rides, does not add to, a generic
          [kdcdefaults] specification.  The search order is:

1. realm-specific subsection of [realms]::

       [realms]
           EXAMPLE.COM = {
               pkinit_anchors = FILE:/usr/local/example.com.crt
           }

2. generic value in the [kdcdefaults] section::

       [kdcdefaults]
           pkinit_anchors = DIR:/usr/local/generic_trusted_cas/

For information about the syntax of some of these options, see
:ref:`Specifying PKINIT identity information <pkinit_identity>` in
:ref:`krb5.conf(5)`.

**pkinit_anchors**
    Specifies the location of trusted anchor (root) certificates which
    the KDC trusts to sign client certificates.  This option is
    required if pkinit is to be supported by the KDC.  This option may
    be specified multiple times.

**pkinit_dh_min_bits**
    Specifies the minimum number of bits the KDC is willing to accept
    for a client's Diffie-Hellman key.  The default is 2048.

**pkinit_allow_upn**
    Specifies that the KDC is willing to accept client certificates
    with the Microsoft UserPrincipalName (UPN) Subject Alternative
    Name (SAN).  This means the KDC accepts the binding of the UPN in
    the certificate to the Kerberos principal name.  The default value
    is false.

    Without this option, the KDC will only accept certificates with
    the id-pkinit-san as defined in :rfc:`4556`.  There is currently
    no option to disable SAN checking in the KDC.

**pkinit_eku_checking**
    This option specifies what Extended Key Usage (EKU) values the KDC
    is willing to accept in client certificates.  The values
    recognized in the kdc.conf file are:

    **kpClientAuth**
        This is the default value and specifies that client
        certificates must have the id-pkinit-KPClientAuth EKU as
        defined in :rfc:`4556`.

    **scLogin**
        If scLogin is specified, client certificates with the
        Microsoft Smart Card Login EKU (id-ms-kp-sc-logon) will be
        accepted.

    **none**
        If none is specified, then client certificates will not be
        checked to verify they have an acceptable EKU.  The use of
        this option is not recommended.

**pkinit_identity**
    Specifies the location of the KDC's X.509 identity information.
    This option is required if pkinit is to be supported by the KDC.

**pkinit_indicator**
    Specifies an authentication indicator to include in the ticket if
    pkinit is used to authenticate.  This option may be specified
    multiple times.  (New in release 1.14.)

**pkinit_pool**
    Specifies the location of intermediate certificates which may be
    used by the KDC to complete the trust chain between a client's
    certificate and a trusted anchor.  This option may be specified
    multiple times.

**pkinit_revoke**
    Specifies the location of Certificate Revocation List (CRL)
    information to be used by the KDC when verifying the validity of
    client certificates.  This option may be specified multiple times.

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

**pkinit_require_freshness**
    Specifies whether to require clients to include a freshness token
    in PKINIT requests.  The default value is false.  (New in release
    1.17.)

.. _Encryption_types:

Encryption types
----------------

Any tag in the configuration files which requires a list of encryption
types can be set to some combination of the following strings.
Encryption types marked as "weak" and "deprecated" are available for
compatibility but not recommended for use.

==================================================== =========================================================
des3-cbc-raw                                         Triple DES cbc mode raw (weak)
des3-cbc-sha1 des3-hmac-sha1 des3-cbc-sha1-kd        Triple DES cbc mode with HMAC/sha1 (deprecated)
aes256-cts-hmac-sha1-96 aes256-cts aes256-sha1       AES-256 CTS mode with 96-bit SHA-1 HMAC
aes128-cts-hmac-sha1-96 aes128-cts aes128-sha1       AES-128 CTS mode with 96-bit SHA-1 HMAC
aes256-cts-hmac-sha384-192 aes256-sha2               AES-256 CTS mode with 192-bit SHA-384 HMAC
aes128-cts-hmac-sha256-128 aes128-sha2               AES-128 CTS mode with 128-bit SHA-256 HMAC
arcfour-hmac rc4-hmac arcfour-hmac-md5               RC4 with HMAC/MD5 (deprecated)
arcfour-hmac-exp rc4-hmac-exp arcfour-hmac-md5-exp   Exportable RC4 with HMAC/MD5 (weak)
camellia256-cts-cmac camellia256-cts                 Camellia-256 CTS mode with CMAC
camellia128-cts-cmac camellia128-cts                 Camellia-128 CTS mode with CMAC
des3                                                 The triple DES family: des3-cbc-sha1
aes                                                  The AES family: aes256-cts-hmac-sha1-96, aes128-cts-hmac-sha1-96, aes256-cts-hmac-sha384-192, and aes128-cts-hmac-sha256-128
rc4                                                  The RC4 family: arcfour-hmac
camellia                                             The Camellia family: camellia256-cts-cmac and camellia128-cts-cmac
==================================================== =========================================================

The string **DEFAULT** can be used to refer to the default set of
types for the variable in question.  Types or families can be removed
from the current list by prefixing them with a minus sign ("-").
Types or families can be prefixed with a plus sign ("+") for symmetry;
it has the same meaning as just listing the type or family.  For
example, "``DEFAULT -rc4``" would be the default set of encryption
types with RC4 types removed, and "``des3 DEFAULT``" would be the
default set of encryption types with triple DES types moved to the
front.

While **aes128-cts** and **aes256-cts** are supported for all Kerberos
operations, they are not supported by very old versions of our GSSAPI
implementation (krb5-1.3.1 and earlier).  Services running versions of
krb5 without AES support must not be given keys of these encryption
types in the KDC database.

The **aes128-sha2** and **aes256-sha2** encryption types are new in
release 1.15.  Services running versions of krb5 without support for
these newer encryption types must not be given keys of these
encryption types in the KDC database.


.. _Keysalt_lists:

Keysalt lists
-------------

Kerberos keys for users are usually derived from passwords.  Kerberos
commands and configuration parameters that affect generation of keys
take lists of enctype-salttype ("keysalt") pairs, known as *keysalt
lists*.  Each keysalt pair is an enctype name followed by a salttype
name, in the format *enc*:*salt*.  Individual keysalt list members are
separated by comma (",") characters or space characters.  For example::

    kadmin -e aes256-cts:normal,aes128-cts:normal

would start up kadmin so that by default it would generate
password-derived keys for the **aes256-cts** and **aes128-cts**
encryption types, using a **normal** salt.

To ensure that people who happen to pick the same password do not have
the same key, Kerberos 5 incorporates more information into the key
using something called a salt.  The supported salt types are as
follows:

================= ============================================
normal            default for Kerberos Version 5
norealm           same as the default, without using realm information
onlyrealm         uses only realm information as the salt
special           generate a random salt
================= ============================================


Sample kdc.conf File
--------------------

Here's an example of a kdc.conf file::

    [kdcdefaults]
        kdc_listen = 88
        kdc_tcp_listen = 88
    [realms]
        ATHENA.MIT.EDU = {
            kadmind_port = 749
            max_life = 12h 0m 0s
            max_renewable_life = 7d 0h 0m 0s
            master_key_type = aes256-cts-hmac-sha1-96
            supported_enctypes = aes256-cts-hmac-sha1-96:normal aes128-cts-hmac-sha1-96:normal
            database_module = openldap_ldapconf
        }

    [logging]
        kdc = FILE:/usr/local/var/krb5kdc/kdc.log
        admin_server = FILE:/usr/local/var/krb5kdc/kadmin.log

    [dbdefaults]
        ldap_kerberos_container_dn = cn=krbcontainer,dc=mit,dc=edu

    [dbmodules]
        openldap_ldapconf = {
            db_library = kldap
            disable_last_success = true
            ldap_kdc_dn = "cn=krbadmin,dc=mit,dc=edu"
                # this object needs to have read rights on
                # the realm container and principal subtrees
            ldap_kadmind_dn = "cn=krbadmin,dc=mit,dc=edu"
                # this object needs to have read and write rights on
                # the realm container and principal subtrees
            ldap_service_password_file = /etc/kerberos/service.keyfile
            ldap_servers = ldaps://kerberos.mit.edu
            ldap_conns_per_server = 5
        }


FILES
------

|kdcdir|\ ``/kdc.conf``


SEE ALSO
---------

:ref:`krb5.conf(5)`, :ref:`krb5kdc(8)`, :ref:`kadm5.acl(5)`
