.. _pkinit:

PKINIT configuration
====================

PKINIT is a preauthentication mechanism for Kerberos 5 which uses
X.509 certificates to authenticate the KDC to clients and vice versa.
PKINIT can also be used to enable anonymity support, allowing clients
to communicate securely with the KDC or with application servers
without authenticating as a particular client principal.


Creating certificates
---------------------

PKINIT requires an X.509 certificate for the KDC and one for each
client principal which will authenticate using PKINIT.  For anonymous
PKINIT, a KDC certificate is required, but client certificates are
not.  A commercially issued server certificate can be used for the KDC
certificate, but generally cannot be used for client certificates.

The instruction in this section describe how to establish a
certificate authority and create standard PKINIT certificates.  Skip
this section if you are using a commercially issued server certificate
as the KDC certificate for anonymous PKINIT, or if you are configuring
a client to use an Active Directory KDC.


Generating a certificate authority certificate
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can establish a new certificate authority (CA) for use with a
PKINIT deployment with the commands::

    openssl genrsa -out cakey.pem 2048
    openssl req -key cakey.pem -new -x509 -out cacert.pem -days 3650

The second command will ask for the values of several certificate
fields.  These fields can be set to any values.  You can adjust the
expiration time of the CA certificate by changing the number after
``-days``.  Since the CA certificate must be deployed to client
machines each time it changes, it should normally have an expiration
time far in the future; however, expiration times after 2037 may cause
interoperability issues in rare circumstances.

The result of these commands will be two files, cakey.pem and
cacert.pem.  cakey.pem will contain a 2048-bit RSA private key, which
must be carefully protected.  cacert.pem will contain the CA
certificate, which must be placed in the filesytems of the KDC and
each client host.  cakey.pem will be required to create KDC and client
certificates.


Generating a KDC certificate
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A KDC certificate for use with PKINIT is required to have some unusual
fields, which makes generating them with OpenSSL somewhat complicated.
First, you will need a file containing the following::

    [kdc_cert]
    basicConstraints=CA:FALSE
    keyUsage=nonRepudiation,digitalSignature,keyEncipherment,keyAgreement
    extendedKeyUsage=1.3.6.1.5.2.3.5
    subjectKeyIdentifier=hash
    authorityKeyIdentifier=keyid,issuer
    issuerAltName=issuer:copy
    subjectAltName=otherName:1.3.6.1.5.2.2;SEQUENCE:kdc_princ_name

    [kdc_princ_name]
    realm=EXP:0,GeneralString:${ENV::REALM}
    principal_name=EXP:1,SEQUENCE:kdc_principal_seq

    [kdc_principal_seq]
    name_type=EXP:0,INTEGER:1
    name_string=EXP:1,SEQUENCE:kdc_principals

    [kdc_principals]
    princ1=GeneralString:krbtgt
    princ2=GeneralString:${ENV::REALM}

If the above contents are placed in extensions.kdc, you can generate
and sign a KDC certificate with the following commands::

    openssl genrsa -out kdckey.pem 2048
    openssl req -new -out kdc.req -key kdckey.pem
    env REALM=YOUR_REALMNAME openssl x509 -req -in kdc.req \
        -CAkey cakey.pem -CA cacert.pem -out kdc.pem -days 365 \
        -extfile extensions.kdc -extensions kdc_cert -CAcreateserial
    rm kdc.req

The second command will ask for the values of certificate fields,
which can be set to any values.  In the third command, substitute your
KDC's realm name for YOUR_REALMNAME.  You can adjust the certificate's
expiration date by changing the number after ``-days``.  Remember to
create a new KDC certificate before the old one expires.

The result of this operation will be in two files, kdckey.pem and
kdc.pem.  Both files must be placed in the KDC's filesystem.
kdckey.pem, which contains the KDC's private key, must be carefully
protected.

If you examine the KDC certificate with ``openssl x509 -in kdc.pem
-text -noout``, OpenSSL will not know how to display the KDC principal
name in the Subject Alternative Name extension, so it will appear as
``othername:<unsupported>``.  This is normal and does not mean
anything is wrong with the KDC certificate.


Generating client certificates
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

PKINIT client certificates also must have some unusual certificate
fields.  To generate a client certificate with OpenSSL for a
single-component principal name, you will need an extensions file
(different from the KDC extensions file above) containing::

    [client_cert]
    basicConstraints=CA:FALSE
    keyUsage=digitalSignature,keyEncipherment,keyAgreement
    extendedKeyUsage=1.3.6.1.5.2.3.4
    subjectKeyIdentifier=hash
    authorityKeyIdentifier=keyid,issuer
    issuerAltName=issuer:copy
    subjectAltName=otherName:1.3.6.1.5.2.2;SEQUENCE:princ_name

    [princ_name]
    realm=EXP:0,GeneralString:${ENV::REALM}
    principal_name=EXP:1,SEQUENCE:principal_seq

    [principal_seq]
    name_type=EXP:0,INTEGER:1
    name_string=EXP:1,SEQUENCE:principals

    [principals]
    princ1=GeneralString:${ENV::CLIENT}

If the above contents are placed in extensions.client, you can
generate and sign a client certificate with the following commands::

    openssl genrsa -out clientkey.pem 2048
    openssl req -new -key clientkey.pem -out client.req
    env REALM=YOUR_REALMNAME CLIENT=YOUR_PRINCNAME openssl x509 \
        -CAkey cakey.pem -CA cacert.pem -req -in client.req \
        -extensions client_cert -extfile extensions.client \
        -days 365 -out client.pem
    rm client.req

Normally, the first two commands should be run on the client host, and
the resulting client.req file transferred to the certificate authority
host for the third command.  As in the previous steps, the second
command will ask for the values of certificate fields, which can be
set to any values.  In the third command, substitute your realm's name
for YOUR_REALMNAME and the client's principal name (without realm) for
YOUR_PRINCNAME.  You can adjust the certificate's expiration date by
changing the number after ``-days``.

The result of this operation will be two files, clientkey.pem and
client.pem.  Both files must be present on the client's host;
clientkey.pem, which contains the client's private key, must be
protected from access by others.

As in the KDC certificate, OpenSSL will display the client principal
name as ``othername:<unsupported>`` in the Subject Alternative Name
extension of a PKINIT client certificate.

If the client principal name contains more than one component
(e.g. ``host/example.com@REALM``), the ``[principals]`` section of
``extensions.client`` must be altered to contain multiple entries.
(Simply setting ``CLIENT`` to ``host/example.com`` would generate a
certificate for ``host\/example.com@REALM`` which would not match the
multi-component principal name.)  For a two-component principal, the
section should read::

    [principals]
    princ1=GeneralString:${ENV::CLIENT1}
    princ2=GeneralString:${ENV::CLIENT2}

The environment variables ``CLIENT1`` and ``CLIENT2`` must then be set
to the first and second components when running ``openssl x509``.


Configuring the KDC
-------------------

The KDC must have filesystem access to the KDC certificate (kdc.pem)
and the KDC private key (kdckey.pem).  Configure the following
relation in the KDC's :ref:`kdc.conf(5)` file, either in the
:ref:`kdcdefaults` section or in a :ref:`kdc_realms` subsection (with
appropriate pathnames)::

    pkinit_identity = FILE:/var/lib/krb5kdc/kdc.pem,/var/lib/krb5kdc/kdckey.pem

If any clients will authenticate using regular (as opposed to
anonymous) PKINIT, the KDC must also have filesystem access to the CA
certificate (cacert.pem), and the following configuration (with the
appropriate pathname)::

    pkinit_anchors = FILE:/var/lib/krb5kdc/cacert.pem

Because of the larger size of requests and responses using PKINIT, you
may also need to allow TCP access to the KDC::

    kdc_tcp_listen = 88

Restart the :ref:`krb5kdc(8)` daemon to pick up the configuration
changes.

The principal entry for each PKINIT-using client must be configured to
require preauthentication.  Ensure this with the command::

    kadmin -q 'modprinc +requires_preauth YOUR_PRINCNAME'

Starting with release 1.12, it is possible to remove the long-term
keys of a principal entry, which can save some space in the database
and help to clarify some PKINIT-related error conditions by not asking
for a password::

    kadmin -q 'purgekeys -all YOUR_PRINCNAME'

These principal options can also be specified at principal creation
time as follows::

    kadmin -q 'add_principal +requires_preauth -nokey YOUR_PRINCNAME'


Configuring the clients
-----------------------

Client hosts must be configured to trust the issuing authority for the
KDC certificate.  For a newly established certificate authority, the
client host must have filesystem access to the CA certificate
(cacert.pem) and the following relation in :ref:`krb5.conf(5)` in the
appropriate :ref:`realms` subsection (with appropriate pathnames)::

    pkinit_anchors = FILE:/etc/krb5/cacert.pem

If the KDC certificate is a commercially issued server certificate,
the issuing certificate is most likely included in a system directory.
You can specify it by filename as above, or specify the whole
directory like so::

    pkinit_anchors = DIR:/etc/ssl/certs

A commercially issued server certificate will usually not have the
standard PKINIT principal name or Extended Key Usage extensions, so
the following additional configuration is required::

    pkinit_eku_checking = kpServerAuth
    pkinit_kdc_hostname = hostname.of.kdc.certificate

Multiple **pkinit_kdc_hostname** relations can be configured to
recognize multiple KDC certificates.  If the KDC is an Active
Directory domain controller, setting **pkinit_kdc_hostname** is
necessary, but it should not be necessary to set
**pkinit_eku_checking**.

To perform regular (as opposed to anonymous) PKINIT authentication, a
client host must have filesystem access to a client certificate
(client.pem), and the corresponding private key (clientkey.pem).
Configure the following relations in the client host's
:ref:`krb5.conf(5)` file in the appropriate :ref:`realms` subsection
(with appropriate pathnames)::

    pkinit_identities = FILE:/etc/krb5/client.pem,/etc/krb5/clientkey.pem

If the KDC and client are properly configured, it should now be
possible to run ``kinit username`` without entering a password.


.. _anonymous_pkinit:

Anonymous PKINIT
----------------

Anonymity support in Kerberos allows a client to obtain a ticket
without authenticating as any particular principal.  Such a ticket can
be used as a FAST armor ticket, or to securely communicate with an
application server anonymously.

To configure anonymity support, you must generate or otherwise procure
a KDC certificate and configure the KDC host, but you do not need to
generate any client certificates.  On the KDC, you must set the
**pkinit_identity** variable to provide the KDC certificate, but do
not need to set the **pkinit_anchors** variable or store the issuing
certificate if you won't have any client certificates to verify.  On
client hosts, you must set the **pkinit_anchors** variable (and
possibly **pkinit_kdc_hostname** and **pkinit_eku_checking**) in order
to trust the issuing authority for the KDC certificate, but do not
need to set the **pkinit_identities** variable.

Anonymity support is not enabled by default.  To enable it, you must
create the principal ``WELLKNOWN/ANONYMOUS`` using the command::

    kadmin -q 'addprinc -randkey WELLKNOWN/ANONYMOUS'

Some Kerberos deployments include application servers which lack
proper access control, and grant some level of access to any user who
can authenticate.  In such an environment, enabling anonymity support
on the KDC would present a security issue.  If you need to enable
anonymity support for TGTs (for use as FAST armor tickets) without
enabling anonymous authentication to application servers, you can set
the variable **restrict_anonymous_to_tgt** to ``true`` in the
appropriate :ref:`kdc_realms` subsection of the KDC's
:ref:`kdc.conf(5)` file.

To obtain anonymous credentials on a client, run ``kinit -n``, or
``kinit -n @REALMNAME`` to specify a realm.  The resulting tickets
will have the client name ``WELLKNOWN/ANONYMOUS@WELLKNOWN:ANONYMOUS``.
