.. highlight:: rst

.. toctree::
    :hidden:

    mitK5license.rst

.. _mitK5features:

MIT Kerberos features
=====================

http://web.mit.edu/kerberos


Quick facts
-----------

License - :ref:`mitK5license`

Releases:
    - Latest stable: http://web.mit.edu/kerberos/krb5-1.15/
    - Supported: http://web.mit.edu/kerberos/krb5-1.14/
    - Release cycle: 9 -- 12 months

Supported platforms \/ OS distributions:
    - Windows (KfW 4.0): Windows 7, Vista, XP
    - Solaris: SPARC, x86_64/x86
    - GNU/Linux: Debian x86_64/x86, Ubuntu x86_64/x86, RedHat x86_64/x86
    - BSD: NetBSD x86_64/x86

Crypto backends:
    - builtin - MIT Kerberos native crypto library
    - OpenSSL (1.0\+) - http://www.openssl.org

Database backends: LDAP, DB2

krb4 support: Kerberos 5 release < 1.8

DES support: configurable (See :ref:`retiring-des`)

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

For more information on the specific project see http://k5wiki.kerberos.org/wiki/Projects

Release 1.7
 -   Credentials delegation                   :rfc:`5896`
 -   Cross-realm authentication and referrals :rfc:`6806`
 -   Master key migration
 -   PKINIT                                   :rfc:`4556` :ref:`pkinit`

Release 1.8
 -   Anonymous PKINIT         :rfc:`6112` :ref:`anonymous_pkinit`
 -   Constrained delegation
 -   IAKERB                   http://tools.ietf.org/html/draft-ietf-krb-wg-iakerb-02
 -   Heimdal bridge plugin for KDC backend
 -   GSS-API S4U extensions   http://msdn.microsoft.com/en-us/library/cc246071
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
 -   GSS-API extentions for SASL GS2 bridge    :rfc:`5801` :rfc:`5587`
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
 -   Principal may refer to nonexistent policies `Policy Refcount project <http://k5wiki.kerberos.org/wiki/Projects/Policy_refcount_elimination>`_
 -   Support for having no long-term keys for a principal `Principals Without Keys project <http://k5wiki.kerberos.org/wiki/Projects/Principals_without_keys>`_
 -   Collection support to the KEYRING credential cache type on Linux :ref:`ccache_definition`
 -   FAST OTP preauthentication module for the KDC which uses RADIUS to validate OTP token values :ref:`otp_preauth`
 -   Experimental Audit plugin for KDC processing `Audit project <http://k5wiki.kerberos.org/wiki/Projects/Audit>`_

Release 1.13

 -   Add support for accessing KDCs via an HTTPS proxy server using
     the `MS-KKDCP
     <http://msdn.microsoft.com/en-us/library/hh553774.aspx>`_
     protocol.
 -   Add support for `hierarchical incremental propagation
     <http://k5wiki.kerberos.org/wiki/Projects/Hierarchical_iprop>`_,
     where slaves can act as intermediates between an upstream master
     and other downstream slaves.
 -   Add support for configuring GSS mechanisms using
     ``/etc/gss/mech.d/*.conf`` files in addition to
     ``/etc/gss/mech``.
 -   Add support to the LDAP KDB module for `binding to the LDAP
     server using SASL
     <http://k5wiki.kerberos.org/wiki/Projects/LDAP_SASL_support>`_.
 -   The KDC listens for TCP connections by default.
 -   Fix a minor key disclosure vulnerability where using the
     "keepold" option to the kadmin randkey operation could return the
     old keys. `[CVE-2014-5351]
     <http://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2014-5351>`_
 -   Add client support for the Kerberos Cache Manager protocol. If
     the host is running a Heimdal kcm daemon, caches served by the
     daemon can be accessed with the KCM: cache type.
 -   When built on OS X 10.7 and higher, use "KCM:" as the default
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

   - On slave KDCs, poll the master KDC immediately after processing a
     full resync, and do not require two full resyncs after the master
     KDC's log file is reset.

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
    and UDP servers and master KDC status in a single DNS lookup, and
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

`Pre-authentication mechanisms`

- PW-SALT                                         :rfc:`4120#section-5.2.7.3`
- ENC-TIMESTAMP                                   :rfc:`4120#section-5.2.7.2`
- SAM-2
- FAST negotiation framework   (release 1.8)      :rfc:`6113`
- PKINIT with FAST on client   (release 1.10)     :rfc:`6113`
- PKINIT                                          :rfc:`4556`
- FX-COOKIE                                       :rfc:`6113#section-5.2`
- S4U-X509-USER                (release 1.8)      http://msdn.microsoft.com/en-us/library/cc246091
- OTP                          (release 1.12)     :ref:`otp_preauth`

`PRNG`

- modularity       (release 1.9)
- Yarrow PRNG      (release < 1.10)
- Fortuna PRNG     (release 1.9)       http://www.schneier.com/book-practical.html
- OS PRNG          (release 1.10)      OS's native PRNG
