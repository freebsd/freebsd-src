.. _ldap_be_ubuntu:

LDAP backend on Ubuntu 10.4 (lucid)
===================================

Setting up Kerberos v1.9 with LDAP backend on Ubuntu 10.4 (Lucid Lynx)


Prerequisites
-------------

Install the following packages: *slapd, ldap-utils* and *libldap2-dev*

You can install the necessary packages with these commands::

    sudo apt-get install slapd
    sudo apt-get install ldap-utils
    sudo apt-get install libldap2-dev

Extend the user schema using schemas from standart OpenLDAP
distribution: *cosine, mics, nis, inetcomperson* ::

    ldapadd -Y EXTERNAL -H ldapi:/// -f /etc/ldap/schema/cosine.ldif
    ldapadd -Y EXTERNAL -H ldapi:/// -f /etc/ldap/schema/mics.ldif
    ldapadd -Y EXTERNAL -H ldapi:/// -f /etc/ldap/schema/nis.ldif
    ldapadd -Y EXTERNAL -H ldapi:/// -f /etc/ldap/schema/inetcomperson.ldif


Building Kerberos from source
-----------------------------

::

    ./configure --with-ldap
    make
    sudo make install


Setting up Kerberos
-------------------

Configuration
~~~~~~~~~~~~~

Update kdc.conf with the LDAP back-end information::

    [realms]
        EXAMPLE.COM = {
            database_module = LDAP
        }

    [dbmodules]
        LDAP = {
            db_library = kldap
            ldap_kerberos_container_dn = cn=krbContainer,dc=example,dc=com
            ldap_kdc_dn = cn=admin,dc=example,dc=com
            ldap_kadmind_dn = cn=admin,dc=example,dc=com
            ldap_service_password_file = /usr/local/var/krb5kdc/admin.stash
            ldap_servers = ldapi:///
        }


Schema
~~~~~~

From the source tree copy
``src/plugins/kdb/ldap/libkdb_ldap/kerberos.schema`` into
``/etc/ldap/schema``

Warning: this step should be done after slapd is installed to avoid
problems with slapd installation.

To convert kerberos.schema to run-time configuration (``cn=config``)
do the following:

#. Create a temporary file ``/tmp/schema_convert.conf`` with the
   following content::

       include /etc/ldap/schema/kerberos.schema

#. Create a temporary directory ``/tmp/krb5_ldif``.

#. Run::

       slaptest -f /tmp/schema_convert.conf -F /tmp/krb5_ldif

   This should in a new file named
   ``/tmp/krb5_ldif/cn=config/cn=schema/cn={0}kerberos.ldif``.

#. Edit ``/tmp/krb5_ldif/cn=config/cn=schema/cn={0}kerberos.ldif`` by
   replacing the lines::

       dn: cn={0}kerberos
       cn: {0}kerberos

   with

       dn: cn=kerberos,cn=schema,cn=config
       cn: kerberos

   Also, remove following attribute-value pairs::

       structuralObjectClass: olcSchemaConfig
       entryUUID: ...
       creatorsName: cn=config
       createTimestamp: ...
       entryCSN: ...
       modifiersName: cn=config
       modifyTimestamp: ...

#. Load the new schema with ldapadd (with the proper authentication)::

       ldapadd -Y EXTERNAL -H ldapi:/// -f  /tmp/krb5_ldif/cn=config/cn=schema/cn={0}kerberos.ldif

   which should result the message ``adding new entry
   "cn=kerberos,cn=schema,cn=config"``.


Create Kerberos database
------------------------

Using LDAP administrator credentials, create Kerberos database and
master key stash::

    kdb5_ldap_util -D cn=admin,dc=example,dc=com -H ldapi:/// create -s

Stash the LDAP administrative passwords::

    kdb5_ldap_util -D cn=admin,dc=example,dc=com -H ldapi:/// stashsrvpw cn=admin,dc=example,dc=com

Start :ref:`krb5kdc(8)`::

    krb5kdc

To destroy database run::

    kdb5_ldap_util -D cn=admin,dc=example,dc=com -H ldapi:/// destroy -f


Useful references
-----------------

* `Kerberos and LDAP <https://help.ubuntu.com/10.04/serverguide/C/kerberos-ldap.html>`_
