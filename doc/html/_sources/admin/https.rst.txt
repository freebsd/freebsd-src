.. _https:

HTTPS proxy configuration
=========================

In addition to being able to use UDP or TCP to communicate directly
with a KDC as is outlined in RFC4120, and with kpasswd services in a
similar fashion, the client libraries can attempt to use an HTTPS
proxy server to communicate with a KDC or kpasswd service, using the
protocol outlined in [MS-KKDCP].

Communicating with a KDC through an HTTPS proxy allows clients to
contact servers when network firewalls might otherwise prevent them
from doing so.  The use of TLS also encrypts all traffic between the
clients and the KDC, preventing observers from conducting password
dictionary attacks or from observing the client and server principals
being authenticated, at additional computational cost to both clients
and servers.

An HTTPS proxy server is provided as a feature in some versions of
Microsoft Windows Server, and a WSGI implementation named `kdcproxy`
is available in the python package index.


Configuring the clients
-----------------------

To use an HTTPS proxy, a client host must trust the CA which issued
that proxy's SSL certificate.  If that CA's certificate is not in the
system-wide default set of trusted certificates, configure the
following relation in the client host's :ref:`krb5.conf(5)` file in
the appropriate :ref:`realms` subsection::

    http_anchors = FILE:/etc/krb5/cacert.pem

Adjust the pathname to match the path of the file which contains a
copy of the CA's certificate.  The `http_anchors` option is documented
more fully in :ref:`krb5.conf(5)`.

Configure the client to access the KDC and kpasswd service by
specifying their locations in its :ref:`krb5.conf(5)` file in the form
of HTTPS URLs for the proxy server::

    kdc = https://server.fqdn/KdcProxy
    kpasswd_server = https://server.fqdn/KdcProxy

If the proxy and client are properly configured, client commands such
as ``kinit``, ``kvno``, and ``kpasswd`` should all function normally.
