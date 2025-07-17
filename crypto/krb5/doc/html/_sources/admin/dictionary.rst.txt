.. _dictionary:

Addressing dictionary attack risks
==================================

Kerberos initial authentication is normally secured using the client
principal's long-term key, which for users is generally derived from a
password.  Using a pasword-derived long-term key carries the risk of a
dictionary attack, where an attacker tries a sequence of possible
passwords, possibly requiring much less effort than would be required
to try all possible values of the key.  Even if :ref:`password policy
objects <policies>` are used to force users not to pick trivial
passwords, dictionary attacks can sometimes be successful against a
significant fraction of the users in a realm.  Dictionary attacks are
not a concern for principals using random keys.

A dictionary attack may be online or offline.  An online dictionary
attack is performed by trying each password in a separate request to
the KDC, and is therefore visible to the KDC and also limited in speed
by the KDC's processing power and the network capacity between the
client and the KDC.  Online dictionary attacks can be mitigated using
:ref:`account lockout <lockout>`.  This measure is not totally
satisfactory, as it makes it easy for an attacker to deny access to a
client principal.

An offline dictionary attack is performed by obtaining a ciphertext
generated using the password-derived key, and trying each password
against the ciphertext.  This category of attack is invisible to the
KDC and can be performed much faster than an online attack.  The
attack will generally take much longer with more recent encryption
types (particularly the ones based on AES), because those encryption
types use a much more expensive string-to-key function.  However, the
best defense is to deny the attacker access to a useful ciphertext.
The required defensive measures depend on the attacker's level of
network access.

An off-path attacker has no access to packets sent between legitimate
users and the KDC.  An off-path attacker could gain access to an
attackable ciphertext either by making an AS request for a client
principal which does not have the **+requires_preauth** flag, or by
making a TGS request (after authenticating as a different user) for a
server principal which does not have the **-allow_svr** flag.  To
address off-path attackers, a KDC administrator should set those flags
on principals with password-derived keys::

    kadmin: add_principal +requires_preauth -allow_svr princname

An attacker with passive network access (one who can monitor packets
sent between legitimate users and the KDC, but cannot change them or
insert their own packets) can gain access to an attackable ciphertext
by observing an authentication by a user using the most common form of
preauthentication, encrypted timestamp.  Any of the following methods
can prevent dictionary attacks by attackers with passive network
access:

* Enabling :ref:`SPAKE preauthentication <spake>` (added in release
  1.17) on the KDC, and ensuring that all clients are able to support
  it.

* Using an :ref:`HTTPS proxy <https>` for communication with the KDC,
  if the attacker cannot monitor communication between the proxy
  server and the KDC.

* Using FAST, protecting the initial authentication with either a
  random key (such as a host key) or with :ref:`anonymous PKINIT
  <anonymous_pkinit>`.

An attacker with active network access (one who can inject or modify
packets sent between legitimate users and the KDC) can try to fool the
client software into sending an attackable ciphertext using an
encryption type and salt string of the attacker's choosing.  Any of the
following methods can prevent dictionary attacks by active attackers:

* Enabling SPAKE preauthentication and setting the
  **disable_encrypted_timestamp** variable to ``true`` in the
  :ref:`realms` subsection of the client configuration.

* Using an HTTPS proxy as described above, configured in the client's
  krb5.conf realm configuration.  If :ref:`KDC discovery
  <kdc_discovery>` is used to locate a proxy server, an active
  attacker may be able to use DNS spoofing to cause the client to use
  a different HTTPS server or to not use HTTPS.

* Using FAST as described above.

If :ref:`PKINIT <pkinit>` or :ref:`OTP <otp_preauth>` are used for
initial authentication, the principal's long-term keys are not used
and dictionary attacks are usually not a concern.
