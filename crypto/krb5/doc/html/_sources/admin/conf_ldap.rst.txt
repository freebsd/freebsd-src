.. _conf_ldap:

Configuring Kerberos with OpenLDAP back-end
===========================================


 1. Make sure the LDAP server is using local authentication
    (``ldapi://``) or TLS (``ldaps``).  See
    https://www.openldap.org/doc/admin/tls.html for instructions on
    configuring TLS support in OpenLDAP.

 2. Add the Kerberos schema file to the LDAP Server using the OpenLDAP
    LDIF file from the krb5 source directory
    (``src/plugins/kdb/ldap/libkdb_ldap/kerberos.openldap.ldif``).
    The following example uses local authentication::

       ldapadd -Y EXTERNAL -H ldapi:/// -f /path/to/kerberos.openldap.ldif

 3. Choose DNs for the :ref:`krb5kdc(8)` and :ref:`kadmind(8)` servers
    to bind to the LDAP server, and create them if necessary.  Specify
    these DNs with the **ldap_kdc_dn** and **ldap_kadmind_dn**
    directives in :ref:`kdc.conf(5)`.  The kadmind DN will also be
    used for administrative commands such as :ref:`kdb5_util(8)`.

    Alternatively, you may configure krb5kdc and kadmind to use SASL
    authentication to access the LDAP server; see the :ref:`dbmodules`
    relations **ldap_kdc_sasl_mech** and similar.

 4. Specify a location for the LDAP service password file by setting
    **ldap_service_password_file**.  Use ``kdb5_ldap_util stashsrvpw``
    to stash passwords for the KDC and kadmind DNs chosen above.  For
    example::

       kdb5_ldap_util stashsrvpw -f /path/to/service.keyfile cn=krbadmin,dc=example,dc=com

    Skip this step if you are using SASL authentication and the
    mechanism does not require a password.

 5. Choose a DN for the global Kerberos container entry (but do not
    create the entry at this time).  Specify this DN with the
    **ldap_kerberos_container_dn** directive in :ref:`kdc.conf(5)`.
    Realm container entries will be created underneath this DN.
    Principal entries may exist either underneath the realm container
    (the default) or in separate trees referenced from the realm
    container.

 6. Configure the LDAP server ACLs to enable the KDC and kadmin server
    DNs to read and write the Kerberos data.  If
    **disable_last_success** and **disable_lockout** are both set to
    true in the :ref:`dbmodules` subsection for the realm, then the
    KDC DN only requires read access to the Kerberos data.

    Sample access control information::

       access to dn.base=""
           by * read

       access to dn.base="cn=Subschema"
           by * read

       # Provide access to the realm container.
       access to dn.subtree= "cn=EXAMPLE.COM,cn=krbcontainer,dc=example,dc=com"
           by dn.exact="cn=kdc-service,dc=example,dc=com" write
           by dn.exact="cn=adm-service,dc=example,dc=com" write
           by * none

       # Provide access to principals, if not underneath the realm container.
       access to dn.subtree= "ou=users,dc=example,dc=com"
           by dn.exact="cn=kdc-service,dc=example,dc=com" write
           by dn.exact="cn=adm-service,dc=example,dc=com" write
           by * none

       access to *
           by * read

    If the locations of the container and principals or the DNs of the
    service objects for a realm are changed then this information
    should be updated.

 7. In :ref:`kdc.conf(5)`, make sure the following relations are set
    in the :ref:`dbmodules` subsection for the realm::

       db_library (set to ``kldap``)
       ldap_kerberos_container_dn
       ldap_kdc_dn
       ldap_kadmind_dn
       ldap_service_password_file
       ldap_servers

 8. Create the realm using :ref:`kdb5_ldap_util(8)`:

       kdb5_ldap_util create -subtrees ou=users,dc=example,dc=com -s

    Use the **-subtrees** option if the principals are to exist in a
    separate subtree from the realm container.  Before executing the
    command, make sure that the subtree mentioned above
    ``(ou=users,dc=example,dc=com)`` exists.  If the principals will
    exist underneath the realm container, omit the **-subtrees** option
    and do not worry about creating the principal subtree.

    For more information, refer to the section :ref:`ops_on_ldap`.

    The realm object is created under the
    **ldap_kerberos_container_dn** specified in the configuration
    file.  This operation will also create the Kerberos container, if
    not present already.  This container can be used to store
    information related to multiple realms.

 9. Add an ``eq`` index for ``krbPrincipalName`` to speed up principal
    lookup operations.  See
    https://www.openldap.org/doc/admin/tuning.html#Indexes for
    details.

With the LDAP back end it is possible to provide aliases for principal
entries.  Currently we provide no administrative utilities for
creating aliases, so it must be done by direct manipulation of the
LDAP entries.

An entry with aliases contains multiple values of the
*krbPrincipalName* attribute.  Since LDAP attribute values are not
ordered, it is necessary to specify which principal name is canonical,
by using the *krbCanonicalName* attribute.  Therefore, to create
aliases for an entry, first set the *krbCanonicalName* attribute of
the entry to the canonical principal name (which should be identical
to the pre-existing *krbPrincipalName* value), and then add additional
*krbPrincipalName* attributes for the aliases.

Principal aliases are only returned by the KDC when the client
requests canonicalization.  Canonicalization is normally requested for
service principals; for client principals, an explicit flag is often
required (e.g., ``kinit -C``) and canonicalization is only performed
for initial ticket requests.
