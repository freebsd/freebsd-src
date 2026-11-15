libsecureboot
*************

This library depends one way or another on verifying detached digital
signatures. 

To do that, the necessary trust anchors need to be available.

The simplest (and most attractive for an embedded system) is to
capture them in this library.

The makefile ``local.trust.mk`` is responsible for doing that.
The file provided is just an example and depends on the environment
here at Juniper.

We expect ``local.trust.mk`` to set ``VE_SIGNATURE_LIST`` to a set of
tuples as below, in order of preference.
Each entry contains sig/alg/hash/flags:

	r3sig/RSA/sha384/0
	esig/ECDSA/sha256/1
	acc/OPENPGP/sha256/0

If using a signing server (such as that referenced below), it
may instead define ``VE_SIGN_URL_LIST`` which is the same
as the above, but with an extra element for the url to
contact.  Thus, ``VE_SIGNATURE_LIST`` can be derrived from
``${VE_SIGN_URL_LIST:H}``.

From ``VE_SIGNATURE_LIST`` we derrive:

``VE_SUPPORTED_LIST``

	a list of hash and signature methods to support.
	Each results in ``-DVE_<thing>_SUPPORT`` being added to ``CFLAGS``.

``VE_SIGNATURE_EXT_LIST``

	A list of signature extensions in order of preference.
	This is used to populate ``signature_ext_list`` used by
	``verify_signed()`` to find the most preferred signature
	available.

Finally use ``VE_SIGNATURE_LIST`` to populate ``signature_info_list``
used by ``verify_sig()`` to lookup the ``signature_info``, 
which provides the hash to use and whether the hash should be hashed
again.

Within Juniper/HPE we use signing servers, which apart from signing things
provide access to the necessary trust anchors.
That signing server is freely available - see
http://www.crufty.net/sjg/docs/signing-server.htm

The entry above for ``esig`` has non-zero ``flags`` to indicate
that the hash needs to re-hashed.
This is to deal with a quirk of our orignal ECDSA signature.  Since
the signing sever expects to receive a hash, and the signature methods
insist on hashing the input, the data is actually hashed twice.  Since
then, I added a FakeHash class to avoid that.

X.509 certificate chains offer a lot of flexibility over time and are
a great solution for an embedded vendor like Juniper or even
FreeBSD.org, but are probably overkill for personal or small site use.

Setting up a CA for this is rather involved, so I'll just provide a
link to a suitable tutorial below.

Using OpenPGP is much simpler.


OpenPGP
========

This is very simple to setup and use.

An RSA key pair can be generated with::

	GNUPGHOME=$PWD/.gnupg gpg --openpgp \
	--quick-generate-key --batch --passphrase '' "keyname" RSA

The use of ``GNUPGHOME=$PWD/.gnupg`` just avoids messing with personal
keyrings.
We can list the resulting key::

	GNUPGHOME=$PWD/.gnupg gpg --openpgp --list-keys

	gpg: WARNING: unsafe permissions on homedir
	'/h/sjg/openpgp/.gnupg'
	gpg: Warning: using insecure memory!
	/h/sjg/openpgp/.gnupg/pubring.kbx
	---------------------------------
	pub   rsa2048 2018-03-26 [SC] [expires: 2020-03-25]
	      AB39B111E40DD019E0E7C171ACA72B4719FD2523
	      uid           [ultimate] OpenPGPtest

The ``keyID`` we want later will be the last 8 octets
(``ACA72B4719FD2523``)
This is what we will use for looking up the key.

We can then export the private and public keys::

	GNUPGHOME=$PWD/.gnupg gpg --openpgp \
	--export --armor > ACA72B4719FD2523.pub.asc
	GNUPGHOME=$PWD/.gnupg gpg --openpgp \
	--export-secret-keys --armor > ACA72B4719FD2523.sec.asc

The public key ``ACA72B4719FD2523.pub.asc`` is what we want to
embed in this library.
If you look at the ``ta_asc.h`` target in ``openpgp/Makefile.inc``
we want the trust anchor in a file named ``t*.asc``
eg. ``ta_openpgp.asc``.

The ``ta_asc.h`` target will capture all such ``t*.asc`` into that
header.

Signatures
----------

We expect ascii armored (``.asc``) detached signatures
Eg.::

	gpg -a --detach-sign manifest

should produce the expected signature in ``manifest.asc``

We only support version 4 signatures using RSA (the default for ``gpg``).


OpenSSL
========

The basic idea here is to setup a private CA.

There are lots of good tutorials on available on this topic;
just google *setup openssl ca*.
A good example is https://jamielinux.com/docs/openssl-certificate-authority/

All we need for this library is a copy of the PEM encoded root CA
certificate (trust anchor).  This is expected to be in a file named
``t*.pem`` eg. ``ta_rsa.pem``.

The ``ta.h`` target in ``Makefile.inc`` will combine all such
``t*.pem`` files into that header.

Signatures
----------

For Junos we currently use EC DSA signatures with file extension
``.esig`` so the signature for ``manifest`` would be ``manifest.esig``

This was the first signature method we used with the remote signing
servers and it ends up being a signature of a hash.
Ie. client sends a hash which during signing gets hashed again.
So for Junos we define VE_ECDSA_HASH_AGAIN which causes ``verify_ec``
to hash again.

Later I added a FakeHash class to the signing server so we could
generate signatures compatible with our previous RSA scheme and
others.

Otherwise our EC DSA and RSA signatures are the default used by
OpenSSL - an original design goal was that a customer could verify our
signatures using nothing but an ``openssl`` binary.


Self tests
==========

If you want the ``loader`` to perform self-test of a given signature
verification method on startup (a must for FIPS 140-2 certification)
you need to provide a suitable file signed by each supported trust
anchor.

These should be stored in files with names that start with ``v`` and
have the same extension as the corresponding trust anchor.
Eg. for ``ta_openpgp.asc`` we use ``vc_openpgp.asc``
and for ``ta_rsa.pem`` we use ``vc_rsa.pem``.

Note for the X.509 case we simply extract the 2nd last certificate
from the relevant chain - which is sure to be a valid certificate
signed by the corresponding trust anchor.

--------------------
