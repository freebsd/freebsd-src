..
    WHEN EDITING MAKE SURE EACH SENTENCE STARTS ON A NEW LINE

..
    IT HELPS RENDERERS TO DO THE RIGHT THING WRT SPACE

..
    IT HELPS PEOPLE DIFFING THE CHANGES

..
    WHEN EDITING MAKE SURE EACH SENTENCE STARTS ON A NEW LINE

..
    IT HELPS RENDERERS TO DO THE RIGHT THING WRT SPACE

..
    IT HELPS PEOPLE DIFFING THE CHANGES

..
    WHEN EDITING MAKE SURE EACH SENTENCE STARTS ON A NEW LINE

..
    IT HELPS RENDERERS TO DO THE RIGHT THING WRT SPACE

..
    IT HELPS PEOPLE DIFFING THE CHANGES

..
    WHEN EDITING MAKE SURE EACH SENTENCE STARTS ON A NEW LINE

..
    IT HELPS RENDERERS TO DO THE RIGHT THING WRT SPACE

..
    IT HELPS PEOPLE DIFFING THE CHANGES

.. program:: unbound-host

unbound-host(1)
===============

Synopsis
--------

**unbound-host** [``-C configfile``] [``-vdhr46D``] [``-c class``]
[``-t type``] [``-y key``] [``-f keyfile``] [``-F namedkeyfile``] hostname

Description
-----------

``unbound-host`` uses the Unbound validating resolver to query for the hostname
and display results.
With the :option:`-v` option it displays validation status: secure, insecure,
bogus (security failure).

By default it reads no configuration file whatsoever.
It attempts to reach the internet root servers.
With :option:`-C` an unbound config file and with :option:`-r` ``resolv.conf``
can be read.

The available options are:

.. option:: hostname

       This name is resolved (looked up in the DNS).
       If a IPv4 or IPv6 address is given, a reverse lookup is performed.

.. option:: -h

       Show the version and commandline option help.

.. option:: -v

       Enable verbose output and it shows validation results, on every line.
       Secure means that the NXDOMAIN (no such domain name), nodata (no such
       data) or positive data response validated correctly with one of the
       keys.
       Insecure means that that domain name has no security set up for it.
       Bogus (security failure) means that the response failed one or more
       checks, it is likely wrong, outdated, tampered with, or broken.

.. option:: -d

       Enable debug output to stderr.
       One :option:`-d` shows what the resolver and validator are doing and may
       tell you what is going on.
       More times, :option:`-d` :option:`-d`, gives a lot of output, with every
       packet sent and received.

.. option:: -c <class>

       Specify the class to lookup for, the default is IN the internet
       class.

.. option:: -t <type>

       Specify the type of data to lookup.
       The default looks for IPv4, IPv6 and mail handler data, or domain name
       pointers for reverse queries.

.. option:: -y <key>

       Specify a public key to use as trust anchor.
       This is the base for a chain of trust that is built up from the trust
       anchor to the response, in order to validate the response message.
       Can be given as a DS or DNSKEY record.
       For example:

       .. code-block:: text

            -y "example.com DS 31560 5 1 1CFED84787E6E19CCF9372C1187325972FE546CD"

.. option:: -D

       Enables DNSSEC validation.
       Reads the root anchor from the default configured root anchor at the
       default location, :file:`@UNBOUND_ROOTKEY_FILE@`.

.. option:: -f <keyfile>

       Reads keys from a file.
       Every line has a DS or DNSKEY record, in the format as for :option:`-y`.
       The zone file format, the same as ``dig`` and ``drill`` produce.

.. option:: -F <namedkeyfile>

       Reads keys from a BIND-style :file:`named.conf` file.
       Only the ``trusted-key {};`` entries are read.

.. option:: -C <configfile>

       Uses the specified unbound.conf to prime :doc:`libunbound(3)</manpages/libunbound>`.
       Pass it as first argument if you want to override some options from the
       config file with further arguments on the commandline.

.. option:: -r

       Read :file:`/etc/resolv.conf`, and use the forward DNS servers from
       there (those could have been set by DHCP).
       More info in *resolv.conf(5)*.
       Breaks validation if those servers do not support DNSSEC.

.. option:: -4

       Use solely the IPv4 network for sending packets.

.. option:: -6

       Use solely the IPv6 network for sending packets.

Examples
--------

Some examples of use.
The keys shown below are fakes, thus a security failure is encountered.

.. code-block:: text

       $ unbound-host www.example.com

       $ unbound-host -v -y "example.com DS 31560 5 1 1CFED84787E6E19CCF9372C1187325972FE546CD" www.example.com

       $ unbound-host -v -y "example.com DS 31560 5 1 1CFED84787E6E19CCF9372C1187325972FE546CD" 192.0.2.153

Exit Code
---------

The ``unbound-host`` program exits with status code 1 on error, 0 on no error.
The data may not be available on exit code 0, exit code 1 means the lookup
encountered a fatal error.

See Also
--------

:doc:`unbound.conf(5)</manpages/unbound.conf>`,
:doc:`unbound(8)</manpages/unbound>`.
