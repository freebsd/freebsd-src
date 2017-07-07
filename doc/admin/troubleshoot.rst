.. _troubleshoot:

Troubleshooting
===============

.. _trace_logging:

Trace logging
-------------

Most programs using MIT krb5 1.9 or later can be made to provide
information about internal krb5 library operations using trace
logging.  To enable this, set the **KRB5_TRACE** environment variable
to a filename before running the program.  On many operating systems,
the filename ``/dev/stdout`` can be used to send trace logging output
to standard output.

Some programs do not honor **KRB5_TRACE**, either because they use
secure library contexts (this generally applies to setuid programs and
parts of the login system) or because they take direct control of the
trace logging system using the API.

Here is a short example showing trace logging output for an invocation
of the :ref:`kvno(1)` command::

    shell% env KRB5_TRACE=/dev/stdout kvno krbtgt/KRBTEST.COM
    [9138] 1332348778.823276: Getting credentials user@KRBTEST.COM ->
        krbtgt/KRBTEST.COM@KRBTEST.COM using ccache
        FILE:/me/krb5/build/testdir/ccache
    [9138] 1332348778.823381: Retrieving user@KRBTEST.COM ->
        krbtgt/KRBTEST.COM@KRBTEST.COM from
        FILE:/me/krb5/build/testdir/ccache with result: 0/Unknown code 0
    krbtgt/KRBTEST.COM@KRBTEST.COM: kvno = 1


List of errors
--------------

Frequently seen errors
~~~~~~~~~~~~~~~~~~~~~~

#. :ref:`init_creds_ETYPE_NOSUPP`

#. :ref:`cert_chain_ETYPE_NOSUPP`

#. :ref:`err_cert_chain_cert_expired`


Errors seen by admins
~~~~~~~~~~~~~~~~~~~~~

.. _prop_failed_start:

#. :ref:`kprop_no_route`

#. :ref:`kprop_con_refused`

#. :ref:`kprop_sendauth_exchange`

.. _prop_failed_end:

-----

.. _init_creds_etype_nosupp:

KDC has no support for encryption type while getting initial credentials
........................................................................

.. _cert_chain_etype_nosupp:


credential verification failed: KDC has no support for encryption type
......................................................................

This most commonly happens when trying to use a principal with only
DES keys, in a release (MIT krb5 1.7 or later) which disables DES by
default.  DES encryption is considered weak due to its inadequate key
size.  If you cannot migrate away from its use, you can re-enable DES
by adding ``allow_weak_crypto = true`` to the :ref:`libdefaults`
section of :ref:`krb5.conf(5)`.


.. _err_cert_chain_cert_expired:

Cannot create cert chain: certificate has expired
.................................................

This error message indicates that PKINIT authentication failed because
the client certificate, KDC certificate, or one of the certificates in
the signing chain above them has expired.

If the KDC certificate has expired, this message appears in the KDC
log file, and the client will receive a "Preauthentication failed"
error.  (Prior to release 1.11, the KDC log file message erroneously
appears as "Out of memory".  Prior to release 1.12, the client will
receive a "Generic error".)

If the client or a signing certificate has expired, this message may
appear in trace_logging_ output from :ref:`kinit(1)` or, starting in
release 1.12, as an error message from kinit or another program which
gets initial tickets.  The error message is more likely to appear
properly on the client if the principal entry has no long-term keys.

.. _kprop_no_route:

kprop: No route to host while connecting to server
..................................................

Make sure that the hostname of the slave (as given to kprop) is
correct, and that any firewalls between the master and the slave allow
a connection on port 754.

.. _kprop_con_refused:

kprop: Connection refused while connecting to server
....................................................

If the slave is intended to run kpropd out of inetd, make sure that
inetd is configured to accept krb5_prop connections.  inetd may need
to be restarted or sent a SIGHUP to recognize the new configuration.
If the slave is intended to run kpropd in standalone mode, make sure
that it is running.

.. _kprop_sendauth_exchange:

kprop: Server rejected authentication (during sendauth exchange) while authenticating to server
...............................................................................................

Make sure that:

#. The time is synchronized between the master and slave KDCs.
#. The master stash file was copied from the master to the expected
   location on the slave.
#. The slave has a keytab file in the default location containing a
   ``host`` principal for the slave's hostname.
