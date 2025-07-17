.. highlight:: rst

.. toctree::
    :hidden:

    mitK5license.rst

.. _mitK5features:

MIT Kerberos features
=====================

https://web.mit.edu/kerberos


Quick facts
-----------

License - :ref:`mitK5license`

Releases:
    - Latest stable: https://web.mit.edu/kerberos/krb5-1.20/
    - Supported: https://web.mit.edu/kerberos/krb5-1.19/
    - Release cycle: approximately 12 months

Supported platforms \/ OS distributions:
    - Windows (KfW 4.0): Windows 7, Vista, XP
    - Solaris: SPARC, x86_64/x86
    - GNU/Linux: Debian x86_64/x86, Ubuntu x86_64/x86, RedHat x86_64/x86
    - BSD: NetBSD x86_64/x86

Crypto backends:
    - builtin - MIT Kerberos native crypto library
    - OpenSSL (1.0\+) - https://www.openssl.org

Database backends: LDAP, DB2, LMDB

krb4 support: Kerberos 5 release < 1.8

DES support: Kerberos 5 release < 1.18 (See :ref:`retiring-des`)

Interoperability
----------------

`Microsoft`

Starting from release 1.7:

* Follow client principal referrals in the client library when
  obtaining initial tickets.

* KDC can issue realm referrals for service principals based on domain names.

* Extensions supporting DCE RPC, including three-leg GSS context setup
  and unencapsulated GSS tokens inside SPNEGO.

* Microsoft GSS_WrapEX, implemented using the gss_iov API, which is
  similar to the equivalent SSPI functionality.  This is needed to
  support some instances of DCE RPC.

* NTLM recognition support in GSS-API, to facilitate dropping in an
  NTLM implementation for improved compatibility with older releases
  of Microsoft Windows.

* KDC support for principal aliases, if the back end supports them.
  Currently, only the LDAP back end supports aliases.

* Support Microsoft set/change password (:rfc:`3244`) protocol in
  kadmind.

* Implement client and KDC support for GSS_C_DELEG_POLICY_FLAG, which
  allows a GSS application to request credential delegation only if
  permitted by KDC policy.


Starting from release 1.8:

* Microsoft Services for User (S4U) compatibility


`Heimdal`

* Support for KCM credential cache starting from release 1.13

Feature list
------------

For more information on the specific project see https://k5wiki.kerberos.org/wiki/Projects

Release 1.7
 -   Credentials delegation                   :rfc:`5896`
 -   Cross-realm authentication and referrals :rfc:`6806`
 -   Master key migration
 -   PKINIT                                   :rfc:`4556` :ref:`pkinit`

Release 1.8
 -   Anonymous PKINIT         :rfc:`6112` :ref:`anonymous_pkinit`
 -   Constrained delegation
 -   IAKERB                   https://tools.ietf.org/html/draft-ietf-krb-wg-iakerb-02
 -   Heimdal bridge plugin for KDC backend
 -   GSS-API S4U extensions   https://msdn.microsoft.com/en-us/library/cc246071
 -   GSS-API naming extensions                            :rfc:`6680`
 -   GSS-API extensions for storing delegated credentials :rfc:`5588`

Release 1.9
 -   Advance warning on password expiry
 -   Camellia encryption (CTS-CMAC mode)       :rfc:`6803`
 -   KDC support for SecurID preauthentication
 -   kadmin over IPv6
 -   Trace logging                             :ref:`trace_logging`
 -   GSSAPI/KRB5 multi-realm support
 -   Plugin to test password quality           :ref:`pwqual_plugin`
 -   Plugin to synchronize password changes    :ref:`kadm5_hook_plugin`
 -   Parallel KDC
 -   GSS-API extensions for SASL GS2 bridge    :rfc:`5801` :rfc:`5587`
 -   Purging old keys
 -   Naming extensions for delegation chain
 -   Password expiration API
 -   Windows client support   (build-only)
 -   IPv6 support in iprop

Release 1.10
 -   Plugin interface for configuration        :ref:`profile_plugin`
 -   Credentials for multiple identities       :ref:`ccselect_plugin`

Release 1.11
 -   Client support for FAST OTP               :rfc:`6560`
 -   GSS-API extensions for credential locations
 -   Responder mechanism

Release 1.12
 -   Plugin to control krb5_aname_to_localname and krb5_kuserok behavior   :ref:`localauth_plugin`
 -   Plugin to control hostname-to-realm mappings and the default realm    :ref:`hostrealm_plugin`
 -   GSSAPI extensions for constructing MIC tokens using IOV lists         :ref:`gssapi_mic_token`
 -   Principal may refer to nonexistent policies `Policy Refcount project <https://k5wiki.kerberos.org/wiki/Projects/Policy_refcount_elimination>`_
 -   Support for having no long-term keys for a principal `Principals Without Keys project <https://k5wiki.kerberos.org/wiki/Projects/Principals_without_keys>`_
 -   Collection support to the KEYRING credential cache type on Linux :ref:`ccache_definition`
 -   FAST OTP preauthentication module for the KDC which uses RADIUS to validate OTP token values :ref:`otp_preauth`
 -   Experimental Audit plugin for KDC processing `Audit project <https://k5wiki.kerberos.org/wiki/Projects/Audit>`_

Release 1.13

 -   Add support for accessing KDCs via an HTTPS proxy server using
     the `MS-KKDCP
     <https://msdn.microsoft.com/en-us/library/hh553774.aspx>`_
     protocol.
 -   Add support for `hierarchical incremental propagation
     <https://k5wiki.kerberos.org/wiki/Projects/Hierarchical_iprop>`_,
     where replicas can act as intermediates between an upstream primary
     and other downstream replicas.
 -   Add support for configuring GSS mechanisms using
     ``/etc/gss/mech.d/*.conf`` files in addition to
     ``/etc/gss/mech``.
 -   Add support to the LDAP KDB module for `binding to the LDAP
     server using SASL
     <https://k5wiki.kerberos.org/wiki/Projects/LDAP_SASL_support>`_.
 -   The KDC listens for TCP connections by default.
 -   Fix a minor key disclosure vulnerability where using the
     "keepold" option to the kadmin randkey operation could return the
     old keys. `[CVE-2014-5351]
     <https://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2014-5351>`_
 -   Add client support for the Kerberos Cache Manager protocol. If
     the host is running a Heimdal kcm daemon, caches served by the
     daemon can be accessed with the KCM: cache type.
 -   When built on macOS 10.7 and higher, use "KCM:" as the default
     cachetype, unless overridden by command-line options or
     krb5-config values.
 -   Add support for doing unlocked database dumps for the DB2 KDC
     back end, which would allow the KDC and kadmind to continue
     accessing the database during lengthy database dumps.

Release 1.14

 * Administrator experience

   - Add a new kdb5_util tabdump command to provide reporting-friendly
     tabular dump formats (tab-separated or CSV) for the KDC database.
     Unlike the normal dump format, each output table has a fixed number
     of fields.  Some tables include human-readable forms of data that
     are opaque in ordinary dump files.  This format is also suitable for
     importing into relational databases for complex queries.
   - Add support to kadmin and kadmin.local for specifying a single
     command line following any global options, where the command
     arguments are split by the shell--for example, "kadmin getprinc
     principalname".  Commands issued this way do not prompt for
     confirmation or display warning messages, and exit with non-zero
     status if the operation fails.
   - Accept the same principal flag names in kadmin as we do for the
     default_principal_flags kdc.conf variable, and vice versa.  Also
     accept flag specifiers in the form that kadmin prints, as well as
     hexadecimal numbers.
   - Remove the triple-DES and RC4 encryption types from the default
     value of supported_enctypes, which determines the default key and
     salt types for new password-derived keys.  By default, keys will
     only created only for AES128 and AES256.  This mitigates some types
     of password guessing attacks.
   - Add support for directory names in the KRB5_CONFIG and
     KRB5_KDC_PROFILE environment variables.
   - Add support for authentication indicators, which are ticket
     annotations to indicate the strength of the initial authentication.
     Add support for the "require_auth" string attribute, which can be
     set on server principal entries to require an indicator when
     authenticating to the server.
   - Add support for key version numbers larger than 255 in keytab files,
     and for version numbers up to 65535 in KDC databases.
   - Transmit only one ETYPE-INFO and/or ETYPE-INFO2 entry from the KDC
     during pre-authentication, corresponding to the client's most
     preferred encryption type.
   - Add support for server name identification (SNI) when proxying KDC
     requests over HTTPS.
   - Add support for the err_fmt profile parameter, which can be used to
     generate custom-formatted error messages.

 * Developer experience:

   - Change gss_acquire_cred_with_password() to acquire credentials into
     a private memory credential cache.  Applications can use
     gss_store_cred() to make the resulting credentials visible to other
     processes.
   - Change gss_acquire_cred() and SPNEGO not to acquire credentials for
     IAKERB or for non-standard variants of the krb5 mechanism OID unless
     explicitly requested.  (SPNEGO will still accept the Microsoft
     variant of the krb5 mechanism OID during negotiation.)
   - Change gss_accept_sec_context() not to accept tokens for IAKERB or
     for non-standard variants of the krb5 mechanism OID unless an
     acceptor credential is acquired for those mechanisms.
   - Change gss_acquire_cred() to immediately resolve credentials if the
     time_rec parameter is not NULL, so that a correct expiration time
     can be returned.  Normally credential resolution is delayed until
     the target name is known.
   - Add krb5_prepend_error_message() and krb5_wrap_error_message() APIs,
     which can be used by plugin modules or applications to add prefixes
     to existing detailed error messages.
   - Add krb5_c_prfplus() and krb5_c_derive_prfplus() APIs, which
     implement the RFC 6113 PRF+ operation and key derivation using PRF+.
   - Add support for pre-authentication mechanisms which use multiple
     round trips, using the the KDC_ERR_MORE_PREAUTH_DATA_REQUIRED error
     code.  Add get_cookie() and set_cookie() callbacks to the kdcpreauth
     interface; these callbacks can be used to save marshalled state
     information in an encrypted cookie for the next request.
   - Add a client_key() callback to the kdcpreauth interface to retrieve
     the chosen client key, corresponding to the ETYPE-INFO2 entry sent
     by the KDC.
   - Add an add_auth_indicator() callback to the kdcpreauth interface,
     allowing pre-authentication modules to assert authentication
     indicators.
   - Add support for the GSS_KRB5_CRED_NO_CI_FLAGS_X cred option to
     suppress sending the confidentiality and integrity flags in GSS
     initiator tokens unless they are requested by the caller.  These
     flags control the negotiated SASL security layer for the Microsoft
     GSS-SPNEGO SASL mechanism.
   - Make the FILE credential cache implementation less prone to
     corruption issues in multi-threaded programs, especially on
     platforms with support for open file description locks.

 * Performance:

   - On replica KDCs, poll the primary KDC immediately after
     processing a full resync, and do not require two full resyncs
     after the primary KDC's log file is reset.

Release 1.15

* Administrator experience:

  - Add support to kadmin for remote extraction of current keys
    without changing them (requires a special kadmin permission that
    is excluded from the wildcard permission), with the exception of
    highly protected keys.

  - Add a lockdown_keys principal attribute to prevent retrieval of
    the principal's keys (old or new) via the kadmin protocol.  In
    newly created databases, this attribute is set on the krbtgt and
    kadmin principals.

  - Restore recursive dump capability for DB2 back end, so sites can
    more easily recover from database corruption resulting from power
    failure events.

  - Add DNS auto-discovery of KDC and kpasswd servers from URI
    records, in addition to SRV records.  URI records can convey TCP
    and UDP servers and primary KDC status in a single DNS lookup, and
    can also point to HTTPS proxy servers.

  - Add support for password history to the LDAP back end.

  - Add support for principal renaming to the LDAP back end.

  - Use the getrandom system call on supported Linux kernels to avoid
    blocking problems when getting entropy from the operating system.

* Code quality:

  - Clean up numerous compilation warnings.

  - Remove various infrequently built modules, including some preauth
    modules that were not built by default.

* Developer experience:

  - Add support for building with OpenSSL 1.1.

  - Use SHA-256 instead of MD5 for (non-cryptographic) hashing of
    authenticators in the replay cache.  This helps sites that must
    build with FIPS 140 conformant libraries that lack MD5.

* Protocol evolution:

  - Add support for the AES-SHA2 enctypes, which allows sites to
    conform to Suite B crypto requirements.

Release 1.16

* Administrator experience:

  - The KDC can match PKINIT client certificates against the
    "pkinit_cert_match" string attribute on the client principal
    entry, using the same syntax as the existing "pkinit_cert_match"
    profile option.

  - The ktutil addent command supports the "-k 0" option to ignore the
    key version, and the "-s" option to use a non-default salt string.

  - kpropd supports a --pid-file option to write a pid file at
    startup, when it is run in standalone mode.

  - The "encrypted_challenge_indicator" realm option can be used to
    attach an authentication indicator to tickets obtained using FAST
    encrypted challenge pre-authentication.

  - Localization support can be disabled at build time with the
    --disable-nls configure option.

* Developer experience:

  - The kdcpolicy pluggable interface allows modules control whether
    tickets are issued by the KDC.

  - The kadm5_auth pluggable interface allows modules to control
    whether kadmind grants access to a kadmin request.

  - The certauth pluggable interface allows modules to control which
    PKINIT client certificates can authenticate to which client
    principals.

  - KDB modules can use the client and KDC interface IP addresses to
    determine whether to allow an AS request.

  - GSS applications can query the bit strength of a krb5 GSS context
    using the GSS_C_SEC_CONTEXT_SASL_SSF OID with
    gss_inquire_sec_context_by_oid().

  - GSS applications can query the impersonator name of a krb5 GSS
    credential using the GSS_KRB5_GET_CRED_IMPERSONATOR OID with
    gss_inquire_cred_by_oid().

  - kdcpreauth modules can query the KDC for the canonicalized
    requested client principal name, or match a principal name against
    the requested client principal name with canonicalization.

* Protocol evolution:

  - The client library will continue to try pre-authentication
    mechanisms after most failure conditions.

  - The KDC will issue trivially renewable tickets (where the
    renewable lifetime is equal to or less than the ticket lifetime)
    if requested by the client, to be friendlier to scripts.

  - The client library will use a random nonce for TGS requests
    instead of the current system time.

  - For the RC4 string-to-key or PAC operations, UTF-16 is supported
    (previously only UCS-2 was supported).

  - When matching PKINIT client certificates, UPN SANs will be matched
    correctly as UPNs, with canonicalization.

* User experience:

  - Dates after the year 2038 are accepted (provided that the platform
    time facilities support them), through the year 2106.

  - Automatic credential cache selection based on the client realm
    will take into account the fallback realm and the service
    hostname.

  - Referral and alternate cross-realm TGTs will not be cached,
    avoiding some scenarios where they can be added to the credential
    cache multiple times.

  - A German translation has been added.

* Code quality:

  - The build is warning-clean under clang with the configured warning
    options.

  - The automated test suite runs cleanly under AddressSanitizer.

Release 1.17

* Administrator experience:

  - A new Kerberos database module using the Lightning Memory-Mapped
    Database library (LMDB) has been added.  The LMDB KDB module
    should be more performant and more robust than the DB2 module, and
    may become the default module for new databases in a future
    release.

  - "kdb5_util dump" will no longer dump policy entries when specific
    principal names are requested.

* Developer experience:

  - The new krb5_get_etype_info() API can be used to retrieve enctype,
    salt, and string-to-key parameters from the KDC for a client
    principal.

  - The new GSS_KRB5_NT_ENTERPRISE_NAME name type allows enterprise
    principal names to be used with GSS-API functions.

  - KDC and kadmind modules which call com_err() will now write to the
    log file in a format more consistent with other log messages.

  - Programs which use large numbers of memory credential caches
    should perform better.

* Protocol evolution:

  - The SPAKE pre-authentication mechanism is now supported.  This
    mechanism protects against password dictionary attacks without
    requiring any additional infrastructure such as certificates.
    SPAKE is enabled by default on clients, but must be manually
    enabled on the KDC for this release.

  - PKINIT freshness tokens are now supported.  Freshness tokens can
    protect against scenarios where an attacker uses temporary access
    to a smart card to generate authentication requests for the
    future.

  - Password change operations now prefer TCP over UDP, to avoid
    spurious error messages about replays when a response packet is
    dropped.

  - The KDC now supports cross-realm S4U2Self requests when used with
    a third-party KDB module such as Samba's.  The client code for
    cross-realm S4U2Self requests is also now more robust.

* User experience:

  - The new ktutil addent -f flag can be used to fetch salt
    information from the KDC for password-based keys.

  - The new kdestroy -p option can be used to destroy a credential
    cache within a collection by client principal name.

  - The Kerberos man page has been restored, and documents the
    environment variables that affect programs using the Kerberos
    library.

* Code quality:

  - Python test scripts now use Python 3.

  - Python test scripts now display markers in verbose output, making
    it easier to find where a failure occurred within the scripts.

  - The Windows build system has been simplified and updated to work
    with more recent versions of Visual Studio.  A large volume of
    unused Windows-specific code has been removed.  Visual Studio 2013
    or later is now required.

Release 1.18

* Administrator experience:

  - Remove support for single-DES encryption types.

  - Change the replay cache format to be more efficient and robust.
    Replay cache filenames using the new format end with ``.rcache2``
    by default.

  - setuid programs will automatically ignore environment variables
    that normally affect krb5 API functions, even if the caller does
    not use krb5_init_secure_context().

  - Add an ``enforce_ok_as_delegate`` krb5.conf relation to disable
    credential forwarding during GSSAPI authentication unless the KDC
    sets the ok-as-delegate bit in the service ticket.

* Developer experience:

  - Implement krb5_cc_remove_cred() for all credential cache types.

  - Add the krb5_pac_get_client_info() API to get the client account
    name from a PAC.

* Protocol evolution:

  - Add KDC support for S4U2Self requests where the user is identified
    by X.509 certificate.  (Requires support for certificate lookup
    from a third-party KDB module.)

  - Remove support for an old ("draft 9") variant of PKINIT.

  - Add support for Microsoft NegoEx.  (Requires one or more
    third-party GSS modules implementing NegoEx mechanisms.)

* User experience:

  - Add support for ``dns_canonicalize_hostname=fallback``, causing
    host-based principal names to be tried first without DNS
    canonicalization, and again with DNS canonicalization if the
    un-canonicalized server is not found.

  - Expand single-component hostnames in hhost-based principal names
    when DNS canonicalization is not used, adding the system's first
    DNS search path as a suffix.  Add a ``qualify_shortname``
    krb5.conf relation to override this suffix or disable expansion.

* Code quality:

  - The libkrb5 serialization code (used to export and import krb5 GSS
    security contexts) has been simplified and made type-safe.

  - The libkrb5 code for creating KRB-PRIV, KRB-SAFE, and KRB-CRED
    messages has been revised to conform to current coding practices.

  - The test suite has been modified to work with macOS System
    Integrity Protection enabled.

  - The test suite incorporates soft-pkcs11 so that PKINIT PKCS11
    support can always be tested.

Release 1.19

* Administrator experience:

  - When a client keytab is present, the GSSAPI krb5 mech will refresh
    credentials even if the current credentials were acquired
    manually.

  - It is now harder to accidentally delete the K/M entry from a KDB.

* Developer experience:

  - gss_acquire_cred_from() now supports the "password" and "verify"
    options, allowing credentials to be acquired via password and
    verified using a keytab key.

  - When an application accepts a GSS security context, the new
    GSS_C_CHANNEL_BOUND_FLAG will be set if the initiator and acceptor
    both provided matching channel bindings.

  - Added the GSS_KRB5_NT_X509_CERT name type, allowing S4U2Self
    requests to identify the desired client principal by certificate.

  - PKINIT certauth modules can now cause the hw-authent flag to be
    set in issued tickets.

  - The krb5_init_creds_step() API will now issue the same password
    expiration warnings as krb5_get_init_creds_password().

* Protocol evolution:

  - Added client and KDC support for Microsoft's Resource-Based
    Constrained Delegation, which allows cross-realm S4U2Proxy
    requests.  A third-party database module is required for KDC
    support.

  - kadmin/admin is now the preferred server principal name for kadmin
    connections, and the host-based form is no longer created by
    default.  The client will still try the host-based form as a
    fallback.

  - Added client and server support for Microsoft's
    KERB_AP_OPTIONS_CBT extension, which causes channel bindings to be
    required for the initiator if the acceptor provided them.  The
    client will send this option if the client_aware_gss_bindings
    profile option is set.

User experience:

  - The default setting of dns_canonicalize_realm is now "fallback".
    Hostnames provided from applications will be tried in principal
    names as given (possibly with shortname qualification), falling
    back to the canonicalized name.

  - kinit will now issue a warning if the des3-cbc-sha1 encryption
    type is used in the reply.  This encryption type will be
    deprecated and removed in future releases.

  - Added kvno flags --out-cache, --no-store, and --cached-only
    (inspired by Heimdal's kgetcred).

Release 1.20

* Administrator experience:

  - Added a "disable_pac" realm relation to suppress adding PAC
    authdata to tickets, for realms which do not need to support S4U
    requests.

  - Most credential cache types will use atomic replacement when a
    cache is reinitialized using kinit or refreshed from the client
    keytab.

  - kprop can now propagate databases with a dump size larger than
    4GB, if both the client and server are upgraded.

  - kprop can now work over NATs that change the destination IP
    address, if the client is upgraded.

* Developer experience:

  - Updated the KDB interface.  The sign_authdata() method is replaced
    with the issue_pac() method, allowing KDB modules to add logon
    info and other buffers to the PAC issued by the KDC.

  - Host-based initiator names are better supported in the GSS krb5
    mechanism.

* Protocol evolution:

  - Replaced AD-SIGNEDPATH authdata with minimal PACs.

  - To avoid spurious replay errors, password change requests will not
    be attempted over UDP until the attempt over TCP fails.

  - PKINIT will sign its CMS messages with SHA-256 instead of SHA-1.

* Code quality:

  - Updated all code using OpenSSL to be compatible with OpenSSL 3.

  - Reorganized the libk5crypto build system to allow the OpenSSL
    back-end to pull in material from the builtin back-end depending
    on the OpenSSL version.

  - Simplified the PRNG logic to always use the platform PRNG.

  - Converted the remaining Tcl tests to Python.

Release 1.21

* User experience:

  - Added a credential cache type providing compatibility with the
    macOS 11 native credential cache.

* Developer experience:

  - libkadm5 will use the provided krb5_context object to read
    configuration values, instead of creating its own.

  - Added an interface to retrieve the ticket session key from a GSS
    context.

* Protocol evolution:

  - The KDC will no longer issue tickets with RC4 or triple-DES
    session keys unless explicitly configured with the new allow_rc4
    or allow_des3 variables respectively.

  - The KDC will assume that all services can handle aes256-sha1
    session keys unless the service principal has a session_enctypes
    string attribute.

  - Support for PAC full KDC checksums has been added to mitigate an
    S4U2Proxy privilege escalation attack.

  - The PKINIT client will advertise a more modern set of supported
    CMS algorithms.

* Code quality:

  - Removed unused code in libkrb5, libkrb5support, and the PKINIT
    module.

  - Modernized the KDC code for processing TGS requests, the code for
    encrypting and decrypting key data, the PAC handling code, and the
    GSS library packet parsing and composition code.

  - Improved the test framework's detection of memory errors in daemon
    processes when used with asan.

`Pre-authentication mechanisms`

- PW-SALT                                         :rfc:`4120#section-5.2.7.3`
- ENC-TIMESTAMP                                   :rfc:`4120#section-5.2.7.2`
- SAM-2
- FAST negotiation framework   (release 1.8)      :rfc:`6113`
- PKINIT with FAST on client   (release 1.10)     :rfc:`6113`
- PKINIT                                          :rfc:`4556`
- FX-COOKIE                                       :rfc:`6113#section-5.2`
- S4U-X509-USER                (release 1.8)      https://msdn.microsoft.com/en-us/library/cc246091
- OTP                          (release 1.12)     :ref:`otp_preauth`
- SPAKE                        (release 1.17)     :ref:`spake`
