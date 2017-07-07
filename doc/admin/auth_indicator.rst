.. _auth_indicator:

Authentication indicators
=========================

As of release 1.14, the KDC can be configured to annotate tickets if
the client authenticated using a stronger preauthentication mechanism
such as :ref:`PKINIT <pkinit>` or :ref:`OTP <otp_preauth>`.  These
annotations are called "authentication indicators."  Service
principals can be configured to require particular authentication
indicators in order to authenticate to that service.  An
authentication indicator value can be any string chosen by the KDC
administrator; there are no pre-set values.

To use authentication indicators with PKINIT or OTP, first configure
the KDC to include an indicator when that preauthentication mechanism
is used.  For PKINIT, use the **pkinit_indicator** variable in
:ref:`kdc.conf(5)`.  For OTP, use the **indicator** variable in the
token type definition, or specify the indicators in the **otp** user
string as described in :ref:`otp_preauth`.

To require an indicator to be present in order to authenticate to a
service principal, set the **require_auth** string attribute on the
principal to the indicator value to be required.  If you wish to allow
one of several indicators to be accepted, you can specify multiple
indicator values separated by spaces.

For example, a realm could be configured to set the authentication
indicator value "strong" when PKINIT is used to authenticate, using a
setting in the :ref:`kdc_realms` subsection::

    pkinit_indicator = strong

A service principal could be configured to require the "strong"
authentication indicator value::

    $ kadmin setstr host/high.value.server require_auth strong
    Password for user/admin@KRBTEST.COM:

A user who authenticates with PKINIT would be able to obtain a ticket
for the service principal::

    $ kinit -X X509_user_identity=FILE:/my/cert.pem,/my/key.pem user
    $ kvno host/high.value.server
    host/high.value.server@KRBTEST.COM: kvno = 1

but a user who authenticates with a password would not::

    $ kinit user
    Password for user@KRBTEST.COM:
    $ kvno host/high.value.server
    kvno: KDC policy rejects request while getting credentials for
      host/high.value.server@KRBTEST.COM

GSSAPI server applications can inspect authentication indicators
through the :ref:`auth-indicators <gssapi_authind_attr>` name
attribute.
