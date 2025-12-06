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

unbound.conf(5)
===============

Synopsis
--------

**unbound.conf**

Description
-----------

**unbound.conf** is used to configure :doc:`unbound(8)</manpages/unbound>`.
The file format has attributes and values.
Some attributes have attributes inside them.
The notation is: ``attribute: value``.

Comments start with ``#`` and last to the end of line.
Empty lines are ignored as is whitespace at the beginning of a line.

The utility :doc:`unbound-checkconf(8)</manpages/unbound-checkconf>` can be
used to check ``unbound.conf`` prior to usage.

Example
-------

An example config file is shown below.
Copy this to :file:`/etc/unbound/unbound.conf` and start the server with:

.. code-block:: text

    $ unbound -c /etc/unbound/unbound.conf

Most settings are the defaults.
Stop the server with:

.. code-block:: text

    $ kill `cat /etc/unbound/unbound.pid`

Below is a minimal config file.
The source distribution contains an extensive :file:`example.conf` file with
all the options.

.. code-block:: text

    # unbound.conf(5) config file for unbound(8).
    server:
        directory: "/etc/unbound"
        username: unbound
        # make sure unbound can access entropy from inside the chroot.
        # e.g. on linux the use these commands (on BSD, devfs(8) is used):
        #      mount --bind -n /dev/urandom /etc/unbound/dev/urandom
        # and  mount --bind -n /dev/log /etc/unbound/dev/log
        chroot: "/etc/unbound"
        # logfile: "/etc/unbound/unbound.log"  #uncomment to use logfile.
        pidfile: "/etc/unbound/unbound.pid"
        # verbosity: 1      # uncomment and increase to get more logging.
        # listen on all interfaces, answer queries from the local subnet.
        interface: 0.0.0.0
        interface: ::0
        access-control: 10.0.0.0/8 allow
        access-control: 2001:DB8::/64 allow

File Format
-----------

There must be whitespace between keywords.
Attribute keywords end with a colon ``':'``.
An attribute is followed by a value, or its containing attributes in which case
it is referred to as a clause.
Clauses can be repeated throughout the file (or included files) to group
attributes under the same clause.

.. _unbound.conf.include:

Files can be included using the **include:** directive.
It can appear anywhere, it accepts a single file name as argument.
Processing continues as if the text from the included file was copied into the
config file at that point.
If also using :ref:`chroot<unbound.conf.chroot>`, using full path names for
the included files works, relative pathnames for the included names work if the
directory where the daemon is started equals its chroot/working directory or is
specified before the include statement with :ref:`directory:
dir<unbound.conf.directory>`.
Wildcards can be used to include multiple files, see *glob(7)*.

.. _unbound.conf.include-toplevel:

For a more structural include option, the **include-toplevel:** directive can
be used.
This closes whatever clause is currently active (if any) and forces the use of
clauses in the included files and right after this directive.

.. _unbound.conf.server:

Server Options
^^^^^^^^^^^^^^

These options are part of the **server:** clause.


@@UAHL@unbound.conf@verbosity@@: *<number>*
    The verbosity level.

    Level 0
        No verbosity, only errors.

    Level 1
        Gives operational information.

    Level 2
        Gives detailed operational information including short information per
        query.

    Level 3
        Gives query level information, output per query.

    Level 4
        Gives algorithm level information.

    Level 5
        Logs client identification for cache misses.

    The verbosity can also be increased from the command line and during run
    time via remote control. See :doc:`unbound(8)</manpages/unbound>` and
    :doc:`unbound-control(8)</manpages/unbound-control>` respectively.

    Default: 1


@@UAHL@unbound.conf@statistics-interval@@: *<seconds>*
    The number of seconds between printing statistics to the log for every
    thread.
    Disable with value ``0`` or ``""``.
    The histogram statistics are only printed if replies were sent during the
    statistics interval, requestlist statistics are printed for every interval
    (but can be 0).
    This is because the median calculation requires data to be present.

    Default: 0 (disabled)


@@UAHL@unbound.conf@statistics-cumulative@@: *<yes or no>*
    If enabled, statistics are cumulative since starting Unbound, without
    clearing the statistics counters after logging the statistics.

    Default: no


@@UAHL@unbound.conf@extended-statistics@@: *<yes or no>*
    If enabled, extended statistics are printed from
    :doc:`unbound-control(8)</manpages/unbound-control>`.
    The counters are listed in
    :doc:`unbound-control(8)</manpages/unbound-control>`.
    Keeping track of more statistics takes time.

    Default: no


@@UAHL@unbound.conf@statistics-inhibit-zero@@: *<yes or no>*
    If enabled, selected extended statistics with a value of 0 are inhibited
    from printing with
    :doc:`unbound-control(8)</manpages/unbound-control>`.
    These are query types, query classes, query opcodes, answer rcodes
    (except NOERROR, FORMERR, SERVFAIL, NXDOMAIN, NOTIMPL, REFUSED)
    and PRZ actions.

    Default: yes


@@UAHL@unbound.conf@num-threads@@: *<number>*
    The number of threads to create to serve clients. Use 1 for no threading.

    Default: 1


@@UAHL@unbound.conf@port@@: *<port number>*
    The port number on which the server responds to queries.

    Default: 53


@@UAHL@unbound.conf@interface@@: *<IP address or interface name[@port]>*
    Interface to use to connect to the network.
    This interface is listened to for queries from clients, and answers to
    clients are given from it.
    Can be given multiple times to work on several interfaces.
    If none are given the default is to listen on localhost.

    If an interface name is used instead of an IP address, the list of IP
    addresses on that interface are used.
    The interfaces are not changed on a reload (``kill -HUP``) but only on
    restart.

    A port number can be specified with @port (without spaces between interface
    and port number), if not specified the default port (from
    :ref:`port<unbound.conf.port>`) is used.


@@UAHL@unbound.conf@ip-address@@: *<IP address or interface name[@port]>*
    Same as :ref:`interface<unbound.conf.interface>` (for ease of
    compatibility with :external+nsd:doc:`manpages/nsd.conf`).


@@UAHL@unbound.conf@interface-automatic@@: *<yes or no>*
    Listen on all addresses on all (current and future) interfaces, detect the
    source interface on UDP queries and copy them to replies.
    This is a lot like :ref:`ip-transparent<unbound.conf.ip-transparent>`, but
    this option services all interfaces whilst with
    :ref:`ip-transparent<unbound.conf.ip-transparent>` you can select which
    (future) interfaces Unbound provides service on.
    This feature is experimental, and needs support in your OS for particular
    socket options.

    Default: no


@@UAHL@unbound.conf@interface-automatic-ports@@: *"<string>"*
    List the port numbers that
    :ref:`interface-automatic<unbound.conf.interface-automatic>` listens on.
    If empty, the default port is listened on.
    The port numbers are separated by spaces in the string.

    This can be used to have interface automatic to deal with the interface,
    and listen on the normal port number, by including it in the list, and
    also HTTPS or DNS-over-TLS port numbers by putting them in the list as
    well.

    Default: ""


@@UAHL@unbound.conf@outgoing-interface@@: *<IPv4/IPv6 address or IPv6 netblock>*
    Interface to use to connect to the network.
    This interface is used to send queries to authoritative servers and receive
    their replies.
    Can be given multiple times to work on several interfaces.
    If none are given the default (all) is used.
    You can specify the same interfaces in
    :ref:`interface<unbound.conf.interface>` and
    :ref:`outgoing-interface<unbound.conf.outgoing-interface>` lines, the
    interfaces are then used for both purposes.
    Outgoing queries are sent via a random outgoing interface to counter
    spoofing.

    If an IPv6 netblock is specified instead of an individual IPv6 address,
    outgoing UDP queries will use a randomised source address taken from the
    netblock to counter spoofing.
    Requires the IPv6 netblock to be routed to the host running Unbound, and
    requires OS support for unprivileged non-local binds (currently only
    supported on Linux).
    Several netblocks may be specified with multiple
    :ref:`outgoing-interface<unbound.conf.outgoing-interface>` options, but do
    not specify both an individual IPv6 address and an IPv6 netblock, or the
    randomisation will be compromised.
    Consider combining with :ref:`prefer-ip6: yes<unbound.conf.prefer-ip6>` to
    increase the likelihood of IPv6 nameservers being selected for queries.
    On Linux you need these two commands to be able to use the freebind socket
    option to receive traffic for the ip6 netblock:

    .. code-block:: text

        ip -6 addr add mynetblock/64 dev lo && \
        ip -6 route add local mynetblock/64 dev lo


@@UAHL@unbound.conf@outgoing-range@@: *<number>*
    Number of ports to open.
    This number of file descriptors can be opened per thread.
    Must be at least 1.
    Default depends on compile options.
    Larger numbers need extra resources from the operating system.
    For performance a very large value is best, use libevent to make this
    possible.

    Default: 4096 (libevent) / 960 (minievent) / 48 (windows)


@@UAHL@unbound.conf@outgoing-port-permit@@: *<port number or range>*
    Permit Unbound to open this port or range of ports for use to send queries.
    A larger number of permitted outgoing ports increases resilience against
    spoofing attempts.
    Make sure these ports are not needed by other daemons.
    By default only ports above 1024 that have not been assigned by IANA are
    used.
    Give a port number or a range of the form "low-high", without spaces.

    The :ref:`outgoing-port-permit<unbound.conf.outgoing-port-permit>` and
    :ref:`outgoing-port-avoid<unbound.conf.outgoing-port-avoid>` statements
    are processed in the line order of the config file, adding the permitted
    ports and subtracting the avoided ports from the set of allowed ports.
    The processing starts with the non IANA allocated ports above 1024 in the
    set of allowed ports.


@@UAHL@unbound.conf@outgoing-port-avoid@@: *<port number or range>*
    Do not permit Unbound to open this port or range of ports for use to send
    queries.
    Use this to make sure Unbound does not grab a port that another daemon
    needs.
    The port is avoided on all outgoing interfaces, both IPv4 and IPv6.
    By default only ports above 1024 that have not been assigned by IANA are
    used.
    Give a port number or a range of the form "low-high", without spaces.


@@UAHL@unbound.conf@outgoing-num-tcp@@: *<number>*
    Number of outgoing TCP buffers to allocate per thread.
    If set to 0, or if :ref:`do-tcp: no<unbound.conf.do-tcp>` is set, no TCP
    queries to authoritative servers are done.
    For larger installations increasing this value is a good idea.

    Default: 10


@@UAHL@unbound.conf@incoming-num-tcp@@: *<number>*
    Number of incoming TCP buffers to allocate per thread.
    If set to 0, or if :ref:`do-tcp: no<unbound.conf.do-tcp>` is set, no TCP
    queries from clients are accepted.
    For larger installations increasing this value is a good idea.

    Default: 10


@@UAHL@unbound.conf@edns-buffer-size@@: *<number>*
    Number of bytes size to advertise as the EDNS reassembly buffer size.
    This is the value put into datagrams over UDP towards peers.
    The actual buffer size is determined by
    :ref:`msg-buffer-size<unbound.conf.msg-buffer-size>` (both for TCP and
    UDP).
    Do not set higher than that value.
    Setting to 512 bypasses even the most stringent path MTU problems, but is
    seen as extreme, since the amount of TCP fallback generated is excessive
    (probably also for this resolver, consider tuning
    :ref:`outgoing-num-tcp<unbound.conf.outgoing-num-tcp>`).

    Default: 1232 (`DNS Flag Day 2020 recommendation
    <https://dnsflagday.net/2020/>`__)


@@UAHL@unbound.conf@max-udp-size@@: *<number>*
    Maximum UDP response size (not applied to TCP response).
    65536 disables the UDP response size maximum, and uses the choice from the
    client, always.
    Suggested values are 512 to 4096.

    Default: 1232 (same as :ref:`edns-buffer-size<unbound.conf.edns-buffer-size>`)


@@UAHL@unbound.conf@stream-wait-size@@: *<number>*
    Number of bytes size maximum to use for waiting stream buffers.
    A plain number is in bytes, append 'k', 'm' or 'g' for kilobytes, megabytes
    or gigabytes (1024*1024 bytes in a megabyte).
    As TCP and TLS streams queue up multiple results, the amount of memory used
    for these buffers does not exceed this number, otherwise the responses are
    dropped.
    This manages the total memory usage of the server (under heavy use), the
    number of requests that can be queued up per connection is also limited,
    with further requests waiting in TCP buffers.

    Default: 4m


@@UAHL@unbound.conf@msg-buffer-size@@: *<number>*
    Number of bytes size of the message buffers.
    Default is 65552 bytes, enough for 64 Kb packets, the maximum DNS message
    size.
    No message larger than this can be sent or received.
    Can be reduced to use less memory, but some requests for DNS data, such as
    for huge resource records, will result in a SERVFAIL reply to the client.

    Default: 65552


@@UAHL@unbound.conf@msg-cache-size@@: *<number>*
    Number of bytes size of the message cache.
    A plain number is in bytes, append 'k', 'm' or 'g' for kilobytes, megabytes
    or gigabytes (1024*1024 bytes in a megabyte).

    Default: 4m


@@UAHL@unbound.conf@msg-cache-slabs@@: *<number>*
    Number of slabs in the message cache.
    Slabs reduce lock contention by threads.
    Must be set to a power of 2.
    Setting (close) to the number of cpus is a fairly good setting.
    If left unconfigured, it will be configured automatically to be a power of
    2 close to the number of configured threads in multi-threaded environments.

    Default: (unconfigured)


@@UAHL@unbound.conf@num-queries-per-thread@@: *<number>*
    The number of queries that every thread will service simultaneously.
    If more queries arrive that need servicing, and no queries can be jostled
    out (see :ref:`jostle-timeout<unbound.conf.jostle-timeout>`), then the
    queries are dropped.
    This forces the client to resend after a timeout; allowing the server time
    to work on the existing queries.
    Default depends on compile options.

    Default: 2048 (libevent) / 512 (minievent) / 24 (windows)


@@UAHL@unbound.conf@jostle-timeout@@: *<msec>*
    Timeout used when the server is very busy.
    Set to a value that usually results in one roundtrip to the authority
    servers.

    If too many queries arrive, then 50% of the queries are allowed to run to
    completion, and the other 50% are replaced with the new incoming query if
    they have already spent more than their allowed time.
    This protects against denial of service by slow queries or high query
    rates.

    The effect is that the qps for long-lasting queries is about:

    .. code-block:: text

        (num-queries-per-thread / 2) / (average time for such long queries) qps

    The qps for short queries can be about:

    .. code-block:: text

        (num-queries-per-thread / 2) / (jostle-timeout in whole seconds) qps per thread

    about (2048/2)*5 = 5120 qps by default.

    Default: 200


@@UAHL@unbound.conf@delay-close@@: *<msec>*
    Extra delay for timeouted UDP ports before they are closed, in msec.
    This prevents very delayed answer packets from the upstream (recursive)
    servers from bouncing against closed ports and setting off all sort of
    close-port counters, with eg. 1500 msec.
    When timeouts happen you need extra sockets, it checks the ID and remote IP
    of packets, and unwanted packets are added to the unwanted packet counter.

    Default: 0 (disabled)


@@UAHL@unbound.conf@udp-connect@@: *<yes or no>*
    Perform *connect(2)* for UDP sockets that mitigates ICMP side channel
    leakage.

    Default: yes


@@UAHL@unbound.conf@unknown-server-time-limit@@: *<msec>*
    The wait time in msec for waiting for an unknown server to reply.
    Increase this if you are behind a slow satellite link, to eg. 1128.
    That would then avoid re-querying every initial query because it times out.

    Default: 376


@@UAHL@unbound.conf@discard-timeout@@: *<msec>*
    The wait time in msec where recursion requests are dropped.
    This is to stop a large number of replies from accumulating.
    They receive no reply, the work item continues to recurse.
    It is nice to be a bit larger than
    :ref:`serve-expired-client-timeout<unbound.conf.serve-expired-client-timeout>`
    if that is enabled.
    A value of ``1900`` msec is suggested.
    The value ``0`` disables it.

    Default: 1900


@@UAHL@unbound.conf@wait-limit@@: *<number>*
    The number of replies that can wait for recursion, for an IP address.
    This makes a ratelimit per IP address of waiting replies for recursion.
    It stops very large amounts of queries waiting to be returned to one
    destination.
    The value ``0`` disables wait limits.

    Default: 1000


@@UAHL@unbound.conf@wait-limit-cookie@@: *<number>*
    The number of replies that can wait for recursion, for an IP address
    that sent the query with a valid DNS Cookie.
    Since the cookie validates the client address, this limit can be higher.

    Default: 10000


@@UAHL@unbound.conf@wait-limit-netblock@@: *<netblock>* *<number>*
    The wait limit for the netblock.
    If not given the
    :ref:`wait-limit<unbound.conf.wait-limit>`
    value is used.
    The most specific netblock is used to determine the limit.
    Useful for overriding the default for a specific, group or individual,
    server.
    The value ``-1`` disables wait limits for the netblock.
    By default the loopback has a wait limit netblock of ``-1``, it is not
    limited, because it is separated from the rest of network for spoofed
    packets.
    The loopback addresses ``127.0.0.0/8`` and ``::1/128`` are default at ``-1``.

    Default: (none)


@@UAHL@unbound.conf@wait-limit-cookie-netblock@@: *<netblock>* *<number>*
    The wait limit for the netblock, when the query has a DNS Cookie.
    If not given, the
    :ref:`wait-limit-cookie<unbound.conf.wait-limit-cookie>`
    value is used.
    The value ``-1`` disables wait limits for the netblock.
    The loopback addresses ``127.0.0.0/8`` and ``::1/128`` are default at ``-1``.

    Default: (none)


@@UAHL@unbound.conf@so-rcvbuf@@: *<number>*
    If not 0, then set the SO_RCVBUF socket option to get more buffer space on
    UDP port 53 incoming queries.
    So that short spikes on busy servers do not drop packets (see counter in
    ``netstat -su``).
    Otherwise, the number of bytes to ask for, try "4m" on a busy server.

    The OS caps it at a maximum, on linux Unbound needs root permission to
    bypass the limit, or the admin can use ``sysctl net.core.rmem_max``.

    On BSD change ``kern.ipc.maxsockbuf`` in ``/etc/sysctl.conf``.

    On OpenBSD change header and recompile kernel.

    On Solaris ``ndd -set /dev/udp udp_max_buf 8388608``.

    Default: 0 (use system value)


@@UAHL@unbound.conf@so-sndbuf@@: *<number>*
    If not 0, then set the SO_SNDBUF socket option to get more buffer space on
    UDP port 53 outgoing queries.
    This for very busy servers handles spikes in answer traffic, otherwise:

    .. code-block:: text

        send: resource temporarily unavailable

    can get logged, the buffer overrun is also visible by ``netstat -su``.
    If set to 0 it uses the system value.
    Specify the number of bytes to ask for, try "8m" on a very busy server.

    It needs some space to be able to deal with packets that wait for local
    address resolution, from like ARP and NDP discovery, before they are sent
    out, hence it is elevated above the system default by default.

    The OS caps it at a maximum, on linux Unbound needs root permission to
    bypass the limit, or the admin can use ``sysctl net.core.wmem_max``.

    On BSD, Solaris changes are similar to
    :ref:`so-rcvbuf<unbound.conf.so-rcvbuf>`.

    Default: 4m


@@UAHL@unbound.conf@so-reuseport@@: *<yes or no>*
    If yes, then open dedicated listening sockets for incoming queries for each
    thread and try to set the SO_REUSEPORT socket option on each socket.
    May distribute incoming queries to threads more evenly.

    On Linux it is supported in kernels >= 3.9.

    On other systems, FreeBSD, OSX it may also work.

    You can enable it (on any platform and kernel), it then attempts to open
    the port and passes the option if it was available at compile time, if that
    works it is used, if it fails, it continues silently (unless verbosity 3)
    without the option.

    At extreme load it could be better to turn it off to distribute the queries
    evenly, reported for Linux systems (4.4.x).

    Default: yes


@@UAHL@unbound.conf@ip-transparent@@: *<yes or no>*
    If yes, then use IP_TRANSPARENT socket option on sockets where Unbound is
    listening for incoming traffic.
    Allows you to bind to non-local interfaces.
    For example for non-existent IP addresses that are going to exist later on,
    with host failover configuration.

    This is a lot like
    :ref:`interface-automatic<unbound.conf.interface-automatic>`, but that one
    services all interfaces and with this option you can select which (future)
    interfaces Unbound provides service on.

    This option needs Unbound to be started with root permissions on some
    systems.
    The option uses IP_BINDANY on FreeBSD systems and SO_BINDANY on OpenBSD
    systems.

    Default: no


@@UAHL@unbound.conf@ip-freebind@@: *<yes or no>*
    If yes, then use IP_FREEBIND socket option on sockets where Unbound is
    listening to incoming traffic.
    Allows you to bind to IP addresses that are nonlocal or do not exist, like
    when the network interface or IP address is down.

    Exists only on Linux, where the similar
    :ref:`ip-transparent<unbound.conf.ip-transparent>` option is also
    available.

    Default: no


@@UAHL@unbound.conf@ip-dscp@@: *<number>*
    The value of the Differentiated Services Codepoint (DSCP) in the
    differentiated services field (DS) of the outgoing IP packet headers.
    The field replaces the outdated IPv4 Type-Of-Service field and the IPv6
    traffic class field.


@@UAHL@unbound.conf@rrset-cache-size@@: *<number>*
    Number of bytes size of the RRset cache.
    A plain number is in bytes, append 'k', 'm' or 'g' for kilobytes, megabytes
    or gigabytes (1024*1024 bytes in a megabyte).

    Default: 4m


@@UAHL@unbound.conf@rrset-cache-slabs@@: *<number>*
    Number of slabs in the RRset cache.
    Slabs reduce lock contention by threads.
    Must be set to a power of 2.
    Setting (close) to the number of cpus is a fairly good setting.
    If left unconfigured, it will be configured automatically to be a power of
    2 close to the number of configured threads in multi-threaded environments.

    Default: (unconfigured)


@@UAHL@unbound.conf@cache-max-ttl@@: *<seconds>*
    Time to live maximum for RRsets and messages in the cache.
    When the TTL expires, the cache item has expired.
    Can be set lower to force the resolver to query for data often, and not
    trust (very large) TTL values.
    Downstream clients also see the lower TTL.


    Default: 86400 (1 day)


@@UAHL@unbound.conf@cache-min-ttl@@: *<seconds>*
    Time to live minimum for RRsets and messages in the cache.
    If the minimum kicks in, the data is cached for longer than the domain
    owner intended, and thus less queries are made to look up the data.
    Zero makes sure the data in the cache is as the domain owner intended,
    higher values, especially more than an hour or so, can lead to trouble as
    the data in the cache does not match up with the actual data any more.

    Default: 0 (disabled)


@@UAHL@unbound.conf@cache-max-negative-ttl@@: *<seconds>*
    Time to live maximum for negative responses, these have a SOA in the
    authority section that is limited in time.
    This applies to NXDOMAIN and NODATA answers.

    Default: 3600


@@UAHL@unbound.conf@cache-min-negative-ttl@@: *<seconds>*
    Time to live minimum for negative responses, these have a SOA in the
    authority section that is limited in time.
    If this is disabled and
    :ref:`cache-min-ttl<unbound.conf.cache-min-ttl>`
    is configured, it will take effect instead.
    In that case you can set this to ``1`` to honor the upstream TTL.
    This applies to NXDOMAIN and NODATA answers.

    Default: 0 (disabled)


@@UAHL@unbound.conf@infra-host-ttl@@: *<seconds>*
    Time to live for entries in the host cache.
    The host cache contains roundtrip timing, lameness and EDNS support
    information.

    Default: 900


@@UAHL@unbound.conf@infra-cache-slabs@@: *<number>*
    Number of slabs in the infrastructure cache.
    Slabs reduce lock contention by threads.
    Must be set to a power of 2.
    Setting (close) to the number of cpus is a fairly good setting.
    If left unconfigured, it will be configured automatically to be a power of
    2 close to the number of configured threads in multi-threaded environments.

    Default: (unconfigured)


@@UAHL@unbound.conf@infra-cache-numhosts@@: *<number>*
    Number of hosts for which information is cached.

    Default: 10000


@@UAHL@unbound.conf@infra-cache-min-rtt@@: *<msec>*
    Lower limit for dynamic retransmit timeout calculation in infrastructure
    cache.
    Increase this value if using forwarders needing more time to do recursive
    name resolution.

    Default: 50


@@UAHL@unbound.conf@infra-cache-max-rtt@@: *<msec>*
    Upper limit for dynamic retransmit timeout calculation in infrastructure
    cache.

    Default: 120000 (2 minutes)


@@UAHL@unbound.conf@infra-keep-probing@@: *<yes or no>*
    If enabled the server keeps probing hosts that are down, in the one probe
    at a time regime.
    Hosts that are down, eg. they did not respond during the one probe at a
    time period, are marked as down and it may take
    :ref:`infra-host-ttl<unbound.conf.infra-host-ttl>` time to get probed
    again.

    Default: no


@@UAHL@unbound.conf@define-tag@@: *"<list of tags>"*
    Define the tags that can be used with
    :ref:`local-zone<unbound.conf.local-zone>` and
    :ref:`access-control<unbound.conf.access-control>`.
    Enclose the list between quotes (``""``) and put spaces between tags.


@@UAHL@unbound.conf@do-ip4@@: *<yes or no>*
    Enable or disable whether IPv4 queries are answered or issued.

    Default: yes


@@UAHL@unbound.conf@do-ip6@@: *<yes or no>*
    Enable or disable whether IPv6 queries are answered or issued.
    If disabled, queries are not answered on IPv6, and queries are not sent on
    IPv6 to the internet nameservers.
    With this option you can disable the IPv6 transport for sending DNS
    traffic, it does not impact the contents of the DNS traffic, which may have
    IPv4 (A) and IPv6 (AAAA) addresses in it.

    Default: yes


@@UAHL@unbound.conf@prefer-ip4@@: *<yes or no>*
    If enabled, prefer IPv4 transport for sending DNS queries to internet
    nameservers.
    Useful if the IPv6 netblock the server has, the entire /64 of that is not
    owned by one operator and the reputation of the netblock /64 is an issue,
    using IPv4 then uses the IPv4 filters that the upstream servers have.

    Default: no


@@UAHL@unbound.conf@prefer-ip6@@: *<yes or no>*
    If enabled, prefer IPv6 transport for sending DNS queries to internet
    nameservers.

    Default: no


@@UAHL@unbound.conf@do-udp@@: *<yes or no>*
    Enable or disable whether UDP queries are answered or issued.

    Default: yes


@@UAHL@unbound.conf@do-tcp@@: *<yes or no>*
    Enable or disable whether TCP queries are answered or issued.

    Default: yes


@@UAHL@unbound.conf@tcp-mss@@: *<number>*
    Maximum segment size (MSS) of TCP socket on which the server responds to
    queries.
    Value lower than common MSS on Ethernet (1220 for example) will address
    path MTU problem.
    Note that not all platform supports socket option to set MSS (TCP_MAXSEG).
    Default is system default MSS determined by interface MTU and negotiation
    between server and client.


@@UAHL@unbound.conf@outgoing-tcp-mss@@: *<number>*
    Maximum segment size (MSS) of TCP socket for outgoing queries (from Unbound
    to other servers).
    Value lower than common MSS on Ethernet (1220 for example) will address
    path MTU problem.
    Note that not all platform supports socket option to set MSS (TCP_MAXSEG).
    Default is system default MSS determined by interface MTU and negotiation
    between Unbound and other servers.


@@UAHL@unbound.conf@tcp-idle-timeout@@: *<msec>*
    The period Unbound will wait for a query on a TCP connection.
    If this timeout expires Unbound closes the connection.
    When the number of free incoming TCP buffers falls below 50% of the total
    number configured, the option value used is progressively reduced, first to
    1% of the configured value, then to 0.2% of the configured value if the
    number of free buffers falls below 35% of the total number configured, and
    finally to 0 if the number of free buffers falls below 20% of the total
    number configured.
    A minimum timeout of 200 milliseconds is observed regardless of the option
    value used.
    It will be overridden by
    :ref:`edns-tcp-keepalive-timeout<unbound.conf.edns-tcp-keepalive-timeout>`
    if
    :ref:`edns-tcp-keepalive<unbound.conf.edns-tcp-keepalive>`
    is enabled.

    Default: 30000 (30 seconds)


@@UAHL@unbound.conf@tcp-reuse-timeout@@: *<msec>*
    The period Unbound will keep TCP persistent connections open to authority
    servers.

    Default: 60000 (60 seconds)


@@UAHL@unbound.conf@max-reuse-tcp-queries@@: *<number>*
    The maximum number of queries that can be sent on a persistent TCP
    connection.

    Default: 200


@@UAHL@unbound.conf@tcp-auth-query-timeout@@: *<number>*
    Timeout in milliseconds for TCP queries to auth servers.

    Default: 3000 (3 seconds)


@@UAHL@unbound.conf@edns-tcp-keepalive@@: *<yes or no>*
    Enable or disable EDNS TCP Keepalive.

    Default: no


@@UAHL@unbound.conf@edns-tcp-keepalive-timeout@@: *<msec>*
    Overrides
    :ref:`tcp-idle-timeout<unbound.conf.tcp-idle-timeout>`
    when
    :ref:`edns-tcp-keepalive<unbound.conf.edns-tcp-keepalive>`
    is enabled.
    If the client supports the EDNS TCP Keepalive option,
    If the client supports the EDNS TCP Keepalive option, Unbound sends the
    timeout value to the client to encourage it to close the connection before
    the server times out.

    Default: 120000 (2 minutes)


@@UAHL@unbound.conf@sock-queue-timeout@@: *<sec>*
    UDP queries that have waited in the socket buffer for a long time can be
    dropped.
    The time is set in seconds, 3 could be a good value to ignore old queries
    that likely the client does not need a reply for any more.
    This could happen if the host has not been able to service the queries for
    a while, i.e. Unbound is not running, and then is enabled again.
    It uses timestamp socket options.
    The socket option is available on the Linux and FreeBSD platforms.

    Default: 0 (disabled)


@@UAHL@unbound.conf@tcp-upstream@@: *<yes or no>*
    Enable or disable whether the upstream queries use TCP only for transport.
    Useful in tunneling scenarios.
    If set to no you can specify TCP transport only for selected forward or
    stub zones using
    :ref:`forward-tcp-upstream<unbound.conf.forward.forward-tcp-upstream>` or
    :ref:`stub-tcp-upstream<unbound.conf.stub.stub-tcp-upstream>`
    respectively.

    Default: no


@@UAHL@unbound.conf@udp-upstream-without-downstream@@: *<yes or no>*
    Enable UDP upstream even if :ref:`do-udp: no<unbound.conf.do-udp>` is set.
    Useful for TLS service providers, that want no UDP downstream but use UDP
    to fetch data upstream.

    Default: no (no changes)


@@UAHL@unbound.conf@tls-upstream@@: *<yes or no>*
    Enabled or disable whether the upstream queries use TLS only for transport.
    Useful in tunneling scenarios.
    The TLS contains plain DNS in TCP wireformat.
    The other server must support this (see
    :ref:`tls-service-key<unbound.conf.tls-service-key>`).

    If you enable this, also configure a
    :ref:`tls-cert-bundle<unbound.conf.tls-cert-bundle>` or use
    :ref:`tls-win-cert<unbound.conf.tls-win-cert>` or
    :ref:`tls-system-cert<unbound.conf.tls-system-cert>` to load CA certs,
    otherwise the connections cannot be authenticated.

    This option enables TLS for all of them, but if you do not set this you can
    configure TLS specifically for some forward zones with
    :ref:`forward-tls-upstream<unbound.conf.forward.forward-tls-upstream>`.
    And also with
    :ref:`stub-tls-upstream<unbound.conf.stub.stub-tls-upstream>`.
    If the
    :ref:`tls-upstream<unbound.conf.tls-upstream>`
    option is enabled, it is for all the forwards and stubs, where the
    :ref:`forward-tls-upstream<unbound.conf.forward.forward-tls-upstream>`
    and
    :ref:`stub-tls-upstream<unbound.conf.stub.stub-tls-upstream>`
    options are ignored, as if they had been set to yes.

    Default: no


@@UAHL@unbound.conf@ssl-upstream@@: *<yes or no>*
    Alternate syntax for :ref:`tls-upstream<unbound.conf.tls-upstream>`.
    If both are present in the config file the last is used.


@@UAHL@unbound.conf@tls-service-key@@: *<file>*
    If enabled, the server provides DNS-over-TLS or DNS-over-HTTPS service on
    the TCP ports marked implicitly or explicitly for these services with
    :ref:`tls-port<unbound.conf.tls-port>` or
    :ref:`https-port<unbound.conf.https-port>`.
    The file must contain the private key for the TLS session, the public
    certificate is in the :ref:`tls-service-pem<unbound.conf.tls-service-pem>`
    file and it must also be specified if
    :ref:`tls-service-key<unbound.conf.tls-service-key>` is specified.
    Enabling or disabling this service requires a restart (a reload is not
    enough), because the key is read while root permissions are held and before
    chroot (if any).
    The ports enabled implicitly or explicitly via
    :ref:`tls-port<unbound.conf.tls-port>` and
    :ref:`https-port<unbound.conf.https-port>` do not provide normal DNS TCP
    service.

    .. note::
        Unbound needs to be compiled with libnghttp2 in order to provide
        DNS-over-HTTPS.

    Default: "" (disabled)


@@UAHL@unbound.conf@ssl-service-key@@: *<file>*
    Alternate syntax for :ref:`tls-service-key<unbound.conf.tls-service-key>`.


@@UAHL@unbound.conf@tls-service-pem@@: *<file>*
    The public key certificate pem file for the tls service.

    Default: "" (disabled)


@@UAHL@unbound.conf@ssl-service-pem@@: *<file>*
    Alternate syntax for :ref:`tls-service-pem<unbound.conf.tls-service-pem>`.


@@UAHL@unbound.conf@tls-port@@: *<number>*
    The port number on which to provide TCP TLS service.
    Only interfaces configured with that port number as @number get the TLS
    service.

    Default: 853


@@UAHL@unbound.conf@ssl-port@@: *<number>*
    Alternate syntax for :ref:`tls-port<unbound.conf.tls-port>`.


@@UAHL@unbound.conf@tls-cert-bundle@@: *<file>*
    If null or ``""``, no file is used.
    Set it to the certificate bundle file, for example
    :file:`/etc/pki/tls/certs/ca-bundle.crt`.
    These certificates are used for authenticating connections made to outside
    peers.
    For example :ref:`auth-zone urls<unbound.conf.auth.url>`, and also
    DNS-over-TLS connections.
    It is read at start up before permission drop and chroot.

    Default: "" (disabled)


@@UAHL@unbound.conf@ssl-cert-bundle@@: *<file>*
    Alternate syntax for :ref:`tls-cert-bundle<unbound.conf.tls-cert-bundle>`.


@@UAHL@unbound.conf@tls-win-cert@@: *<yes or no>*
    Add the system certificates to the cert bundle certificates for
    authentication.
    If no cert bundle, it uses only these certificates.
    On windows this option uses the certificates from the cert store.
    Use the :ref:`tls-cert-bundle<unbound.conf.tls-cert-bundle>` option on
    other systems.
    On other systems, this option enables the system certificates.

    Default: no


@@UAHL@unbound.conf@tls-system-cert@@: *<yes or no>*
    This the same attribute as the
    :ref:`tls-win-cert<unbound.conf.tls-win-cert>` attribute, under a
    different name.
    Because it is not windows specific.


@@UAHL@unbound.conf@tls-additional-port@@: *<portnr>*
    List port numbers as
    :ref:`tls-additional-port<unbound.conf.tls-additional-port>`, and when
    interfaces are defined, eg. with the @port suffix, as this port number,
    they provide DNS-over-TLS service.
    Can list multiple, each on a new statement.


@@UAHL@unbound.conf@tls-session-ticket-keys@@: *<file>*
    If not ``""``, lists files with 80 bytes of random contents that are used
    to perform TLS session resumption for clients using the Unbound server.
    These files contain the secret key for the TLS session tickets.
    First key use to encrypt and decrypt TLS session tickets.
    Other keys use to decrypt only.

    With this you can roll over to new keys, by generating a new first file and
    allowing decrypt of the old file by listing it after the first file for
    some time, after the wait clients are not using the old key any more and
    the old key can be removed.
    One way to create the file is:

    .. code-block:: text

        dd if=/dev/random bs=1 count=80 of=ticket.dat

    The first 16 bytes should be different from the old one if you create a
    second key, that is the name used to identify the key.
    Then there is 32 bytes random data for an AES key and then 32 bytes random
    data for the HMAC key.

    Default: ""


@@UAHL@unbound.conf@tls-ciphers@@: *<string with cipher list>*
    Set the list of ciphers to allow when serving TLS.
    Use ``""`` for default ciphers.

    Default: ""


@@UAHL@unbound.conf@tls-ciphersuites@@: *<string with ciphersuites list>*
    Set the list of ciphersuites to allow when serving TLS.
    This is for newer TLS 1.3 connections.
    Use ``""`` for default ciphersuites.

    Default: ""


@@UAHL@unbound.conf@pad-responses@@: *<yes or no>*
    If enabled, TLS serviced queries that contained an EDNS Padding option will
    cause responses padded to the closest multiple of the size specified in
    :ref:`pad-responses-block-size<unbound.conf.pad-responses-block-size>`.

    Default: yes


@@UAHL@unbound.conf@pad-responses-block-size@@: *<number>*
    The block size with which to pad responses serviced over TLS.
    Only responses to padded queries will be padded.

    Default: 468


@@UAHL@unbound.conf@pad-queries@@: *<yes or no>*
    If enabled, all queries sent over TLS upstreams will be padded to the
    closest multiple of the size specified in
    :ref:`pad-queries-block-size<unbound.conf.pad-queries-block-size>`.

    Default: yes


@@UAHL@unbound.conf@pad-queries-block-size@@: *<number>*
    The block size with which to pad queries sent over TLS upstreams.

    Default: 128


@@UAHL@unbound.conf@tls-use-sni@@: *<yes or no>*
    Enable or disable sending the SNI extension on TLS connections.

    .. note:: Changing the value requires a reload.

    Default: yes


@@UAHL@unbound.conf@https-port@@: *<number>*
    The port number on which to provide DNS-over-HTTPS service.
    Only interfaces configured with that port number as @number get the HTTPS
    service.

    Default: 443


@@UAHL@unbound.conf@http-endpoint@@: *<endpoint string>*
    The HTTP endpoint to provide DNS-over-HTTPS service on.

    Default: /dns-query


@@UAHL@unbound.conf@http-max-streams@@: *<number of streams>*
    Number used in the SETTINGS_MAX_CONCURRENT_STREAMS parameter in the HTTP/2
    SETTINGS frame for DNS-over-HTTPS connections.

    Default: 100


@@UAHL@unbound.conf@http-query-buffer-size@@: *<size in bytes>*
    Maximum number of bytes used for all HTTP/2 query buffers combined.
    These buffers contain (partial) DNS queries waiting for request stream
    completion.
    An RST_STREAM frame will be send to streams exceeding this limit.
    A plain number is in bytes, append 'k', 'm' or 'g' for kilobytes, megabytes
    or gigabytes (1024*1024 bytes in a megabyte).

    Default: 4m


@@UAHL@unbound.conf@http-response-buffer-size@@: *<size in bytes>*
    Maximum number of bytes used for all HTTP/2 response buffers combined.
    These buffers contain DNS responses waiting to be written back to the
    clients.
    An RST_STREAM frame will be send to streams exceeding this limit.
    A plain number is in bytes, append 'k', 'm' or 'g' for kilobytes, megabytes
    or gigabytes (1024*1024 bytes in a megabyte).

    Default: 4m


@@UAHL@unbound.conf@http-nodelay@@: *<yes or no>*
    Set TCP_NODELAY socket option on sockets used to provide DNS-over-HTTPS
    service.
    Ignored if the option is not available.

    Default: yes


@@UAHL@unbound.conf@http-notls-downstream@@: *<yes or no>*
    Disable use of TLS for the downstream DNS-over-HTTP connections.
    Useful for local back end servers.

    Default: no


@@UAHL@unbound.conf@proxy-protocol-port@@: *<portnr>*
    List port numbers as
    :ref:`proxy-protocol-port<unbound.conf.proxy-protocol-port>`, and when
    interfaces are defined, eg. with the @port suffix, as this port number,
    they support and expect PROXYv2.

    In this case the proxy address will only be used for the network
    communication and initial ACL (check if the proxy itself is denied/refused
    by configuration).

    The proxied address (if any) will then be used as the true client address
    and will be used where applicable for logging, ACL, DNSTAP, RPZ and IP
    ratelimiting.

    PROXYv2 is supported for UDP and TCP/TLS listening interfaces.

    There is no support for PROXYv2 on a DoH, DoQ or DNSCrypt listening interface.

    Can list multiple, each on a new statement.


@@UAHL@unbound.conf@quic-port@@: *<number>*
    The port number on which to provide DNS-over-QUIC service.
    Only interfaces configured with that port number as @number get the QUIC
    service.
    The interface uses QUIC for the UDP traffic on that port number.

    Default: 853


@@UAHL@unbound.conf@quic-size@@: *<size in bytes>*
    Maximum number of bytes for all QUIC buffers and data combined.
    A plain number is in bytes, append 'k', 'm' or 'g' for kilobytes, megabytes
    or gigabytes (1024*1024 bytes in a megabyte).
    New connections receive connection refused when the limit is exceeded.
    New streams are reset when the limit is exceeded.

    Default: 8m


@@UAHL@unbound.conf@use-systemd@@: *<yes or no>*
    Enable or disable systemd socket activation.

    Default: no


@@UAHL@unbound.conf@do-daemonize@@: *<yes or no>*
    Enable or disable whether the Unbound server forks into the background as a
    daemon.
    Set the value to no when Unbound runs as systemd service.

    Default: yes


@@UAHL@unbound.conf@tcp-connection-limit@@: *<IP netblock> <limit>*
    Allow up to limit simultaneous TCP connections from the given netblock.
    When at the limit, further connections are accepted but closed immediately.
    This option is experimental at this time.

    Default: (disabled)


@@UAHL@unbound.conf@access-control@@: *<IP netblock> <action>*
    Specify treatment of incoming queries from their originating IP address.
    Queries can be allowed to have access to this server that gives DNS
    answers, or refused, with other actions possible.
    The IP address range can be specified as a netblock, it is possible to give
    the statement several times in order to specify the treatment of different
    netblocks.
    The netblock is given as an IPv4 or IPv6 address with /size appended for a
    classless network block.
    The most specific netblock match is used, if none match
    :ref:`refuse<unbound.conf.access-control.action.refuse>` is used.
    The order of the access-control statements therefore does not matter.
    The action can be
    :ref:`deny<unbound.conf.access-control.action.deny>`,
    :ref:`refuse<unbound.conf.access-control.action.refuse>`,
    :ref:`allow<unbound.conf.access-control.action.allow>`,
    :ref:`allow_setrd<unbound.conf.access-control.action.allow_setrd>`,
    :ref:`allow_snoop<unbound.conf.access-control.action.allow_snoop>`,
    :ref:`allow_cookie<unbound.conf.access-control.action.allow_cookie>`,
    :ref:`deny_non_local<unbound.conf.access-control.action.deny_non_local>` or
    :ref:`refuse_non_local<unbound.conf.access-control.action.refuse_non_local>`.


    @@UAHL@unbound.conf.access-control.action@deny@@
        Stops queries from hosts from that netblock.

    @@UAHL@unbound.conf.access-control.action@refuse@@
        Stops queries too, but sends a DNS rcode REFUSED error message back.

    @@UAHL@unbound.conf.access-control.action@allow@@
        Gives access to clients from that netblock.
        It gives only access for recursion clients (which is what almost all
        clients need).
        Non-recursive queries are refused.

        The :ref:`allow<unbound.conf.access-control.action.allow>` action does
        allow non-recursive queries to access the local-data that is
        configured.
        The reason is that this does not involve the Unbound server recursive
        lookup algorithm, and static data is served in the reply.
        This supports normal operations where non-recursive queries are made
        for the authoritative data.
        For non-recursive queries any replies from the dynamic cache are
        refused.

    @@UAHL@unbound.conf.access-control.action@allow_setrd@@
        Ignores the recursion desired (RD) bit and treats all requests as if
        the recursion desired bit is set.

        Note that this behavior violates :rfc:`1034` which states that a name
        server should never perform recursive service unless asked via the RD
        bit since this interferes with trouble shooting of name servers and
        their databases.
        This prohibited behavior may be useful if another DNS server must
        forward requests for specific zones to a resolver DNS server, but only
        supports stub domains and sends queries to the resolver DNS server with
        the RD bit cleared.

    @@UAHL@unbound.conf.access-control.action@allow_snoop@@
        Gives non-recursive access too.
        This gives both recursive and non recursive access.
        The name *allow_snoop* refers to cache snooping, a technique to use
        non-recursive queries to examine the cache contents (for malicious
        acts).
        However, non-recursive queries can also be a valuable debugging tool
        (when you want to examine the cache contents).

        In that case use
        :ref:`allow_snoop<unbound.conf.access-control.action.allow_snoop>` for
        your administration host.

    @@UAHL@unbound.conf.access-control.action@allow_cookie@@
        Allows access only to UDP queries that contain a valid DNS Cookie as
        specified in RFC 7873 and RFC 9018, when the
        :ref:`answer-cookie<unbound.conf.answer-cookie>` option is enabled.
        UDP queries containing only a DNS Client Cookie and no Server Cookie,
        or an invalid DNS Cookie, will receive a BADCOOKIE response including a
        newly generated DNS Cookie, allowing clients to retry with that DNS
        Cookie.
        The *allow_cookie* action will also accept requests over stateful
        transports, regardless of the presence of an DNS Cookie and regardless
        of the :ref:`answer-cookie<unbound.conf.answer-cookie>` setting.
        UDP queries without a DNS Cookie receive REFUSED responses with the TC
        flag set, that may trigger fall back to TCP for those clients.

    @@UAHL@unbound.conf.access-control.action@deny_non_local@@
        The
        :ref:`deny_non_local<unbound.conf.access-control.action.deny_non_local>`
        action is for hosts that are only allowed to query for the
        authoritative :ref:`local-data<unbound.conf.local-data>`, they are not
        allowed full recursion but only the static data.
        Messages that are disallowed are dropped.

    @@UAHL@unbound.conf.access-control.action@refuse_non_local@@
        The
        :ref:`refuse_non_local<unbound.conf.access-control.action.refuse_non_local>`
        action is for hosts that are only allowed to query for the
        authoritative :ref:`local-data<unbound.conf.local-data>`, they are not
        allowed full recursion but only the static data.
        Messages that are disallowed receive error code REFUSED.


    By default only localhost (the 127.0.0.0/8 IP netblock, not the loopback
    interface) is implicitly *allowed*, the rest is refused.
    The default is *refused*, because that is protocol-friendly.
    The DNS protocol is not designed to handle dropped packets due to policy,
    and dropping may result in (possibly excessive) retried queries.


@@UAHL@unbound.conf@access-control-tag@@: *<IP netblock> "<list of tags>"*
    Assign tags to :ref:`access-control<unbound.conf.access-control>`
    elements.
    Clients using this access control element use localzones that are tagged
    with one of these tags.

    Tags must be defined in :ref:`define-tag<unbound.conf.define-tag>`.
    Enclose list of tags in quotes (``""``) and put spaces between tags.

    If :ref:`access-control-tag<unbound.conf.access-control-tag>` is
    configured for a netblock that does not have an
    :ref:`access-control<unbound.conf.access-control>`, an access-control
    element with action :ref:`allow<unbound.conf.access-control.action.allow>`
    is configured for this netblock.


@@UAHL@unbound.conf@access-control-tag-action@@: *<IP netblock> <tag> <action>*
    Set action for particular tag for given access control element.
    If you have multiple tag values, the tag used to lookup the action is the
    first tag match between
    :ref:`access-control-tag<unbound.conf.access-control-tag>` and
    :ref:`local-zone-tag<unbound.conf.local-zone-tag>` where "first" comes
    from the order of the :ref:`define-tag<unbound.conf.define-tag>` values.


@@UAHL@unbound.conf@access-control-tag-data@@: *<IP netblock> <tag> "<resource record string>"*
    Set redirect data for particular tag for given access control element.


@@UAHL@unbound.conf@access-control-view@@: *<IP netblock> <view name>*
    Set view for given access control element.


@@UAHL@unbound.conf@interface-action@@: *<ip address or interface name [@port]> <action>*
    Similar to :ref:`access-control<unbound.conf.access-control>` but for
    interfaces.

    The action is the same as the ones defined under
    :ref:`access-control<unbound.conf.access-control>`.

    Default action for interfaces is
    :ref:`refuse<unbound.conf.access-control.action.refuse>`.
    By default only localhost (the 127.0.0.0/8 IP netblock, not the loopback
    interface) is implicitly allowed through the default
    :ref:`access-control<unbound.conf.access-control>` behavior.
    This also means that any attempt to use the **interface-\*:** options for
    the loopback interface will not work as they will be overridden by the
    implicit default "access-control: 127.0.0.0/8 allow" option.

    .. note::
        The interface needs to be already specified with
        :ref:`interface<unbound.conf.interface>` and that any
        **access-control\*:** attribute overrides all **interface-\*:**
        attributes for targeted clients.


@@UAHL@unbound.conf@interface-tag@@: *<ip address or interface name [@port]> <"list of tags">*
    Similar to :ref:`access-control-tag<unbound.conf.access-control-tag>` but
    for interfaces.

    .. note::
        The interface needs to be already specified with
        :ref:`interface<unbound.conf.interface>` and that any
        **access-control\*:** attribute overrides all **interface-\*:**
        attributes for targeted clients.


@@UAHL@unbound.conf@interface-tag-action@@: *<ip address or interface name [@port]> <tag> <action>*
    Similar to
    :ref:`access-control-tag-action<unbound.conf.access-control-tag-action>`
    but for interfaces.

    .. note::
        The interface needs to be already specified with
        :ref:`interface<unbound.conf.interface>` and that any
        **access-control\*:** attribute overrides all **interface-\*:**
        attributes for targeted clients.


@@UAHL@unbound.conf@interface-tag-data@@: *<ip address or interface name [@port]> <tag> <"resource record string">*
    Similar to
    :ref:`access-control-tag-data<unbound.conf.access-control-tag-data>` but
    for interfaces.

    .. note::
        The interface needs to be already specified with
        :ref:`interface<unbound.conf.interface>` and that any
        **access-control\*:** attribute overrides all **interface-\*:**
        attributes for targeted clients.


@@UAHL@unbound.conf@interface-view@@: *<ip address or interface name [@port]> <view name>*
    Similar to :ref:`access-control-view<unbound.conf.access-control-view>`
    but for interfaces.

    .. note::
        The interface needs to be already specified with
        :ref:`interface<unbound.conf.interface>` and that any
        **access-control\*:** attribute overrides all **interface-\*:**
        attributes for targeted clients.


@@UAHL@unbound.conf@chroot@@: *<directory>*
    If :ref:`chroot<unbound.conf.chroot>` is enabled, you should pass the
    configfile (from the commandline) as a full path from the original root.
    After the chroot has been performed the now defunct portion of the config
    file path is removed to be able to reread the config after a reload.

    All other file paths (working dir, logfile, roothints, and key files) can
    be specified in several ways: as an absolute path relative to the new root,
    as a relative path to the working directory, or as an absolute path
    relative to the original root.
    In the last case the path is adjusted to remove the unused portion.

    The pidfile can be either a relative path to the working directory, or an
    absolute path relative to the original root.
    It is written just prior to chroot and dropping permissions.
    This allows the pidfile to be :file:`/var/run/unbound.pid` and the chroot
    to be :file:`/var/unbound`, for example.
    Note that Unbound is not able to remove the pidfile after termination when
    it is located outside of the chroot directory.

    Additionally, Unbound may need to access :file:`/dev/urandom` (for entropy)
    from inside the chroot.

    If given, a *chroot(2)* is done to the given directory.
    If you give ``""`` no *chroot(2)* is performed.

    Default: @UNBOUND_CHROOT_DIR@


@@UAHL@unbound.conf@username@@: *<name>*
    If given, after binding the port the user privileges are dropped.
    If you give username: ``""`` no user change is performed.

    If this user is not capable of binding the port, reloads (by signal HUP)
    will still retain the opened ports.
    If you change the port number in the config file, and that new port number
    requires privileges, then a reload will fail; a restart is needed.

    Default: @UNBOUND_USERNAME@


@@UAHL@unbound.conf@directory@@: *<directory>*
    Sets the working directory for the program.
    On Windows the string "%EXECUTABLE%" tries to change to the directory that
    :command:`unbound.exe` resides in.
    If you give a :ref:`server: directory:
    \<directory\><unbound.conf.directory>` before
    :ref:`include<unbound.conf.include>` file statements then those includes
    can be relative to the working directory.

    Default: @UNBOUND_RUN_DIR@


@@UAHL@unbound.conf@logfile@@: *<filename>*
    If ``""`` is given, logging goes to stderr, or nowhere once daemonized.
    The logfile is appended to, in the following format:

    .. code-block:: text

        [seconds since 1970] unbound[pid:tid]: type: message.

    If this option is given, the :ref:`use-syslog<unbound.conf.use-syslog>`
    attribute is internally set to ``no``.

    The logfile is reopened (for append) when the config file is reread, on
    SIGHUP.

    Default: "" (disabled)


@@UAHL@unbound.conf@use-syslog@@: *<yes or no>*
    Sets Unbound to send log messages to the syslogd, using *syslog(3)*.
    The log facility LOG_DAEMON is used, with identity "unbound".
    The logfile setting is overridden when
    :ref:`use-syslog: yes<unbound.conf.use-syslog>` is set.

    Default: yes


@@UAHL@unbound.conf@log-identity@@: *<string>*
    If ``""`` is given, then the name of the executable, usually
    "unbound" is used to report to the log.
    Enter a string to override it with that, which is useful on systems that
    run more than one instance of Unbound, with different configurations, so
    that the logs can be easily distinguished against.

    Default: ""


@@UAHL@unbound.conf@log-time-ascii@@: *<yes or no>*
    Sets logfile lines to use a timestamp in UTC ASCII.
    No effect if using syslog, in that case syslog formats the timestamp
    printed into the log files.

    Default: no (prints the seconds since 1970 in brackets)


@@UAHL@unbound.conf@log-time-iso@@: *<yes or no>*
    Log time in ISO8601 format, if
    :ref:`log-time-ascii: yes<unbound.conf.log-time-ascii>`
    is also set.

    Default: no


@@UAHL@unbound.conf@log-queries@@: *<yes or no>*
    Prints one line per query to the log, with the log timestamp and IP
    address, name, type and class.
    Note that it takes time to print these lines which makes the server
    (significantly) slower.
    Odd (nonprintable) characters in names are printed as ``'?'``.

    Default: no


@@UAHL@unbound.conf@log-replies@@: *<yes or no>*
    Prints one line per reply to the log, with the log timestamp and IP
    address, name, type, class, return code, time to resolve, from cache and
    response size.
    Note that it takes time to print these lines which makes the server
    (significantly) slower.
    Odd (nonprintable) characters in names are printed as ``'?'``.

    Default: no


@@UAHL@unbound.conf@log-tag-queryreply@@: *<yes or no>*
    Prints the word 'query' and 'reply' with
    :ref:`log-queries<unbound.conf.log-queries>` and
    :ref:`log-replies<unbound.conf.log-replies>`.
    This makes filtering logs easier.

    Default: no (backwards compatible)


@@UAHL@unbound.conf@log-destaddr@@: *<yes or no>*
    Prints the destination address, port and type in the
    :ref:`log-replies<unbound.conf.log-replies>` output.
    This disambiguates what type of traffic, eg. UDP or TCP, and to what local
    port the traffic was sent to.

    Default: no


@@UAHL@unbound.conf@log-local-actions@@: *<yes or no>*
    Print log lines to inform about local zone actions.
    These lines are like the :ref:`local-zone type
    inform<unbound.conf.local-zone.type.inform>` print outs, but they are also
    printed for the other types of local zones.

    Default: no


@@UAHL@unbound.conf@log-servfail@@: *<yes or no>*
    Print log lines that say why queries return SERVFAIL to clients.
    This is separate from the verbosity debug logs, much smaller, and printed
    at the error level, not the info level of debug info from verbosity.

    Default: no


@@UAHL@unbound.conf@pidfile@@: *<filename>*
    The process id is written to the file.
    Default is :file:`"@UNBOUND_PIDFILE@"`.
    So,

    .. code-block:: text

        kill -HUP `cat @UNBOUND_PIDFILE@`

    triggers a reload,

    .. code-block:: text

        kill -TERM `cat @UNBOUND_PIDFILE@`

    gracefully terminates.

    Default: @UNBOUND_PIDFILE@


@@UAHL@unbound.conf@root-hints@@: *<filename>*
    Read the root hints from this file.
    Default is nothing, using builtin hints for the IN class.
    The file has the format of zone files, with root nameserver names and
    addresses only.
    The default may become outdated, when servers change, therefore it is good
    practice to use a root hints file.

    Default: ""


@@UAHL@unbound.conf@hide-identity@@: *<yes or no>*
    If enabled 'id.server' and 'hostname.bind' queries are REFUSED.

    Default: no


@@UAHL@unbound.conf@identity@@: *<string>*
    Set the identity to report.
    If set to ``""``, then the hostname of the server is returned.

    Default: ""


@@UAHL@unbound.conf@hide-version@@: *<yes or no>*
    If enabled 'version.server' and 'version.bind' queries are REFUSED.

    Default: no


@@UAHL@unbound.conf@version@@: *<string>*
    Set the version to report.
    If set to ``""``, then the package version is returned.

    Default: ""


@@UAHL@unbound.conf@hide-http-user-agent@@: *<yes or no>*
    If enabled the HTTP header User-Agent is not set.
    Use with caution as some webserver configurations may reject HTTP requests
    lacking this header.
    If needed, it is better to explicitly set the
    :ref:`http-user-agent<unbound.conf.http-user-agent>` below.

    Default: no


@@UAHL@unbound.conf@http-user-agent@@: *<string>*
    Set the HTTP User-Agent header for outgoing HTTP requests.
    If set to ``""``, then the package name and version are used.

    Default: ""


@@UAHL@unbound.conf@nsid@@: *<string>*
    Add the specified nsid to the EDNS section of the answer when queried with
    an NSID EDNS enabled packet.
    As a sequence of hex characters or with 'ascii\_' prefix and then an ASCII
    string.

    Default: (disabled)


@@UAHL@unbound.conf@hide-trustanchor@@: *<yes or no>*
    If enabled 'trustanchor.unbound' queries are REFUSED.

    Default: no


@@UAHL@unbound.conf@target-fetch-policy@@: *<"list of numbers">*
    Set the target fetch policy used by Unbound to determine if it should fetch
    nameserver target addresses opportunistically.
    The policy is described per dependency depth.

    The number of values determines the maximum dependency depth that Unbound
    will pursue in answering a query.
    A value of -1 means to fetch all targets opportunistically for that
    dependency depth.
    A value of 0 means to fetch on demand only.
    A positive value fetches that many targets opportunistically.

    Enclose the list between quotes (``""``) and put spaces between numbers.
    Setting all zeroes, "0 0 0 0 0" gives behaviour closer to that of BIND 9,
    while setting "-1 -1 -1 -1 -1" gives behaviour rumoured to be closer to
    that of BIND 8.

    Default:  "3 2 1 0 0"


@@UAHL@unbound.conf@harden-short-bufsize@@: *<yes or no>*
    Very small EDNS buffer sizes from queries are ignored.

    Default: yes (as described in the standard)


@@UAHL@unbound.conf@harden-large-queries@@: *<yes or no>*
    Very large queries are ignored.
    Default is no, since it is legal protocol wise to send these, and could be
    necessary for operation if TSIG or EDNS payload is very large.

    Default: no


@@UAHL@unbound.conf@harden-glue@@: *<yes or no>*
    Will trust glue only if it is within the servers authority.

    Default: yes


@@UAHL@unbound.conf@harden-unverified-glue@@: *<yes or no>*
    Will trust only in-zone glue.
    Will try to resolve all out of zone (*unverified*) glue.
    Will fallback to the original glue if unable to resolve.

    Default: no


@@UAHL@unbound.conf@harden-dnssec-stripped@@: *<yes or no>*
    Require DNSSEC data for trust-anchored zones, if such data is absent, the
    zone becomes bogus.
    If turned off, and no DNSSEC data is received (or the DNSKEY data fails to
    validate), then the zone is made insecure, this behaves like there is no
    trust anchor.
    You could turn this off if you are sometimes behind an intrusive firewall
    (of some sort) that removes DNSSEC data from packets, or a zone changes
    from signed to unsigned to badly signed often.
    If turned off you run the risk of a downgrade attack that disables security
    for a zone.

    Default: yes


@@UAHL@unbound.conf@harden-below-nxdomain@@: *<yes or no>*
    From :rfc:`8020` (with title "NXDOMAIN: There Really Is Nothing
    Underneath"), returns NXDOMAIN to queries for a name below another name
    that is already known to be NXDOMAIN.
    DNSSEC mandates NOERROR for empty nonterminals, hence this is possible.
    Very old software might return NXDOMAIN for empty nonterminals (that
    usually happen for reverse IP address lookups), and thus may be
    incompatible with this.
    To try to avoid this only DNSSEC-secure NXDOMAINs are used, because the old
    software does not have DNSSEC.

    .. note::
        The NXDOMAIN must be secure, this means NSEC3 with optout is
        insufficient.

    Default: yes


@@UAHL@unbound.conf@harden-referral-path@@: *<yes or no>*
    Harden the referral path by performing additional queries for
    infrastructure data.
    Validates the replies if trust anchors are configured and the zones are
    signed.
    This enforces DNSSEC validation on nameserver NS sets and the nameserver
    addresses that are encountered on the referral path to the answer.
    Default is off, because it burdens the authority servers, and it is not RFC
    standard, and could lead to performance problems because of the extra query
    load that is generated.
    Experimental option.
    If you enable it consider adding more numbers after the
    :ref:`target-fetch-policy<unbound.conf.target-fetch-policy>` to increase
    the max depth that is checked to.

    Default: no


@@UAHL@unbound.conf@harden-algo-downgrade@@: *<yes or no>*
    Harden against algorithm downgrade when multiple algorithms are advertised
    in the DS record.
    This works by first choosing only the strongest DS digest type as per
    :rfc:`4509` (Unbound treats the highest algorithm as the strongest) and
    then expecting signatures from all the advertised signing algorithms from
    the chosen DS(es) to be present.
    If no, allows any one supported algorithm to validate the zone, even if
    other advertised algorithms are broken.
    :rfc:`6840` mandates that zone signers must produce zones signed with all
    advertised algorithms, but sometimes they do not.
    :rfc:`6840` also clarifies that this requirement is not for validators and
    validators should accept any single valid path.
    It should thus be explicitly noted that this option violates :rfc:`6840`
    for DNSSEC validation and should only be used to perform a signature
    completeness test to support troubleshooting.

    .. warning::
        Using this option may break DNSSEC resolution with non :rfc:`6840`
        conforming signers and/or in multi-signer configurations that don't
        send all the advertised signatures.

    Default: no


@@UAHL@unbound.conf@harden-unknown-additional@@: *<yes or no>*
    Harden against unknown records in the authority section and additional
    section.
    If no, such records are copied from the upstream and presented to the
    client together with the answer.
    If yes, it could hamper future protocol developments that want to add
    records.

    Default: no


@@UAHL@unbound.conf@use-caps-for-id@@: *<yes or no>*
    Use 0x20-encoded random bits in the query to foil spoof attempts.
    This perturbs the lowercase and uppercase of query names sent to authority
    servers and checks if the reply still has the correct casing.
    This feature is an experimental implementation of draft dns-0x20.

    Default: no


@@UAHL@unbound.conf@caps-exempt@@: *<domain>*
    Exempt the domain so that it does not receive caps-for-id perturbed
    queries.
    For domains that do not support 0x20 and also fail with fallback because
    they keep sending different answers, like some load balancers.
    Can be given multiple times, for different domains.


@@UAHL@unbound.conf@caps-whitelist@@: *<domain>*
    Alternate syntax for :ref:`caps-exempt<unbound.conf.caps-exempt>`.


@@UAHL@unbound.conf@qname-minimisation@@: *<yes or no>*
    Send minimum amount of information to upstream servers to enhance privacy.
    Only send minimum required labels of the QNAME and set QTYPE to A when
    possible.
    Best effort approach; full QNAME and original QTYPE will be sent when
    upstream replies with a RCODE other than NOERROR, except when receiving
    NXDOMAIN from a DNSSEC signed zone.

    Default: yes


@@UAHL@unbound.conf@qname-minimisation-strict@@: *<yes or no>*
    QNAME minimisation in strict mode.
    Do not fall-back to sending full QNAME to potentially broken nameservers.
    A lot of domains will not be resolvable when this option in enabled.
    Only use if you know what you are doing.
    This option only has effect when
    :ref:`qname-minimisation<unbound.conf.qname-minimisation>` is enabled.

    Default: no


@@UAHL@unbound.conf@aggressive-nsec@@: *<yes or no>*
    Aggressive NSEC uses the DNSSEC NSEC chain to synthesize NXDOMAIN and other
    denials, using information from previous NXDOMAINs answers.
    It helps to reduce the query rate towards targets that get a very high
    nonexistent name lookup rate.

    Default: yes


@@UAHL@unbound.conf@private-address@@: *<IP address or subnet>*
    Give IPv4 of IPv6 addresses or classless subnets.
    These are addresses on your private network, and are not allowed to be
    returned for public internet names.
    Any occurrence of such addresses are removed from DNS answers.
    Additionally, the DNSSEC validator may mark the answers bogus.
    This protects against so-called DNS Rebinding, where a user browser is
    turned into a network proxy, allowing remote access through the browser to
    other parts of your private network.

    Some names can be allowed to contain your private addresses, by default all
    the :ref:`local-data<unbound.conf.local-data>` that you configured is
    allowed to, and you can specify additional names using
    :ref:`private-domain<unbound.conf.private-domain>`.
    No private addresses are enabled by default.

    We consider to enable this for the :rfc:`1918` private IP address space by
    default in later releases.
    That would enable private addresses for ``10.0.0.0/8``, ``172.16.0.0/12``,
    ``192.168.0.0/16``, ``169.254.0.0/16``, ``fd00::/8`` and ``fe80::/10``,
    since the RFC standards say these addresses should not be visible on the
    public internet.

    Turning on ``127.0.0.0/8`` would hinder many spamblocklists as they use
    that.
    Adding ``::ffff:0:0/96`` stops IPv4-mapped IPv6 addresses from bypassing
    the filter.


@@UAHL@unbound.conf@private-domain@@: *<domain name>*
    Allow this domain, and all its subdomains to contain private addresses.
    Give multiple times to allow multiple domain names to contain private
    addresses.

    Default: (none)


@@UAHL@unbound.conf@unwanted-reply-threshold@@: *<number>*
    If set, a total number of unwanted replies is kept track of in every
    thread.
    When it reaches the threshold, a defensive action is taken and a warning is
    printed to the log.
    The defensive action is to clear the rrset and message caches, hopefully
    flushing away any poison.
    A value of 10 million is suggested.

    Default: 0 (disabled)


@@UAHL@unbound.conf@do-not-query-address@@: *<IP address>*
    Do not query the given IP address.
    Can be IPv4 or IPv6.
    Append /num to indicate a classless delegation netblock, for example like
    ``10.2.3.4/24`` or ``2001::11/64``.

    Default: (none)


@@UAHL@unbound.conf@do-not-query-localhost@@: *<yes or no>*
    If yes, localhost is added to the
    :ref:`do-not-query-address<unbound.conf.do-not-query-address>` entries,
    both IPv6 ``::1`` and IPv4 ``127.0.0.1/8``.
    If no, then localhost can be used to send queries to.

    Default: yes


@@UAHL@unbound.conf@prefetch@@: *<yes or no>*
    If yes, cache hits on message cache elements that are on their last 10
    percent of their TTL value trigger a prefetch to keep the cache up to date.
    Turning it on gives about 10 percent more traffic and load on the machine,
    but popular items do not expire from the cache.

    Default: no


@@UAHL@unbound.conf@prefetch-key@@: *<yes or no>*
    If yes, fetch the DNSKEYs earlier in the validation process, when a DS
    record is encountered.
    This lowers the latency of requests.
    It does use a little more CPU.
    Also if the cache is set to 0, it is no use.

    Default: no


@@UAHL@unbound.conf@deny-any@@: *<yes or no>*
    If yes, deny queries of type ANY with an empty response.
    If disabled, Unbound responds with a short list of resource records if some
    can be found in the cache and makes the upstream type ANY query if there
    are none.

    Default: no


@@UAHL@unbound.conf@rrset-roundrobin@@: *<yes or no>*
    If yes, Unbound rotates RRSet order in response (the random number is taken
    from the query ID, for speed and thread safety).

    Default: yes


@@UAHL@unbound.conf@minimal-responses@@: *<yes or no>*
    If yes, Unbound does not insert authority/additional sections into response
    messages when those sections are not required.
    This reduces response size significantly, and may avoid TCP fallback for
    some responses which may cause a slight speedup.
    The default is yes, even though the DNS protocol RFCs mandate these
    sections, and the additional content could save roundtrips for clients that
    use the additional content.
    However these sections are hardly used by clients.
    Enabling prefetch can benefit clients that need the additional content
    by trying to keep that content fresh in the cache.

    Default: yes


@@UAHL@unbound.conf@disable-dnssec-lame-check@@: *<yes or no>*
    If yes, disables the DNSSEC lameness check in the iterator.
    This check sees if RRSIGs are present in the answer, when DNSSEC is
    expected, and retries another authority if RRSIGs are unexpectedly missing.
    The validator will insist in RRSIGs for DNSSEC signed domains regardless of
    this setting, if a trust anchor is loaded.

    Default: no


@@UAHL@unbound.conf@module-config@@: *"<module names>"*
    Module configuration, a list of module names separated by spaces, surround
    the string with quotes (``""``).
    The modules can be ``respip``, ``validator``, or ``iterator`` (and possibly
    more, see below).

    .. note::
        The ordering of the modules is significant, the order decides the order
        of processing.

    Setting this to just "iterator" will result in a non-validating server.
    Setting this to "validator iterator" will turn on DNSSEC validation.

    .. note::
        You must also set trust-anchors for validation to be useful.

    Adding ``respip`` to the front will cause RPZ processing to be done on all
    queries.

    Most modules that need to be listed here have to be listed at the beginning
    of the line.

    The ``subnetcache`` module has to be listed just before the iterator.

    The ``python`` module can be listed in different places, it then processes
    the output of the module it is just before.

    The ``dynlib`` module can be listed pretty much anywhere, it is only a very
    thin wrapper that allows dynamic libraries to run in its place.

    Default: "validator iterator"


@@UAHL@unbound.conf@trust-anchor-file@@: *<filename>*
    File with trusted keys for validation.
    Both DS and DNSKEY entries can appear in the file.
    The format of the file is the standard DNS Zone file format.

    Default: "" (no trust anchor file)


@@UAHL@unbound.conf@auto-trust-anchor-file@@: *<filename>*
    File with trust anchor for one zone, which is tracked with :rfc:`5011`
    probes.
    The probes are run several times per month, thus the machine must be online
    frequently.
    The initial file can be one with contents as described in
    :ref:`trust-anchor-file<unbound.conf.trust-anchor-file>`.
    The file is written to when the anchor is updated, so the Unbound user must
    have write permission.
    Write permission to the file, but also to the directory it is in (to create
    a temporary file, which is necessary to deal with filesystem full events),
    it must also be inside the :ref:`chroot<unbound.conf.chroot>` (if that is
    used).

    Default: "" (no auto trust anchor file)


@@UAHL@unbound.conf@trust-anchor@@: *"<Resource Record>"*
    A DS or DNSKEY RR for a key to use for validation.
    Multiple entries can be given to specify multiple trusted keys, in addition
    to the :ref:`trust-anchor-file<unbound.conf.trust-anchor-file>`.
    The resource record is entered in the same format as *dig(1)* or *drill(1)*
    prints them, the same format as in the zone file.
    Has to be on a single line, with ``""`` around it.
    A TTL can be specified for ease of cut and paste, but is ignored.
    A class can be specified, but class IN is default.

    Default: (none)


@@UAHL@unbound.conf@trusted-keys-file@@: *<filename>*
    File with trusted keys for validation.
    Specify more than one file with several entries, one file per entry.
    Like :ref:`trust-anchor-file<unbound.conf.trust-anchor-file>` but has a
    different file format.
    Format is BIND-9 style format, the ``trusted-keys { name flag proto algo
    "key"; };`` clauses are read.
    It is possible to use wildcards with this statement, the wildcard is
    expanded on start and on reload.

    Default: "" (no trusted keys file)


@@UAHL@unbound.conf@trust-anchor-signaling@@: *<yes or no>*
    Send :rfc:`8145` key tag query after trust anchor priming.

    Default: yes


@@UAHL@unbound.conf@root-key-sentinel@@: *<yes or no>*
    Root key trust anchor sentinel.

    Default: yes


@@UAHL@unbound.conf@domain-insecure@@: *<domain name>*
    Sets *<domain name>* to be insecure, DNSSEC chain of trust is ignored
    towards the *<domain name>*.
    So a trust anchor above the domain name can not make the domain secure with
    a DS record, such a DS record is then ignored.
    Can be given multiple times to specify multiple domains that are treated as
    if unsigned.
    If you set trust anchors for the domain they override this setting (and the
    domain is secured).

    This can be useful if you want to make sure a trust anchor for external
    lookups does not affect an (unsigned) internal domain.
    A DS record externally can create validation failures for that internal
    domain.

    Default: (none)


@@UAHL@unbound.conf@val-override-date@@: *<rrsig-style date spec>*
    .. warning:: Debugging feature!

    If enabled by giving a RRSIG style date, that date is used for verifying
    RRSIG inception and expiration dates, instead of the current date.
    Do not set this unless you are debugging signature inception and
    expiration.
    The value -1 ignores the date altogether, useful for some special
    applications.

    Default: 0 (disabled)


@@UAHL@unbound.conf@val-sig-skew-min@@: *<seconds>*
    Minimum number of seconds of clock skew to apply to validated signatures.
    A value of 10% of the signature lifetime (expiration - inception) is used,
    capped by this setting.
    Default is 3600 (1 hour) which allows for daylight savings differences.
    Lower this value for more strict checking of short lived signatures.

    Default: 3600 (1 hour)


@@UAHL@unbound.conf@val-sig-skew-max@@: *<seconds>*
    Maximum number of seconds of clock skew to apply to validated signatures.
    A value of 10% of the signature lifetime (expiration - inception) is used,
    capped by this setting.
    Default is 86400 (24 hours) which allows for timezone setting problems in
    stable domains.
    Setting both min and max very low disables the clock skew allowances.
    Setting both min and max very high makes the validator check the signature
    timestamps less strictly.

    Default: 86400 (24 hours)


@@UAHL@unbound.conf@val-max-restart@@: *<number>*
    The maximum number the validator should restart validation with another
    authority in case of failed validation.

    Default: 5


@@UAHL@unbound.conf@val-bogus-ttl@@: *<seconds>*
    The time to live for bogus data.
    This is data that has failed validation; due to invalid signatures or other
    checks.
    The TTL from that data cannot be trusted, and this value is used instead.
    The time interval prevents repeated revalidation of bogus data.

    Default: 60


@@UAHL@unbound.conf@val-clean-additional@@: *<yes or no>*
    Instruct the validator to remove data from the additional section of secure
    messages that are not signed properly.
    Messages that are insecure, bogus, indeterminate or unchecked are not
    affected.
    Use this setting to protect the users that rely on this validator for
    authentication from potentially bad data in the additional section.

    Default: yes


@@UAHL@unbound.conf@val-log-level@@: *<number>*
    Have the validator print validation failures to the log.
    Regardless of the verbosity setting.

    At 1, for every user query that fails a line is printed to the logs.
    This way you can monitor what happens with validation.
    Use a diagnosis tool, such as dig or drill, to find out why validation is
    failing for these queries.

    At 2, not only the query that failed is printed but also the reason why
    Unbound thought it was wrong and which server sent the faulty data.

    Default: 0 (disabled)


@@UAHL@unbound.conf@val-permissive-mode@@: *<yes or no>*
    Instruct the validator to mark bogus messages as indeterminate.
    The security checks are performed, but if the result is bogus (failed
    security), the reply is not withheld from the client with SERVFAIL as
    usual.
    The client receives the bogus data.
    For messages that are found to be secure the AD bit is set in replies.
    Also logging is performed as for full validation.

    Default: no


@@UAHL@unbound.conf@ignore-cd-flag@@: *<yes or no>*
    Instruct Unbound to ignore the CD flag from clients and refuse to return
    bogus answers to them.
    Thus, the CD (Checking Disabled) flag does not disable checking any more.
    This is useful if legacy (w2008) servers that set the CD flag but cannot
    validate DNSSEC themselves are the clients, and then Unbound provides them
    with DNSSEC protection.

    Default: no


@@UAHL@unbound.conf@disable-edns-do@@: *<yes or no>*
    Disable the EDNS DO flag in upstream requests.
    It breaks DNSSEC validation for Unbound's clients.
    This results in the upstream name servers to not include DNSSEC records in
    their replies and could be helpful for devices that cannot handle DNSSEC
    information.
    When the option is enabled, clients that set the DO flag receive no EDNS
    record in the response to indicate the lack of support to them.
    If this option is enabled but Unbound is already configured for DNSSEC
    validation (i.e., the validator module is enabled; default) this option is
    implicitly turned off with a warning as to not break DNSSEC validation in
    Unbound.

    Default: no


@@UAHL@unbound.conf@serve-expired@@: *<yes or no>*
    If enabled, Unbound attempts to serve old responses from cache with a TTL
    of :ref:`serve-expired-reply-ttl<unbound.conf.serve-expired-reply-ttl>` in
    the response.
    By default the expired answer will be used after a resolution attempt
    errored out or is taking more than
    :ref:`serve-expired-client-timeout<unbound.conf.serve-expired-client-timeout>`
    to resolve.

    Default: no


@@UAHL@unbound.conf@serve-expired-ttl@@: *<seconds>*
    Limit serving of expired responses to configured seconds after expiration.
    ``0`` disables the limit.
    This option only applies when
    :ref:`serve-expired<unbound.conf.serve-expired>` is enabled.
    A suggested value per RFC 8767 is between 86400 (1 day) and 259200 (3 days).
    The default is 86400.

    Default: 86400


@@UAHL@unbound.conf@serve-expired-ttl-reset@@: *<yes or no>*
    Set the TTL of expired records to the
    :ref:`serve-expired-ttl<unbound.conf.serve-expired-ttl>` value after a
    failed attempt to retrieve the record from upstream.
    This makes sure that the expired records will be served as long as there
    are queries for it.

    Default: no


@@UAHL@unbound.conf@serve-expired-reply-ttl@@: *<seconds>*
    TTL value to use when replying with expired data.
    If
    :ref:`serve-expired-client-timeout<unbound.conf.serve-expired-client-timeout>`
    is also used then it is RECOMMENDED to use 30 as the value (:rfc:`8767`).

    Default: 30


@@UAHL@unbound.conf@serve-expired-client-timeout@@: *<msec>*
    Time in milliseconds before replying to the client with expired data.
    This essentially enables the serve-stale behavior as specified in
    :rfc:`8767` that first tries to resolve before immediately responding with
    expired data.
    Setting this to ``0`` will disable this behavior and instead serve the
    expired record immediately from the cache before attempting to refresh it
    via resolution.

    Default: 1800


@@UAHL@unbound.conf@serve-original-ttl@@: *<yes or no>*
    If enabled, Unbound will always return the original TTL as received from
    the upstream name server rather than the decrementing TTL as stored in the
    cache.
    This feature may be useful if Unbound serves as a front-end to a hidden
    authoritative name server.

    Enabling this feature does not impact cache expiry, it only changes the TTL
    Unbound embeds in responses to queries.

    .. note::
        Enabling this feature implicitly disables enforcement of the configured
        minimum and maximum TTL, as it is assumed users who enable this feature
        do not want Unbound to change the TTL obtained from an upstream server.

    .. note::
        The values set using :ref:`cache-min-ttl<unbound.conf.cache-min-ttl>`
        and :ref:`cache-max-ttl<unbound.conf.cache-max-ttl>` are ignored.

    Default: no


@@UAHL@unbound.conf@val-nsec3-keysize-iterations@@: <"list of values">
    List of keysize and iteration count values, separated by spaces, surrounded
    by quotes.
    This determines the maximum allowed NSEC3 iteration count before a message
    is simply marked insecure instead of performing the many hashing
    iterations.
    The list must be in ascending order and have at least one entry.
    If you set it to "1024 65535" there is no restriction to NSEC3 iteration
    values.

    .. note::
        This table must be kept short; a very long list could cause slower
        operation.

    Default: "1024 150 2048 150 4096 150"


@@UAHL@unbound.conf@zonemd-permissive-mode@@: *<yes or no>*
    If enabled the ZONEMD verification failures are only logged and do not
    cause the zone to be blocked and only return servfail.
    Useful for testing out if it works, or if the operator only wants to be
    notified of a problem without disrupting service.

    Default: no


@@UAHL@unbound.conf@add-holddown@@: *<seconds>*
    Instruct the
    :ref:`auto-trust-anchor-file<unbound.conf.auto-trust-anchor-file>` probe
    mechanism for :rfc:`5011` autotrust updates to add new trust anchors only
    after they have been visible for this time.

    Default: 2592000 (30 days as per the RFC)


@@UAHL@unbound.conf@del-holddown@@: *<seconds>*
    Instruct the
    :ref:`auto-trust-anchor-file<unbound.conf.auto-trust-anchor-file>` probe
    mechanism for :rfc:`5011` autotrust updates to remove revoked trust anchors
    after they have been kept in the revoked list for this long.

    Default: 2592000 (30 days as per the RFC)


@@UAHL@unbound.conf@keep-missing@@: *<seconds>*
    Instruct the
    :ref:`auto-trust-anchor-file<unbound.conf.auto-trust-anchor-file>` probe
    mechanism for :rfc:`5011` autotrust updates to remove missing trust anchors
    after they have been unseen for this long.
    This cleans up the state file if the target zone does not perform trust
    anchor revocation, so this makes the auto probe mechanism work with zones
    that perform regular (non-5011) rollovers.
    The value 0 does not remove missing anchors, as per the RFC.

    Default: 31622400 (366 days)


@@UAHL@unbound.conf@permit-small-holddown@@: *<yes or no>*
    Debug option that allows the autotrust 5011 rollover timers to assume very
    small values.

    Default: no


@@UAHL@unbound.conf@key-cache-size@@: *<number>*
    Number of bytes size of the key cache.
    A plain number is in bytes, append 'k', 'm' or 'g' for kilobytes, megabytes
    or gigabytes (1024*1024 bytes in a megabyte).

    Default: 4m


@@UAHL@unbound.conf@key-cache-slabs@@: *<number>*
    Number of slabs in the key cache.
    Slabs reduce lock contention by threads.
    Must be set to a power of 2.
    Setting (close) to the number of cpus is a fairly good setting.
    If left unconfigured, it will be configured automatically to be a power of
    2 close to the number of configured threads in multi-threaded environments.

    Default: (unconfigured)


@@UAHL@unbound.conf@neg-cache-size@@: *<number>*
    Number of bytes size of the aggressive negative cache.
    A plain number is in bytes, append 'k', 'm' or 'g' for kilobytes, megabytes
    or gigabytes (1024*1024 bytes in a megabyte).

    Default: 1m


@@UAHL@unbound.conf@unblock-lan-zones@@: *<yes or no>*
    If enabled, then for private address space, the reverse lookups are no
    longer filtered.
    This allows Unbound when running as dns service on a host where it provides
    service for that host, to put out all of the queries for the 'lan'
    upstream.
    When enabled, only localhost, ``127.0.0.1`` reverse and ``::1`` reverse
    zones are configured with default local zones.
    Disable the option when Unbound is running as a (DHCP-) DNS network
    resolver for a group of machines, where such lookups should be filtered
    (RFC compliance), this also stops potential data leakage about the local
    network to the upstream DNS servers.

    Default: no


@@UAHL@unbound.conf@insecure-lan-zones@@: *<yes or no>*
    If enabled, then reverse lookups in private address space are not
    validated.
    This is usually required whenever
    :ref:`unblock-lan-zones<unbound.conf.unblock-lan-zones>` is used.

    Default: no


@@UAHL@unbound.conf@local-zone@@: *<zone> <type>*
    Configure a local zone.
    The type determines the answer to give if there is no match from
    :ref:`local-data<unbound.conf.local-data>`.
    The types are
    :ref:`deny<unbound.conf.local-zone.type.deny>`,
    :ref:`refuse<unbound.conf.local-zone.type.refuse>`,
    :ref:`static<unbound.conf.local-zone.type.static>`,
    :ref:`transparent<unbound.conf.local-zone.type.transparent>`,
    :ref:`redirect<unbound.conf.local-zone.type.redirect>`,
    :ref:`nodefault<unbound.conf.local-zone.type.nodefault>`,
    :ref:`typetransparent<unbound.conf.local-zone.type.typetransparent>`,
    :ref:`inform<unbound.conf.local-zone.type.inform>`,
    :ref:`inform_deny<unbound.conf.local-zone.type.inform_deny>`,
    :ref:`inform_redirect<unbound.conf.local-zone.type.inform_redirect>`,
    :ref:`always_transparent<unbound.conf.local-zone.type.always_transparent>`,
    :ref:`block_a<unbound.conf.local-zone.type.block_a>`,
    :ref:`always_refuse<unbound.conf.local-zone.type.always_refuse>`,
    :ref:`always_nxdomain<unbound.conf.local-zone.type.always_nxdomain>`,
    :ref:`always_null<unbound.conf.local-zone.type.always_null>`,
    :ref:`noview<unbound.conf.local-zone.type.noview>`,
    and are explained below.
    After that the default settings are listed.
    Use :ref:`local-data<unbound.conf.local-data>` to enter data into the
    local zone.
    Answers for local zones are authoritative DNS answers.
    By default the zones are class IN.

    If you need more complicated authoritative data, with referrals,
    wildcards, CNAME/DNAME support, or DNSSEC authoritative service,
    setup a :ref:`stub-zone<unbound.conf.stub>` for it as detailed in the
    stub zone section below.
    A :ref:`stub-zone<unbound.conf.stub>` can be used to have unbound
    send queries to another server, an authoritative server, to fetch the
    information.
    With a :ref:`forward-zone<unbound.conf.forward>`, unbound sends
    queries to a server that is a recursive server to fetch the information.
    With an :ref:`auth-zone<unbound.conf.auth>` a zone can be loaded from
    file and used, it can be used like a local zone for users downstream, or
    the :ref:`auth-zone<unbound.conf.auth>` information can be used to fetch
    information from when resolving like it is an upstream server.
    The :ref:`forward-zone<unbound.conf.forward>` and
    :ref:`auth-zone<unbound.conf.auth>` options are described in their
    sections below.
    If you want to perform filtering of the information that the users can
    fetch, the :ref:`local-zone<unbound.conf.local-zone>` and
    :ref:`local-data<unbound.conf.local-data>` statements allow for this, but
    also the :ref:`rpz<unbound.conf.rpz>` functionality can be used, described
    in the RPZ section.

    @@UAHL@unbound.conf.local-zone.type@deny@@
        Do not send an answer, drop the query.
        If there is a match from local data, the query is answered.

    @@UAHL@unbound.conf.local-zone.type@refuse@@
        Send an error message reply, with rcode REFUSED.
        If there is a match from local data, the query is answered.

    @@UAHL@unbound.conf.local-zone.type@static@@
        If there is a match from local data, the query is answered.
        Otherwise, the query is answered with NODATA or NXDOMAIN.
        For a negative answer a SOA is included in the answer if present as
        :ref:`local-data<unbound.conf.local-data>` for the zone apex domain.

    @@UAHL@unbound.conf.local-zone.type@transparent@@
        If there is a match from :ref:`local-data<unbound.conf.local-data>`,
        the query is answered.
        Otherwise if the query has a different name, the query is resolved
        normally.
        If the query is for a name given in
        :ref:`local-data<unbound.conf.local-data>` but no such type of data is
        given in localdata, then a NOERROR NODATA answer is returned.
        If no :ref:`local-zone<unbound.conf.local-zone>` is given
        :ref:`local-data<unbound.conf.local-data>` causes a transparent zone
        to be created by default.

    @@UAHL@unbound.conf.local-zone.type@typetransparent@@
        If there is a match from local data, the query is answered.
        If the query is for a different name, or for the same name but for a
        different type, the query is resolved normally.
        So, similar to
        :ref:`transparent<unbound.conf.local-zone.type.transparent>` but types
        that are not listed in local data are resolved normally, so if an A
        record is in the local data that does not cause a NODATA reply for AAAA
        queries.

    @@UAHL@unbound.conf.local-zone.type@redirect@@
        The query is answered from the local data for the zone name.
        There may be no local data beneath the zone name.
        This answers queries for the zone, and all subdomains of the zone with
        the local data for the zone.
        It can be used to redirect a domain to return a different address
        record to the end user, with:

        .. code-block:: text

            local-zone: "example.com." redirect
            local-data: "example.com. A 127.0.0.1"

        queries for ``www.example.com`` and ``www.foo.example.com`` are
        redirected, so that users with web browsers cannot access sites with
        suffix example.com.

    @@UAHL@unbound.conf.local-zone.type@inform@@
        The query is answered normally, same as
        :ref:`transparent<unbound.conf.local-zone.type.transparent>`.
        The client IP address (@portnumber) is printed to the logfile.
        The log message is:

        .. code-block:: text

            timestamp, unbound-pid, info: zonename inform IP@port queryname type class.

        This option can be used for normal resolution, but machines looking up
        infected names are logged, eg. to run antivirus on them.

    @@UAHL@unbound.conf.local-zone.type@inform_deny@@
        The query is dropped, like
        :ref:`deny<unbound.conf.local-zone.type.deny>`, and logged, like
        :ref:`inform<unbound.conf.local-zone.type.inform>`.
        Ie. find infected machines without answering the queries.

    @@UAHL@unbound.conf.local-zone.type@inform_redirect@@
        The query is redirected, like
        :ref:`redirect<unbound.conf.local-zone.type.redirect>`, and logged,
        like :ref:`inform<unbound.conf.local-zone.type.inform>`.
        Ie. answer queries with fixed data and also log the machines that ask.

    @@UAHL@unbound.conf.local-zone.type@always_transparent@@
        Like :ref:`transparent<unbound.conf.local-zone.type.transparent>`, but
        ignores local data and resolves normally.

    @@UAHL@unbound.conf.local-zone.type@block_a@@
        Like :ref:`transparent<unbound.conf.local-zone.type.transparent>`, but
        ignores local data and resolves normally all query types excluding A.
        For A queries it unconditionally returns NODATA.
        Useful in cases when there is a need to explicitly force all apps to
        use IPv6 protocol and avoid any queries to IPv4.

    @@UAHL@unbound.conf.local-zone.type@always_refuse@@
        Like :ref:`refuse<unbound.conf.local-zone.type.refuse>`, but ignores
        local data and refuses the query.

    @@UAHL@unbound.conf.local-zone.type@always_nxdomain@@
        Like :ref:`static<unbound.conf.local-zone.type.static>`, but ignores
        local data and returns NXDOMAIN for the query.

    @@UAHL@unbound.conf.local-zone.type@always_nodata@@
        Like :ref:`static<unbound.conf.local-zone.type.static>`, but ignores
        local data and returns NODATA for the query.

    @@UAHL@unbound.conf.local-zone.type@always_deny@@
        Like :ref:`deny<unbound.conf.local-zone.type.deny>`, but ignores local
        data and drops the query.

    @@UAHL@unbound.conf.local-zone.type@always_null@@
        Always returns ``0.0.0.0`` or ``::0`` for every name in the zone.
        Like :ref:`redirect<unbound.conf.local-zone.type.redirect>` with zero
        data for A and AAAA.
        Ignores local data in the zone.
        Used for some block lists.

    @@UAHL@unbound.conf.local-zone.type@noview@@
        Breaks out of that view and moves towards the global local zones for
        answer to the query.
        If the :ref:`view-first<unbound.conf.view.view-first>` is no, it'll
        resolve normally.
        If :ref:`view-first<unbound.conf.view.view-first>` is enabled, it'll
        break perform that step and check the global answers.
        For when the view has view specific overrides but some zone has to be
        answered from global local zone contents.

    @@UAHL@unbound.conf.local-zone.type@nodefault@@
        Used to turn off default contents for AS112 zones.
        The other types also turn off default contents for the zone.
        The :ref:`nodefault<unbound.conf.local-zone.type.nodefault>` option has
        no other effect than turning off default contents for the given zone.
        Use :ref:`nodefault<unbound.conf.local-zone.type.nodefault>` if you use
        exactly that zone, if you want to use a subzone, use
        :ref:`transparent<unbound.conf.local-zone.type.transparent>`.

    The default zones are localhost, reverse ``127.0.0.1`` and ``::1``, the
    ``home.arpa``, ``resolver.arpa``, ``service.arpa``, ``onion``, ``test``,
    ``invalid`` and the AS112 zones.
    The AS112 zones are reverse DNS zones for private use and reserved IP
    addresses for which the servers on the internet cannot provide correct
    answers.
    They are configured by default to give NXDOMAIN (no reverse information)
    answers.

    The defaults can be turned off by specifying your own
    :ref:`local-zone<unbound.conf.local-zone>` of that name, or using the
    :ref:`nodefault<unbound.conf.local-zone.type.nodefault>` type.
    Below is a list of the default zone contents.

    @@UAHL@unbound.conf.local-zone.defaults@localhost@@
        The IPv4 and IPv6 localhost information is given.
        NS and SOA records are provided for completeness and to satisfy some
        DNS update tools.
        Default content:

        .. code-block:: text

            local-zone: "localhost." redirect
            local-data: "localhost. 10800 IN NS localhost."
            local-data: "localhost. 10800 IN SOA localhost. nobody.invalid. 1 3600 1200 604800 10800"
            local-data: "localhost. 10800 IN A 127.0.0.1"
            local-data: "localhost. 10800 IN AAAA ::1"

    @@UAHL@unbound.conf.local-zone.defaults@reverse IPv4 loopback@@
        Default content:

        .. code-block:: text

            local-zone: "127.in-addr.arpa." static
            local-data: "127.in-addr.arpa. 10800 IN NS localhost."
            local-data: "127.in-addr.arpa. 10800 IN SOA localhost. nobody.invalid. 1 3600 1200 604800 10800"
            local-data: "1.0.0.127.in-addr.arpa. 10800 IN PTR localhost."

    @@UAHL@unbound.conf.local-zone.defaults@reverse IPv6 loopback@@
        Default content:

        .. code-block:: text

            local-zone: "1.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.ip6.arpa." static
            local-data: "1.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.ip6.arpa. 10800 IN NS localhost."
            local-data: "1.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.ip6.arpa. 10800 IN SOA localhost. nobody.invalid. 1 3600 1200 604800 10800"
            local-data: "1.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.ip6.arpa. 10800 IN PTR localhost."

    @@UAHL@unbound.conf.local-zone.defaults@home.arpa@@ (:rfc:`8375`)
        Default content:

        .. code-block:: text

            local-zone: "home.arpa." static
            local-data: "home.arpa. 10800 IN NS localhost."
            local-data: "home.arpa. 10800 IN SOA localhost. nobody.invalid. 1 3600 1200 604800 10800"

    @@UAHL@unbound.conf.local-zone.defaults@resolver.arpa@@ (:rfc:`9462`)
        Default content:

        .. code-block:: text

            local-zone: "resolver.arpa." static
            local-data: "resolver.arpa. 10800 IN NS localhost."
            local-data: "resolver.arpa. 10800 IN SOA localhost. nobody.invalid. 1 3600 1200 604800 10800"

    @@UAHL@unbound.conf.local-zone.defaults@service.arpa@@ (draft-ietf-dnssd-srp-25)
        Default content:

        .. code-block:: text

            local-zone: "service.arpa." static
            local-data: "service.arpa. 10800 IN NS localhost."
            local-data: "service.arpa. 10800 IN SOA localhost. nobody.invalid. 1 3600 1200 604800 10800"

    @@UAHL@unbound.conf.local-zone.defaults@onion@@ (:rfc:`7686`)
        Default content:

        .. code-block:: text

            local-zone: "onion." static
            local-data: "onion. 10800 IN NS localhost."
            local-data: "onion. 10800 IN SOA localhost. nobody.invalid. 1 3600 1200 604800 10800"

    @@UAHL@unbound.conf.local-zone.defaults@test@@ (:rfc:`6761`)
        Default content:

        .. code-block:: text

            local-zone: "test." static
            local-data: "test. 10800 IN NS localhost."
            local-data: "test. 10800 IN SOA localhost. nobody.invalid. 1 3600 1200 604800 10800"

    @@UAHL@unbound.conf.local-zone.defaults@invalid@@ (:rfc:`6761`)
        Default content:

        .. code-block:: text

            local-zone: "invalid." static
            local-data: "invalid. 10800 IN NS localhost."
            local-data: "invalid. 10800 IN SOA localhost. nobody.invalid. 1 3600 1200 604800 10800"

    @@UAHL@unbound.conf.local-zone.defaults@reverse local use zones@@ (:rfc:`1918`)
        Reverse data for zones ``10.in-addr.arpa``, ``16.172.in-addr.arpa`` to
        ``31.172.in-addr.arpa``, ``168.192.in-addr.arpa``.
        The :ref:`local-zone<unbound.conf.local-zone>` is set static and as
        :ref:`local-data<unbound.conf.local-data>` SOA and NS records are
        provided.

    @@UAHL@unbound.conf.local-zone.defaults@special-use IPv4 Addresses@@ (:rfc:`3330`)
        Reverse data for zones ``0.in-addr.arpa`` (this), ``254.169.in-addr.arpa`` (link-local),
        ``2.0.192.in-addr.arpa`` (TEST NET 1), ``100.51.198.in-addr.arpa``
        (TEST NET 2), ``113.0.203.in-addr.arpa`` (TEST NET 3),
        ``255.255.255.255.in-addr.arpa`` (broadcast).
        And from ``64.100.in-addr.arpa`` to ``127.100.in-addr.arpa`` (Shared
        Address Space).

    @@UAHL@unbound.conf.local-zone.defaults@reverse IPv6 unspecified@@ (:rfc:`4291`)
        Reverse data for zone
        ``0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.ip6.arpa.``

    @@UAHL@unbound.conf.local-zone.defaults@reverse IPv6 Locally Assigned Local Addresses@@ (:rfc:`4193`)
        Reverse data for zone ``D.F.ip6.arpa``.

    @@UAHL@unbound.conf.local-zone.defaults@reverse IPv6 Link Local Addresses@@ (:rfc:`4291`)
        Reverse data for zones ``8.E.F.ip6.arpa`` to ``B.E.F.ip6.arpa``.

    @@UAHL@unbound.conf.local-zone.defaults@reverse IPv6 Example Prefix@@
        Reverse data for zone ``8.B.D.0.1.0.0.2.ip6.arpa``.
        This zone is used for tutorials and examples.
        You can remove the block on this zone with:

        .. code-block:: text

            local-zone: 8.B.D.0.1.0.0.2.ip6.arpa. nodefault

    You can also selectively unblock a part of the zone by making that part
    transparent with a :ref:`local-zone<unbound.conf.local-zone>` statement.
    This also works with the other default zones.


@@UAHL@unbound.conf@local-data@@: *"<resource record string>"*
    Configure local data, which is served in reply to queries for it.
    The query has to match exactly unless you configure the
    :ref:`local-zone<unbound.conf.local-zone>` as redirect.
    If not matched exactly, the :ref:`local-zone<unbound.conf.local-zone>`
    type determines further processing.
    If :ref:`local-data<unbound.conf.local-data>` is configured that is not a
    subdomain of a :ref:`local-zone<unbound.conf.local-zone>`, a
    :ref:`transparent local-zone<unbound.conf.local-zone.type.transparent>` is
    configured.
    For record types such as TXT, use single quotes, as in:

    .. code-block:: text

        local-data: 'example. TXT "text"'

    .. note::
        If you need more complicated authoritative data, with referrals,
        wildcards, CNAME/DNAME support, or DNSSEC authoritative service, setup
        a :ref:`stub-zone<unbound.conf.stub>` for it as detailed in the stub
        zone section below.


@@UAHL@unbound.conf@local-data-ptr@@: *"IPaddr name"*
    Configure local data shorthand for a PTR record with the reversed IPv4 or
    IPv6 address and the host name.
    For example ``"192.0.2.4 www.example.com"``.
    TTL can be inserted like this: ``"2001:DB8::4 7200 www.example.com"``


@@UAHL@unbound.conf@local-zone-tag@@: *<zone> <"list of tags">*
    Assign tags to local zones.
    Tagged localzones will only be applied when the used
    :ref:`access-control<unbound.conf.access-control>` element has a matching
    tag.
    Tags must be defined in :ref:`define-tag<unbound.conf.define-tag>`.
    Enclose list of tags in quotes (``""``) and put spaces between tags.
    When there are multiple tags it checks if the intersection of the list of
    tags for the query and :ref:`local-zone-tag<unbound.conf.local-zone-tag>`
    is non-empty.


@@UAHL@unbound.conf@local-zone-override@@: *<zone> <IP netblock> <type>*
    Override the local zone type for queries from addresses matching netblock.
    Use this localzone type, regardless the type configured for the local zone
    (both tagged and untagged) and regardless the type configured using
    :ref:`access-control-tag-action<unbound.conf.access-control-tag-action>`.


@@UAHL@unbound.conf@response-ip@@: *<IP-netblock> <action>*
    This requires use of the ``respip`` module.

    If the IP address in an AAAA or A RR in the answer section of a response
    matches the specified IP netblock, the specified action will apply.
    *<action>* has generally the same semantics as that for
    :ref:`access-control-tag-action<unbound.conf.access-control-tag-action>`,
    but there are some exceptions.

    Actions for :ref:`response-ip<unbound.conf.response-ip>` are different
    from those for :ref:`local-zone<unbound.conf.local-zone>` in that in case
    of the former there is no point of such conditions as "the query matches it
    but there is no local data".
    Because of this difference, the semantics of
    :ref:`response-ip<unbound.conf.response-ip>` actions are modified or
    simplified as follows: The *static*, *refuse*, *transparent*,
    *typetransparent*, and *nodefault* actions are invalid for *response-ip*.
    Using any of these will cause the configuration to be rejected as faulty.
    The *deny* action is non-conditional, i.e. it always results in dropping
    the corresponding query.
    The resolution result before applying the *deny* action is still cached and
    can be used for other queries.


@@UAHL@unbound.conf@response-ip-data@@: *<IP-netblock> <"resource record string">*
    This requires use of the ``respip`` module.

    This specifies the action data for
    :ref:`response-ip<unbound.conf.response-ip>` with action being to redirect
    as specified by *<"resource record string">*.
    *<"Resource record string">* is similar to that of
    :ref:`access-control-tag-action<unbound.conf.access-control-tag-action>`,
    but it must be of either AAAA, A or CNAME types.
    If the *<IP-netblock>* is an IPv6/IPv4 prefix, the record must be AAAA/A
    respectively, unless it is a CNAME (which can be used for both versions of
    IP netblocks).
    If it is CNAME there must not be more than one
    :ref:`response-ip-data<unbound.conf.response-ip-data>` for the same
    *<IP-netblock>*.
    Also, CNAME and other types of records must not coexist for the same
    *<IP-netblock>*, following the normal rules for CNAME records.
    The textual domain name for the CNAME does not have to be explicitly
    terminated with a dot (``"."``); the root name is assumed to be the origin
    for the name.


@@UAHL@unbound.conf@response-ip-tag@@: *<IP-netblock> <"list of tags">*
    This requires use of the ``respip`` module.

    Assign tags to response *<IP-netblock>*.
    If the IP address in an AAAA or A RR in the answer section of a response
    matches the specified *<IP-netblock>*, the specified tags are assigned to
    the IP address.
    Then, if an :ref:`access-control-tag<unbound.conf.access-control-tag>` is
    defined for the client and it includes one of the tags for the response IP,
    the corresponding
    :ref:`access-control-tag-action<unbound.conf.access-control-tag-action>`
    will apply.
    Tag matching rule is the same as that for
    :ref:`access-control-tag<unbound.conf.access-control-tag>` and
    :ref:`local-zone<unbound.conf.local-zone>`.
    Unlike :ref:`local-zone-tag<unbound.conf.local-zone-tag>`,
    :ref:`response-ip-tag<unbound.conf.response-ip-tag>` can be defined for an
    *<IP-netblock>* even if no :ref:`response-ip<unbound.conf.response-ip>` is
    defined for that netblock.
    If multiple :ref:`response-ip-tag<unbound.conf.response-ip-tag>` options
    are specified for the same *<IP-netblock>* in different statements, all but
    the first will be ignored.
    However, this will not be flagged as a configuration error, but the result
    is probably not what was intended.

    Actions specified in an
    :ref:`access-control-tag-action<unbound.conf.access-control-tag-action>`
    that has a matching tag with
    :ref:`response-ip-tag<unbound.conf.response-ip-tag>` can be those that are
    "invalid" for :ref:`response-ip<unbound.conf.response-ip>` listed above,
    since
    :ref:`access-control-tag-action<unbound.conf.access-control-tag-action>`
    can be shared with local zones.
    For these actions, if they behave differently depending on whether local
    data exists or not in case of local zones, the behavior for
    :ref:`response-ip-data<unbound.conf.response-ip-data>` will generally
    result in NOERROR/NODATA instead of NXDOMAIN, since the
    :ref:`response-ip<unbound.conf.response-ip>` data are inherently type
    specific, and non-existence of data does not indicate anything about the
    existence or non-existence of the qname itself.
    For example, if the matching tag action is static but there is no data for
    the corresponding :ref:`response-ip<unbound.conf.response-ip>`
    configuration, then the result will be NOERROR/NODATA.
    The only case where NXDOMAIN is returned is when an
    :ref:`always_nxdomain<unbound.conf.local-zone.type.always_nxdomain>`
    action applies.


@@UAHL@unbound.conf@ratelimit@@: *<number or 0>*
    Enable ratelimiting of queries sent to nameserver for performing recursion.
    0 disables the feature.
    This option is experimental at this time.

    The ratelimit is in queries per second that are allowed.
    More queries are turned away with an error (SERVFAIL).
    Cached responses are not ratelimited by this setting.

    This stops recursive floods, eg. random query names, but not spoofed
    reflection floods.
    The zone of the query is determined by examining the nameservers for it,
    the zone name is used to keep track of the rate.
    For example, 1000 may be a suitable value to stop the server from being
    overloaded with random names, and keeps unbound from sending traffic to the
    nameservers for those zones.

    .. note:: Configured forwarders are excluded from ratelimiting.

    Default: 0


@@UAHL@unbound.conf@ratelimit-size@@: *<memory size>*
    Give the size of the data structure in which the current ongoing rates are
    kept track in.
    In bytes or use m(mega), k(kilo), g(giga).
    The ratelimit structure is small, so this data structure likely does not
    need to be large.

    Default: 4m


@@UAHL@unbound.conf@ratelimit-slabs@@: *<number>*
    Number of slabs in the ratelimit tracking data structure.
    Slabs reduce lock contention by threads.
    Must be set to a power of 2.
    Setting (close) to the number of cpus is a fairly good setting.
    If left unconfigured, it will be configured automatically to be a power of
    2 close to the number of configured threads in multi-threaded environments.

    Default: (unconfigured)


@@UAHL@unbound.conf@ratelimit-factor@@: *<number>*
    Set the amount of queries to rate limit when the limit is exceeded.
    If set to 0, all queries are dropped for domains where the limit is
    exceeded.
    If set to another value, 1 in that number is allowed through to complete.
    Default is 10, allowing 1/10 traffic to flow normally.
    This can make ordinary queries complete (if repeatedly queried for), and
    enter the cache, whilst also mitigating the traffic flow by the factor
    given.

    Default: 10


@@UAHL@unbound.conf@ratelimit-backoff@@: *<yes or no>*
    If enabled, the ratelimit is treated as a hard failure instead of the
    default maximum allowed constant rate.
    When the limit is reached, traffic is ratelimited and demand continues to
    be kept track of for a 2 second rate window.
    No traffic is allowed, except for
    :ref:`ratelimit-factor<unbound.conf.ratelimit-factor>`, until demand
    decreases below the configured ratelimit for a 2 second rate window.
    Useful to set :ref:`ratelimit<unbound.conf.ratelimit>` to a suspicious
    rate to aggressively limit unusually high traffic.

    Default: no


@@UAHL@unbound.conf@ratelimit-for-domain@@: *<domain> <number qps or 0>*
    Override the global :ref:`ratelimit<unbound.conf.ratelimit>` for an exact
    match domain name with the listed number.
    You can give this for any number of names.
    For example, for a top-level-domain you may want to have a higher limit
    than other names.
    A value of 0 will disable ratelimiting for that domain.


@@UAHL@unbound.conf@ratelimit-below-domain@@: *<domain> <number qps or 0>*
    Override the global :ref:`ratelimit<unbound.conf.ratelimit>` for a domain
    name that ends in this name.
    You can give this multiple times, it then describes different settings in
    different parts of the namespace.
    The closest matching suffix is used to determine the qps limit.
    The rate for the exact matching domain name is not changed, use
    :ref:`ratelimit-for-domain<unbound.conf.ratelimit-for-domain>` to set
    that, you might want to use different settings for a top-level-domain and
    subdomains.
    A value of 0 will disable ratelimiting for domain names that end in this
    name.


@@UAHL@unbound.conf@ip-ratelimit@@: *<number or 0>*
    Enable global ratelimiting of queries accepted per ip address.
    This option is experimental at this time.
    The ratelimit is in queries per second that are allowed.
    More queries are completely dropped and will not receive a reply, SERVFAIL
    or otherwise.
    IP ratelimiting happens before looking in the cache.
    This may be useful for mitigating amplification attacks.
    Clients with a valid DNS Cookie will bypass the ratelimit.
    If a ratelimit for such clients is still needed,
    :ref:`ip-ratelimit-cookie<unbound.conf.ip-ratelimit-cookie>`
    can be used instead.

    Default: 0 (disabled)


@@UAHL@unbound.conf@ip-ratelimit-cookie@@: *<number or 0>*
    Enable global ratelimiting of queries accepted per IP address with a valid
    DNS Cookie.
    This option is experimental at this time.
    The ratelimit is in queries per second that are allowed.
    More queries are completely dropped and will not receive a reply, SERVFAIL
    or otherwise.
    IP ratelimiting happens before looking in the cache.
    This option could be useful in combination with
    :ref:`allow_cookie<unbound.conf.access-control.action.allow_cookie>`, in an
    attempt to mitigate other amplification attacks than UDP reflections (e.g.,
    attacks targeting Unbound itself) which are already handled with DNS
    Cookies.
    If used, the value is suggested to be higher than
    :ref:`ip-ratelimit<unbound.conf.ip-ratelimit>` e.g., tenfold.

    Default: 0 (disabled)


@@UAHL@unbound.conf@ip-ratelimit-size@@: *<memory size>*
    Give the size of the data structure in which the current ongoing rates are
    kept track in.
    In bytes or use m(mega), k(kilo), g(giga).
    The IP ratelimit structure is small, so this data structure likely does not
    need to be large.

    Default: 4m


@@UAHL@unbound.conf@ip-ratelimit-slabs@@: *<number>*
    Number of slabs in the ip ratelimit tracking data structure.
    Slabs reduce lock contention by threads.
    Must be set to a power of 2.
    Setting (close) to the number of cpus is a fairly good setting.
    If left unconfigured, it will be configured automatically to be a power of
    2 close to the number of configured threads in multi-threaded environments.

    Default: (unconfigured)


@@UAHL@unbound.conf@ip-ratelimit-factor@@: *<number>*
    Set the amount of queries to rate limit when the limit is exceeded.
    If set to 0, all queries are dropped for addresses where the limit is
    exceeded.
    If set to another value, 1 in that number is allowed through to complete.
    Default is 10, allowing 1/10 traffic to flow normally.
    This can make ordinary queries complete (if repeatedly queried for), and
    enter the cache, whilst also mitigating the traffic flow by the factor
    given.

    Default: 10


@@UAHL@unbound.conf@ip-ratelimit-backoff@@: *<yes or no>*
    If enabled, the rate limit is treated as a hard failure instead of the
    default maximum allowed constant rate.
    When the limit is reached, traffic is ratelimited and demand continues to
    be kept track of for a 2 second rate window.
    No traffic is allowed, except for
    :ref:`ip-ratelimit-factor<unbound.conf.ip-ratelimit-factor>`, until demand
    decreases below the configured ratelimit for a 2 second rate window.
    Useful to set :ref:`ip-ratelimit<unbound.conf.ip-ratelimit>` to a
    suspicious rate to aggressively limit unusually high traffic.

    Default: no


@@UAHL@unbound.conf@outbound-msg-retry@@: *<number>*
    The number of retries, per upstream nameserver in a delegation, that
    Unbound will attempt in case a throwaway response is received.
    No response (timeout) contributes to the retry counter.
    If a forward/stub zone is used, this is the number of retries per
    nameserver in the zone.

    Default: 5


@@UAHL@unbound.conf@max-sent-count@@: *<number>*
    Hard limit on the number of outgoing queries Unbound will make while
    resolving a name, making sure large NS sets do not loop.
    Results in SERVFAIL when reached.
    It resets on query restarts (e.g., CNAME) and referrals.

    Default: 32


@@UAHL@unbound.conf@max-query-restarts@@: *<number>*
    Hard limit on the number of times Unbound is allowed to restart a query
    upon encountering a CNAME record.
    Results in SERVFAIL when reached.
    Changing this value needs caution as it can allow long CNAME chains to be
    accepted, where Unbound needs to verify (resolve) each link individually.

    Default: 11


@@UAHL@unbound.conf@iter-scrub-ns@@: *<number>*
    Limit on the number of NS records allowed in an rrset of type NS, from the
    iterator scrubber.
    This protects the internals of the resolver from overly large NS sets.

    Default: 20


@@UAHL@unbound.conf@iter-scrub-cname@@: *<number>*
    Limit on the number of CNAME, DNAME records in an answer, from the iterator
    scrubber.
    This protects the internals of the resolver from overly long indirection
    chains.
    Clips off the remainder of the reply packet at that point.

    Default: 11


@@UAHL@unbound.conf@max-global-quota@@: *<number>*
    Limit on the number of upstream queries sent out for an incoming query and
    its subqueries from recursion.
    It is not reset during the resolution.
    When it is exceeded the query is failed and the lookup process stops.

    Default: 200


@@UAHL@unbound.conf@iter-scrub-promiscuous@@: *<yes or no>*
    Should the iterator scrubber remove promiscuous NS from positive answers.
    This protects against poisonous contents, that could affect names in the
    same zone as a spoofed packet.

    Default: yes


@@UAHL@unbound.conf@fast-server-permil@@: *<number>*
    Specify how many times out of 1000 to pick from the set of fastest servers.
    0 turns the feature off.
    A value of 900 would pick from the fastest servers 90 percent of the time,
    and would perform normal exploration of random servers for the remaining
    time.
    When :ref:`prefetch<unbound.conf.prefetch>` is enabled (or
    :ref:`serve-expired<unbound.conf.serve-expired>`), such prefetches are not
    sped up, because there is no one waiting for it, and it presents a good
    moment to perform server exploration.
    The :ref:`fast-server-num<unbound.conf.fast-server-num>` option can be
    used to specify the size of the fastest servers set.

    Default: 0


@@UAHL@unbound.conf@fast-server-num@@: *<number>*
    Set the number of servers that should be used for fast server selection.
    Only use the fastest specified number of servers with the
    :ref:`fast-server-permil<unbound.conf.fast-server-permil>` option, that
    turns this on or off.

    Default: 3


@@UAHL@unbound.conf@answer-cookie@@: *<yes or no>*
    If enabled, Unbound will answer to requests containing DNS Cookies as
    specified in RFC 7873 and RFC 9018.

    Default: no


@@UAHL@unbound.conf@cookie-secret@@: *"<128 bit hex string>"*
    Server's secret for DNS Cookie generation.
    Useful to explicitly set for servers in an anycast deployment that need to
    share the secret in order to verify each other's Server Cookies.
    An example hex string would be "000102030405060708090a0b0c0d0e0f".

    .. note::
        This option is ignored if a
        :ref:`cookie-secret-file<unbound.conf.cookie-secret-file>` is present.
        In that case the secrets from that file are used in DNS Cookie
        calculations.

    Default: 128 bits random secret generated at startup time


@@UAHL@unbound.conf@cookie-secret-file@@: *<filename>*
    File from which the secrets are read used in DNS Cookie calculations.
    When this file exists, the secrets in this file are used and the secret
    specified by the
    :ref:`cookie-secret<unbound.conf.cookie-secret>` option is ignored.
    Enable it by setting a filename, like
    "/usr/local/etc/unbound_cookiesecrets.txt".
    The content of this file must be manipulated with the
    :ref:`add_cookie_secret<unbound-control.commands.add_cookie_secret>`,
    :ref:`drop_cookie_secret<unbound-control.commands.drop_cookie_secret>` and
    :ref:`activate_cookie_secret<unbound-control.commands.activate_cookie_secret>`
    commands to the :doc:`unbound-control(8)</manpages/unbound-control>` tool.
    Please see that manpage on how to perform a safe cookie secret rollover.

    Default: "" (disabled)


@@UAHL@unbound.conf@edns-client-string@@: *<IP netblock> <string>*
    Include an EDNS0 option containing configured ASCII string in queries with
    destination address matching the configured *<IP netblock>*.
    This configuration option can be used multiple times.
    The most specific match will be used.


@@UAHL@unbound.conf@edns-client-string-opcode@@: *<opcode>*
    EDNS0 option code for the
    :ref:`edns-client-string<unbound.conf.edns-client-string>` option, from 0
    to 65535.
    A value from the 'Reserved for Local/Experimental' range (65001-65534)
    should be used.

    Default: 65001


@@UAHL@unbound.conf@ede@@: *<yes or no>*
    If enabled, Unbound will respond with Extended DNS Error codes
    (:rfc:`8914`).
    These EDEs provide additional information with a response mainly for, but
    not limited to, DNS and DNSSEC errors.

    When the :ref:`val-log-level<unbound.conf.val-log-level>` option is also
    set to ``2``, responses with Extended DNS Errors concerning DNSSEC failures
    will also contain a descriptive text message about the reason for the
    failure.

    Default: no


@@UAHL@unbound.conf@ede-serve-expired@@: *<yes or no>*
    If enabled, Unbound will attach an Extended DNS Error (:rfc:`8914`) *Code 3
    - Stale Answer* as EDNS0 option to the expired response.

    .. note::
        :ref:`ede: yes<unbound.conf.ede>` needs to be set as well for this to
        work.

    Default: no


@@UAHL@unbound.conf@dns-error-reporting@@: *<yes or no>*
    If enabled, Unbound will send DNS Error Reports (:rfc:`9567`).
    The name servers need to express support by attaching the Report-Channel
    EDNS0 option on their replies specifying the reporting agent for the zone.
    Any errors encountered during resolution that would result in Unbound
    generating an Extended DNS Error (:rfc:`8914`) will be reported to the
    zone's reporting agent.

    The :ref:`ede<unbound.conf.ede>` option does not need to be enabled for
    this to work.

    It is advised that the
    :ref:`qname-minimisation<unbound.conf.qname-minimisation>` option is also
    enabled to increase privacy on the outgoing reports.

    Default: no

.. _unbound.conf.remote:

Remote Control Options
^^^^^^^^^^^^^^^^^^^^^^

In the **remote-control:** clause are the declarations for the remote control
facility.
If this is enabled, the :doc:`unbound-control(8)</manpages/unbound-control>`
utility can be used to send commands to the running Unbound server.
The server uses these clauses to setup TLSv1 security for the connection.
The :doc:`unbound-control(8)</manpages/unbound-control>` utility also reads the
**remote-control:** section for options.
To setup the correct self-signed certificates use the
*unbound-control-setup(8)* utility.


@@UAHL@unbound.conf.remote@control-enable@@: *<yes or no>*
    The option is used to enable remote control.
    If turned off, the server does not listen for control commands.

    Default: no


@@UAHL@unbound.conf.remote@control-interface@@: *<IP address or interface name or path>*
    Give IPv4 or IPv6 addresses or local socket path to listen on for control
    commands.
    If an interface name is used instead of an IP address, the list of IP
    addresses on that interface are used.

    By default localhost (``127.0.0.1`` and ``::1``) is listened to.
    Use ``0.0.0.0`` and ``::0`` to listen to all interfaces.
    If you change this and permissions have been dropped, you must restart the
    server for the change to take effect.

    If you set it to an absolute path, a unix domain socket is used.
    This socket does not use the certificates and keys, so those files need not
    be present.
    To restrict access, Unbound sets permissions on the file to the user and
    group that is configured, the access bits are set to allow the group
    members to access the control socket file.
    Put users that need to access the socket in the that group.
    To restrict access further, create a directory to put the control socket in
    and restrict access to that directory.


@@UAHL@unbound.conf.remote@control-port@@: *<port number>*
    The port number to listen on for IPv4 or IPv6 control interfaces.

    .. note::
        If you change this and permissions have been dropped, you must restart
        the server for the change to take effect.

    Default: 8953


@@UAHL@unbound.conf.remote@control-use-cert@@: *<yes or no>*
    For localhost
    :ref:`control-interface<unbound.conf.remote.control-interface>` you can
    disable the use of TLS by setting this option to "no".
    For local sockets, TLS is disabled and the value of this option is ignored.

    Default: yes


@@UAHL@unbound.conf.remote@server-key-file@@: *<private key file>*
    Path to the server private key.
    This file is generated by the
    :doc:`unbound-control-setup(8)</manpages/unbound-control>` utility.
    This file is used by the Unbound server, but not by
    :doc:`unbound-control(8)</manpages/unbound-control>`.

    Default: unbound_server.key


@@UAHL@unbound.conf.remote@server-cert-file@@: *<certificate file.pem>*
    Path to the server self signed certificate.
    This file is generated by the
    :doc:`unbound-control-setup(8)</manpages/unbound-control>` utility.
    This file is used by the Unbound server, and also by
    :doc:`unbound-control(8)</manpages/unbound-control>`.

    Default: unbound_server.pem


@@UAHL@unbound.conf.remote@control-key-file@@: *<private key file>*
    Path to the control client private key.
    This file is generated by the
    :doc:`unbound-control-setup(8)</manpages/unbound-control>` utility.
    This file is used by :doc:`unbound-control(8)</manpages/unbound-control>`.

    Default: unbound_control.key


@@UAHL@unbound.conf.remote@control-cert-file@@: *<certificate file.pem>*
    Path to the control client certificate.
    This certificate has to be signed with the server certificate.
    This file is generated by the
    :doc:`unbound-control-setup(8)</manpages/unbound-control>` utility.
    This file is used by :doc:`unbound-control(8)</manpages/unbound-control>`.

    Default: unbound_control.pem

.. _unbound.conf.stub:

Stub Zone Options
^^^^^^^^^^^^^^^^^

There may be multiple **stub-zone:** clauses.
Each with a :ref:`name<unbound.conf.stub.name>` and zero or more hostnames or
IP addresses.
For the stub zone this list of nameservers is used.
Class IN is assumed.
The servers should be authority servers, not recursors; Unbound performs the
recursive processing itself for stub zones.

The stub zone can be used to configure authoritative data to be used by the
resolver that cannot be accessed using the public internet servers.
This is useful for company-local data or private zones.
Setup an authoritative server on a different host (or different port).
Enter a config entry for Unbound with:

.. code-block:: text

   stub-addr: <ip address of host[@port]>

The Unbound resolver can then access the data, without referring to the public
internet for it.

This setup allows DNSSEC signed zones to be served by that authoritative
server, in which case a trusted key entry with the public key can be put in
config, so that Unbound can validate the data and set the AD bit on replies for
the private zone (authoritative servers do not set the AD bit).
This setup makes Unbound capable of answering queries for the private zone, and
can even set the AD bit ('authentic'), but the AA ('authoritative') bit is not
set on these replies.

Consider adding :ref:`server<unbound.conf.server>` statements for
:ref:`domain-insecure<unbound.conf.domain-insecure>` and for
:ref:`local-zone: \<name\> nodefault<unbound.conf.local-zone.type.nodefault>`
for the zone if it is a locally served zone.
The insecure clause stops DNSSEC from invalidating the zone.
The :ref:`local-zone: nodefault<unbound.conf.local-zone.type.nodefault>` (or
:ref:`transparent<unbound.conf.local-zone.type.transparent>`) clause makes the
(reverse-) zone bypass Unbound's filtering of :rfc:`1918` zones.


@@UAHL@unbound.conf.stub@name@@: *<domain name>*
    Name of the stub zone.
    This is the full domain name of the zone.


@@UAHL@unbound.conf.stub@stub-host@@: *<domain name>*
    Name of stub zone nameserver.
    Is itself resolved before it is used.

    To use a non-default port for DNS communication append ``'@'`` with the
    port number.

    If TLS is enabled, then you can append a ``'#'`` and a name, then it'll
    check the TLS authentication certificates with that name.

    If you combine the ``'@'`` and ``'#'``, the ``'@'`` comes first.
    If only ``'#'`` is used the default port is the configured
    :ref:`tls-port<unbound.conf.tls-port>`.


@@UAHL@unbound.conf.stub@stub-addr@@: *<IP address>*
    IP address of stub zone nameserver.
    Can be IPv4 or IPv6.

    To use a non-default port for DNS communication append ``'@'`` with the
    port number.

    If TLS is enabled, then you can append a ``'#'`` and a name, then it'll
    check the tls authentication certificates with that name.

    If you combine the ``'@'`` and ``'#'``, the ``'@'`` comes first.
    If only ``'#'`` is used the default port is the configured
    :ref:`tls-port<unbound.conf.tls-port>`.


@@UAHL@unbound.conf.stub@stub-prime@@: *<yes or no>*
    If enabled it performs NS set priming, which is similar to root hints,
    where it starts using the list of nameservers currently published by the
    zone.
    Thus, if the hint list is slightly outdated, the resolver picks up a
    correct list online.

    Default: no


@@UAHL@unbound.conf.stub@stub-first@@: *<yes or no>*
    If enabled, a query is attempted without the stub clause if it fails.
    The data could not be retrieved and would have caused SERVFAIL because the
    servers are unreachable, instead it is tried without this clause.

    Default: no


@@UAHL@unbound.conf.stub@stub-tls-upstream@@: *<yes or no>*
    Enabled or disable whether the queries to this stub use TLS for transport.

    Default: no


@@UAHL@unbound.conf.stub@stub-ssl-upstream@@: *<yes or no>*
    Alternate syntax for
    :ref:`stub-tls-upstream<unbound.conf.stub.stub-tls-upstream>`.


@@UAHL@unbound.conf.stub@stub-tcp-upstream@@: *<yes or no>*
    If it is set to "yes" then upstream queries use TCP only for transport
    regardless of global flag :ref:`tcp-upstream<unbound.conf.tcp-upstream>`.

    Default: no


@@UAHL@unbound.conf.stub@stub-no-cache@@: *<yes or no>*
    If enabled, data inside the stub is not cached.
    This is useful when you want immediate changes to be visible.

    Default: no

.. _unbound.conf.forward:

Forward Zone Options
^^^^^^^^^^^^^^^^^^^^

There may be multiple **forward-zone:** clauses.
Each with a :ref:`name<unbound.conf.forward.name>` and zero or more hostnames
or IP addresses.
For the forward zone this list of nameservers is used to forward the queries
to.
The servers listed as :ref:`forward-host<unbound.conf.forward.forward-host>`
and :ref:`forward-addr<unbound.conf.forward.forward-addr>` have to handle
further recursion for the query.
Thus, those servers are not authority servers, but are (just like Unbound is)
recursive servers too; Unbound does not perform recursion itself for the
forward zone, it lets the remote server do it.
Class IN is assumed.
CNAMEs are chased by Unbound itself, asking the remote server for every name in
the indirection chain, to protect the local cache from illegal indirect
referenced items.
A :ref:`forward-zone<unbound.conf.forward>` entry with name
``"."`` and a :ref:`forward-addr<unbound.conf.forward.forward-addr>` target
will forward all queries to that other server (unless it can answer from the
cache).


@@UAHL@unbound.conf.forward@name@@: *<domain name>*
    Name of the forward zone.
    This is the full domain name of the zone.


@@UAHL@unbound.conf.forward@forward-host@@: *<domain name>*
    Name of server to forward to.
    Is itself resolved before it is used.

    To use a non-default port for DNS communication append ``'@'`` with the
    port number.

    If TLS is enabled, then you can append a ``'#'`` and a name, then it'll
    check the TLS authentication certificates with that name.

    If you combine the ``'@'`` and ``'#'``, the ``'@'`` comes first.
    If only ``'#'`` is used the default port is the configured
    :ref:`tls-port<unbound.conf.tls-port>`.


@@UAHL@unbound.conf.forward@forward-addr@@: *<IP address>*
    IP address of server to forward to.
    Can be IPv4 or IPv6.

    To use a non-default port for DNS communication append ``'@'`` with the
    port number.

    If TLS is enabled, then you can append a ``'#'`` and a name, then it'll
    check the tls authentication certificates with that name.

    If you combine the ``'@'`` and ``'#'``, the ``'@'`` comes first.
    If only ``'#'`` is used the default port is the configured
    :ref:`tls-port<unbound.conf.tls-port>`.

    At high verbosity it logs the TLS certificate, with TLS enabled.
    If you leave out the ``'#'`` and auth name from the
    :ref:`forward-addr<unbound.conf.forward.forward-addr>`, any name is
    accepted.
    The cert must also match a CA from the
    :ref:`tls-cert-bundle<unbound.conf.tls-cert-bundle>`.


@@UAHL@unbound.conf.forward@forward-first@@: *<yes or no>*
    If a forwarded query is met with a SERVFAIL error, and this option is
    enabled, Unbound will fall back to normal recursive resolution for this
    query as if no query forwarding had been specified.

    Default: no


@@UAHL@unbound.conf.forward@forward-tls-upstream@@: *<yes or no>*
    Enabled or disable whether the queries to this forwarder use TLS for
    transport.
    If you enable this, also configure a
    :ref:`tls-cert-bundle<unbound.conf.tls-cert-bundle>` or use
    :ref:`tls-win-cert<unbound.conf.tls-win-cert>` to load CA certs, otherwise
    the connections cannot be authenticated.

    Default: no


@@UAHL@unbound.conf.forward@forward-ssl-upstream@@: *<yes or no>*
    Alternate syntax for
    :ref:`forward-tls-upstream<unbound.conf.forward.forward-tls-upstream>`.


@@UAHL@unbound.conf.forward@forward-tcp-upstream@@: *<yes or no>*
    If it is set to "yes" then upstream queries use TCP only for transport
    regardless of global flag :ref:`tcp-upstream<unbound.conf.tcp-upstream>`.

    Default: no


@@UAHL@unbound.conf.forward@forward-no-cache@@: *<yes or no>*
    If enabled, data inside the forward is not cached.
    This is useful when you want immediate changes to be visible.

    Default: no

.. _unbound.conf.auth:

Authority Zone Options
^^^^^^^^^^^^^^^^^^^^^^

Authority zones are configured with **auth-zone:**, and each one must have a
:ref:`name<unbound.conf.auth.name>`.
There can be multiple ones, by listing multiple auth-zone clauses, each with a
different name, pertaining to that part of the namespace.
The authority zone with the name closest to the name looked up is used.
Authority zones can be processed on two distinct, non-exclusive, configurable
stages.

With :ref:`for-downstream: yes<unbound.conf.auth.for-downstream>` (default),
authority zones are processed after **local-zones** and before cache.
When used in this manner, Unbound responds like an authority server with no
further processing other than returning an answer from the zone contents.
A notable example, in this case, is CNAME records which are returned verbatim
to downstream clients without further resolution.

With :ref:`for-upstream: yes<unbound.conf.auth.for-upstream>` (default),
authority zones are processed after the cache lookup, just before going to the
network to fetch information for recursion.
When used in this manner they provide a local copy of an authority server
that speeds up lookups for that data during resolving.

If both options are enabled (default), client queries for an authority zone are
answered authoritatively from Unbound, while internal queries that require data
from the authority zone consult the local zone data instead of going to the
network.

An interesting configuration is
:ref:`for-downstream: no<unbound.conf.auth.for-downstream>`,
:ref:`for-upstream: yes<unbound.conf.auth.for-upstream>`
that allows for hyperlocal behavior where both client and internal queries
consult the local zone data while resolving.
In this case, the aforementioned CNAME example will result in a thoroughly
resolved answer.

Authority zones can be read from zonefile.
And can be kept updated via AXFR and IXFR.
After update the zonefile is rewritten.
The update mechanism uses the SOA timer values and performs SOA UDP queries to
detect zone changes.

If the update fetch fails, the timers in the SOA record are used to time
another fetch attempt.
Until the SOA expiry timer is reached.
Then the zone is expired.
When a zone is expired, queries are SERVFAIL, and any new serial number is
accepted from the primary (even if older), and if fallback is enabled, the
fallback activates to fetch from the upstream instead of the SERVFAIL.


@@UAHL@unbound.conf.auth@name@@: *<zone name>*
    Name of the authority zone.


@@UAHL@unbound.conf.auth@primary@@: *<IP address or host name>*
    Where to download a copy of the zone from, with AXFR and IXFR.
    Multiple primaries can be specified.
    They are all tried if one fails.

    To use a non-default port for DNS communication append ``'@'`` with the
    port number.

    You can append a ``'#'`` and a name, then AXFR over TLS can be used and the
    TLS authentication certificates will be checked with that name.

    If you combine the ``'@'`` and ``'#'``, the ``'@'`` comes first.
    If you point it at another Unbound instance, it would not work because that
    does not support AXFR/IXFR for the zone, but if you used
    :ref:`url<unbound.conf.auth.url>` to download the zonefile as a text file
    from a webserver that would work.

    If you specify the hostname, you cannot use the domain from the zonefile,
    because it may not have that when retrieving that data, instead use a plain
    IP address to avoid a circular dependency on retrieving that IP address.


@@UAHL@unbound.conf.auth@master@@: *<IP address or host name>*
    Alternate syntax for :ref:`primary<unbound.conf.auth.primary>`.


@@UAHL@unbound.conf.auth@url@@: *<URL to zone file>*
    Where to download a zonefile for the zone.
    With HTTP or HTTPS.
    An example for the url is:

    .. code-block:: text

        http://www.example.com/example.org.zone

    Multiple url statements can be given, they are tried in turn.

    If only urls are given the SOA refresh timer is used to wait for making new
    downloads.
    If also primaries are listed, the primaries are first probed with UDP SOA
    queries to see if the SOA serial number has changed, reducing the number of
    downloads.
    If none of the urls work, the primaries are tried with IXFR and AXFR.

    For HTTPS, the :ref:`tls-cert-bundle<unbound.conf.tls-cert-bundle>` and
    the hostname from the url are used to authenticate the connection.

    If you specify a hostname in the URL, you cannot use the domain from the
    zonefile, because it may not have that when retrieving that data, instead
    use a plain IP address to avoid a circular dependency on retrieving that IP
    address.

    Avoid dependencies on name lookups by using a notation like
    ``"http://192.0.2.1/unbound-primaries/example.com.zone"``, with an explicit
    IP address.


@@UAHL@unbound.conf.auth@allow-notify@@: *<IP address or host name or netblockIP/prefix>*
    With :ref:`allow-notify<unbound.conf.auth.allow-notify>` you can specify
    additional sources of notifies.
    When notified, the server attempts to first probe and then zone transfer.
    If the notify is from a primary, it first attempts that primary.
    Otherwise other primaries are attempted.
    If there are no primaries, but only urls, the file is downloaded when
    notified.

    .. note::
        The primaries from :ref:`primary<unbound.conf.auth.primary>` and
        :ref:`url<unbound.conf.auth.url>` statements are allowed notify by
        default.


@@UAHL@unbound.conf.auth@fallback-enabled@@: *<yes or no>*
    If enabled, Unbound falls back to querying the internet as a resolver for
    this zone when lookups fail.
    For example for DNSSEC validation failures.

    Default: no


@@UAHL@unbound.conf.auth@for-downstream@@: *<yes or no>*
    If enabled, Unbound serves authority responses to downstream clients for
    this zone.
    This option makes Unbound behave, for the queries with names in this zone,
    like one of the authority servers for that zone.

    Turn it off if you want Unbound to provide recursion for the zone but have
    a local copy of zone data.

    If :ref:`for-downstream: no<unbound.conf.auth.for-downstream>` and
    :ref:`for-upstream: yes<unbound.conf.auth.for-upstream>` are set, then
    Unbound will DNSSEC validate the contents of the zone before serving the
    zone contents to clients and store validation results in the cache.

    Default: yes



@@UAHL@unbound.conf.auth@for-upstream@@: *<yes or no>*
    If enabled, Unbound fetches data from this data collection for answering
    recursion queries.
    Instead of sending queries over the internet to the authority servers for
    this zone, it'll fetch the data directly from the zone data.

    Turn it on when you want Unbound to provide recursion for downstream
    clients, and use the zone data as a local copy to speed up lookups.

    Default: yes


@@UAHL@unbound.conf.auth@zonemd-check@@: *<yes or no>*
    Enable this option to check ZONEMD records in the zone.
    The ZONEMD record is a checksum over the zone data.
    This includes glue in the zone and data from the zone file, and excludes
    comments from the zone file.
    When there is a DNSSEC chain of trust, DNSSEC signatures are checked too.

    Default: no


@@UAHL@unbound.conf.auth@zonemd-reject-absence@@: *<yes or no>*
    Enable this option to reject the absence of the ZONEMD record.
    Without it, when ZONEMD is not there it is not checked.

    It is useful to enable for a non-DNSSEC signed zone where the operator
    wants to require the verification of a ZONEMD, hence a missing ZONEMD is a
    failure.

    The action upon failure is controlled by the
    :ref:`zonemd-permissive-mode<unbound.conf.zonemd-permissive-mode>` option,
    for log only or also block the zone.

    Without the option, absence of a ZONEMD is only a failure when the zone is
    DNSSEC signed, and we have a trust anchor, and the DNSSEC verification of
    the absence of the ZONEMD fails.
    With the option enabled, the absence of a ZONEMD is always a failure, also
    for nonDNSSEC signed zones.

    Default: no


@@UAHL@unbound.conf.auth@zonefile@@: *<filename>*
    The filename where the zone is stored.
    If not given then no zonefile is used.
    If the file does not exist or is empty, Unbound will attempt to fetch zone
    data (eg. from the primary servers).

.. _unbound.conf.view:

View Options
^^^^^^^^^^^^

There may be multiple **view:** clauses.
Each with a :ref:`name<unbound.conf.view.name>` and zero or more
:ref:`local-zone<unbound.conf.view.local-zone>` and
:ref:`local-data<unbound.conf.view.local-data>` attributes.
Views can also contain :ref:`view-first<unbound.conf.view.view-first>`,
:ref:`response-ip<unbound.conf.response-ip>`,
:ref:`response-ip-data<unbound.conf.response-ip-data>` and
:ref:`local-data-ptr<unbound.conf.view.local-data-ptr>` attributes.
View can be mapped to requests by specifying the view name in an
:ref:`access-control-view<unbound.conf.access-control-view>` attribute.
Options from matching views will override global options.
Global options will be used if no matching view is found, or when the matching
view does not have the option specified.


@@UAHL@unbound.conf.view@name@@: *<view name>*
    Name of the view.
    Must be unique.
    This name is used in the
    :ref:`access-control-view<unbound.conf.access-control-view>` attribute.


@@UAHL@unbound.conf.view@local-zone@@: *<zone> <type>*
    View specific local zone elements.
    Has the same types and behaviour as the global
    :ref:`local-zone<unbound.conf.local-zone>` elements.
    When there is at least one *local-zone:* specified and :ref:`view-first:
    no<unbound.conf.view.view-first>` is set, the default local-zones will be
    added to this view.
    Defaults can be disabled using the nodefault type.
    When :ref:`view-first: yes<unbound.conf.view.view-first>` is set or when a
    view does not have a :ref:`local-zone<unbound.conf.view.local-zone>`, the
    global :ref:`local-zone<unbound.conf.local-zone>` will be used including
    it's default zones.


@@UAHL@unbound.conf.view@local-data@@: *"<resource record string>"*
    View specific local data elements.
    Has the same behaviour as the global
    :ref:`local-data<unbound.conf.local-data>` elements.


@@UAHL@unbound.conf.view@local-data-ptr@@: *"IPaddr name"*
    View specific local-data-ptr elements.
    Has the same behaviour as the global
    :ref:`local-data-ptr<unbound.conf.local-data-ptr>` elements.


@@UAHL@unbound.conf.view@view-first@@: *<yes or no>*
    If enabled, it attempts to use the global
    :ref:`local-zone<unbound.conf.local-zone>` and
    :ref:`local-data<unbound.conf.local-data>` if there is no match in the
    view specific options.

    Default: no

Python Module Options
^^^^^^^^^^^^^^^^^^^^^

The **python:** clause gives the settings for the *python(1)* script module.
This module acts like the iterator and validator modules do, on queries and
answers.
To enable the script module it has to be compiled into the daemon, and the word
``python`` has to be put in the
:ref:`module-config<unbound.conf.module-config>` option (usually first, or
between the validator and iterator).
Multiple instances of the python module are supported by adding the word
``python`` more than once.

If the :ref:`chroot<unbound.conf.chroot>` option is enabled, you should make
sure Python's library directory structure is bind mounted in the new root
environment, see *mount(8)*.
Also the :ref:`python-script<unbound.conf.python.python-script>` path should
be specified as an absolute path relative to the new root, or as a relative
path to the working directory.


@@UAHL@unbound.conf.python@python-script@@: *<python file>*
    The script file to load.
    Repeat this option for every python module instance added to the
    :ref:`module-config<unbound.conf.module-config>` option.

Dynamic Library Module Options
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The **dynlib:** clause gives the settings for the ``dynlib`` module.
This module is only a very small wrapper that allows dynamic modules to be
loaded on runtime instead of being compiled into the application.
To enable the dynlib module it has to be compiled into the daemon, and the word
``dynlib`` has to be put in the
:ref:`module-config<unbound.conf.module-config>` attribute.
Multiple instances of dynamic libraries are supported by adding the word
``dynlib`` more than once.

The :ref:`dynlib-file<unbound.conf.dynlib.dynlib-file>` path should be
specified as an absolute path relative to the new path set by
:ref:`chroot<unbound.conf.chroot>`, or as a relative path to the working
directory.


@@UAHL@unbound.conf.dynlib@dynlib-file@@: *<dynlib file>*
    The dynamic library file to load.
    Repeat this option for every dynlib module instance added to the
    :ref:`module-config<unbound.conf.module-config>` option.

DNS64 Module Options
^^^^^^^^^^^^^^^^^^^^

The ``dns64`` module must be configured in the
:ref:`module-config<unbound.conf.module-config>` directive, e.g.:

.. code-block:: text

    module-config: "dns64 validator iterator"

and be compiled into the daemon to be enabled.

.. note::
    These settings go in the :ref:`server:<unbound.conf.server>` section.


@@UAHL@unbound.conf.dns64@dns64-prefix@@: *<IPv6 prefix>*
    This sets the DNS64 prefix to use to synthesize AAAA records with.
    It must be /96 or shorter.

    Default: 64:ff9b::/96


@@UAHL@unbound.conf.dns64@dns64-synthall@@: *<yes or no>*
    .. warning:: Debugging feature!

    If enabled, synthesize all AAAA records despite the presence of actual AAAA
    records.

    Default: no


@@UAHL@unbound.conf.dns64@dns64-ignore-aaaa@@: *<domain name>*
    List domain for which the AAAA records are ignored and the A record is used
    by DNS64 processing instead.
    Can be entered multiple times, list a new domain for which it applies, one
    per line.
    Applies also to names underneath the name given.

NAT64 Operation
^^^^^^^^^^^^^^^

NAT64 operation allows using a NAT64 prefix for outbound requests to IPv4-only
servers.
It is controlled by two options in the
:ref:`server:<unbound.conf.server>` section:


@@UAHL@unbound.conf.nat64@do-nat64@@: *<yes or no>*
    Use NAT64 to reach IPv4-only servers.
    Consider also enabling :ref:`prefer-ip6<unbound.conf.prefer-ip6>`
    to prefer native IPv6 connections to nameservers.

    Default: no


@@UAHL@unbound.conf.nat64@nat64-prefix@@: *<IPv6 prefix>*
    Use a specific NAT64 prefix to reach IPv4-only servers.
    The prefix length must be one of /32, /40, /48, /56, /64 or /96.

    Default: 64:ff9b::/96 (same as :ref:`dns64-prefix<unbound.conf.dns64.dns64-prefix>`)

DNSCrypt Options
^^^^^^^^^^^^^^^^

The **dnscrypt:** clause gives the settings of the dnscrypt channel.
While those options are available, they are only meaningful if Unbound was
compiled with ``--enable-dnscrypt``.
Currently certificate and secret/public keys cannot be generated by Unbound.
You can use dnscrypt-wrapper to generate those:
https://github.com/cofyc/dnscrypt-wrapper/blob/master/README.md#usage


@@UAHL@unbound.conf.dnscrypt@dnscrypt-enable@@: *<yes or no>*
    Whether or not the dnscrypt config should be enabled.
    You may define configuration but not activate it.

    Default: no


@@UAHL@unbound.conf.dnscrypt@dnscrypt-port@@: *<port number>*
    On which port should dnscrypt should be activated.

    .. note::
        There should be a matching interface option defined in the
        :ref:`server:<unbound.conf.server>` section for this port.


@@UAHL@unbound.conf.dnscrypt@dnscrypt-provider@@: *<provider name>*
    The provider name to use to distribute certificates.
    This is of the form:

    .. code-block:: text

        2.dnscrypt-cert.example.com.

    .. important:: The name *MUST* end with a dot.


@@UAHL@unbound.conf.dnscrypt@dnscrypt-secret-key@@: *<path to secret key file>*
    Path to the time limited secret key file.
    This option may be specified multiple times.


@@UAHL@unbound.conf.dnscrypt@dnscrypt-provider-cert@@: *<path to cert file>*
    Path to the certificate related to the
    :ref:`dnscrypt-secret-key<unbound.conf.dnscrypt.dnscrypt-secret-key>`.
    This option may be specified multiple times.


@@UAHL@unbound.conf.dnscrypt@dnscrypt-provider-cert-rotated@@: *<path to cert file>*
    Path to a certificate that we should be able to serve existing connection
    from but do not want to advertise over
    :ref:`dnscrypt-provider<unbound.conf.dnscrypt.dnscrypt-provider>` 's TXT
    record certs distribution.

    A typical use case is when rotating certificates, existing clients may
    still use the client magic from the old cert in their queries until they
    fetch and update the new cert.
    Likewise, it would allow one to prime the new cert/key without distributing
    the new cert yet, this can be useful when using a network of servers using
    anycast and on which the configuration may not get updated at the exact
    same time.

    By priming the cert, the servers can handle both old and new certs traffic
    while distributing only one.

    This option may be specified multiple times.


@@UAHL@unbound.conf.dnscrypt@dnscrypt-shared-secret-cache-size@@: *<memory size>*
    Give the size of the data structure in which the shared secret keys are
    kept in.
    In bytes or use m(mega), k(kilo), g(giga).
    The shared secret cache is used when a same client is making multiple
    queries using the same public key.
    It saves a substantial amount of CPU.

    Default: 4m


@@UAHL@unbound.conf.dnscrypt@dnscrypt-shared-secret-cache-slabs@@: *<number>*
    Number of slabs in the dnscrypt shared secrets cache.
    Slabs reduce lock contention by threads.
    Must be set to a power of 2.
    Setting (close) to the number of cpus is a fairly good setting.
    If left unconfigured, it will be configured automatically to be a power of
    2 close to the number of configured threads in multi-threaded environments.

    Default: (unconfigured)


@@UAHL@unbound.conf.dnscrypt@dnscrypt-nonce-cache-size@@: *<memory size>*
    Give the size of the data structure in which the client nonces are kept in.
    In bytes or use m(mega), k(kilo), g(giga).
    The nonce cache is used to prevent dnscrypt message replaying.
    Client nonce should be unique for any pair of client pk/server sk.

    Default: 4m


@@UAHL@unbound.conf.dnscrypt@dnscrypt-nonce-cache-slabs@@: *<number>*
    Number of slabs in the dnscrypt nonce cache.
    Slabs reduce lock contention by threads.
    Must be set to a power of 2.
    Setting (close) to the number of cpus is a fairly good setting.
    If left unconfigured, it will be configured automatically to be a power of
    2 close to the number of configured threads in multi-threaded environments.

    Default: (unconfigured)

EDNS Client Subnet Module Options
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The ECS module must be configured in the
:ref:`module-config<unbound.conf.module-config>` directive, e.g.:

.. code-block:: text

    module-config: "subnetcache validator iterator"

and be compiled into the daemon to be enabled.

.. note::
    These settings go in the :ref:`server:<unbound.conf.server>` section.

If the destination address is allowed in the configuration Unbound will add the
EDNS0 option to the query containing the relevant part of the client's address.
When an answer contains the ECS option the response and the option are placed
in a specialized cache.
If the authority indicated no support, the response is stored in the regular
cache.

Additionally, when a client includes the option in its queries, Unbound will
forward the option when sending the query to addresses that are explicitly
allowed in the configuration using
:ref:`send-client-subnet<unbound.conf.ecs.send-client-subnet>`.
The option will always be forwarded, regardless the allowed addresses, when
:ref:`client-subnet-always-forward: yes<unbound.conf.ecs.client-subnet-always-forward>`
is set.
In this case the lookup in the regular cache is skipped.

The maximum size of the ECS cache is controlled by
:ref:`msg-cache-size<unbound.conf.msg-cache-size>` in the configuration file.
On top of that, for each query only 100 different subnets are allowed to be
stored for each address family.
Exceeding that number, older entries will be purged from cache.

Note that due to the nature of how EDNS Client Subnet works, by segregating the
client IP space in order to try and have tailored responses for prefixes of
unknown sizes, resolution and cache response performance are impacted as a
result.
Usage of the subnetcache module should only be enabled in installations that
require such functionality where the resolver and the clients belong to
different networks.
An example of that is an open resolver installation.

This module does not interact with the
:ref:`serve-expired\*<unbound.conf.serve-expired>` and
:ref:`prefetch<unbound.conf.prefetch>` options.


@@UAHL@unbound.conf.ecs@send-client-subnet@@: *<IP address>*
    Send client source address to this authority.
    Append /num to indicate a classless delegation netblock, for example like
    ``10.2.3.4/24`` or ``2001::11/64``.
    Can be given multiple times.
    Authorities not listed will not receive edns-subnet information, unless
    domain in query is specified in
    :ref:`client-subnet-zone<unbound.conf.ecs.client-subnet-zone>`.


@@UAHL@unbound.conf.ecs@client-subnet-zone@@: *<domain>*
    Send client source address in queries for this domain and its subdomains.
    Can be given multiple times.
    Zones not listed will not receive edns-subnet information, unless hosted by
    authority specified in
    :ref:`send-client-subnet<unbound.conf.ecs.send-client-subnet>`.


@@UAHL@unbound.conf.ecs@client-subnet-always-forward@@: *<yes or no>*
    Specify whether the ECS address check (configured using
    :ref:`send-client-subnet<unbound.conf.ecs.send-client-subnet>`) is applied
    for all queries, even if the triggering query contains an ECS record, or
    only for queries for which the ECS record is generated using the querier
    address (and therefore did not contain ECS data in the client query).
    If enabled, the address check is skipped when the client query contains an
    ECS record.
    And the lookup in the regular cache is skipped.

    Default: no


@@UAHL@unbound.conf.ecs@max-client-subnet-ipv6@@: *<number>*
    Specifies the maximum prefix length of the client source address we are
    willing to expose to third parties for IPv6.

    Default: 56


@@UAHL@unbound.conf.ecs@max-client-subnet-ipv4@@: *<number>*
    Specifies the maximum prefix length of the client source address we are
    willing to expose to third parties for IPv4.

    Default: 24


@@UAHL@unbound.conf.ecs@min-client-subnet-ipv6@@: *<number>*
    Specifies the minimum prefix length of the IPv6 source mask we are willing
    to accept in queries.
    Shorter source masks result in REFUSED answers.
    Source mask of 0 is always accepted.

    Default: 0


@@UAHL@unbound.conf.ecs@min-client-subnet-ipv4@@: *<number>*
    Specifies the minimum prefix length of the IPv4 source mask we are willing
    to accept in queries.
    Shorter source masks result in REFUSED answers.
    Source mask of 0 is always accepted.
    Default: 0


@@UAHL@unbound.conf.ecs@max-ecs-tree-size-ipv4@@: *<number>*
    Specifies the maximum number of subnets ECS answers kept in the ECS radix
    tree.
    This number applies for each qname/qclass/qtype tuple.

    Default: 100


@@UAHL@unbound.conf.ecs@max-ecs-tree-size-ipv6@@: *<number>*
    Specifies the maximum number of subnets ECS answers kept in the ECS radix
    tree.
    This number applies for each qname/qclass/qtype tuple.

    Default: 100

Opportunistic IPsec Support Module Options
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The IPsec module must be configured in the
:ref:`module-config<unbound.conf.module-config>` directive, e.g.:

.. code-block:: text

    module-config: "ipsecmod validator iterator"

and be compiled into Unbound by using ``--enable-ipsecmod`` to be enabled.

.. note::
    These settings go in the :ref:`server:<unbound.conf.server>` section.

When Unbound receives an A/AAAA query that is not in the cache and finds a
valid answer, it will withhold returning the answer and instead will generate
an IPSECKEY subquery for the same domain name.
If an answer was found, Unbound will call an external hook passing the
following arguments:

QNAME
    Domain name of the A/AAAA and IPSECKEY query.
    In string format.

IPSECKEY TTL
    TTL of the IPSECKEY RRset.

A/AAAA
    String of space separated IP addresses present in the A/AAAA RRset.
    The IP addresses are in string format.

IPSECKEY
    String of space separated IPSECKEY RDATA present in the IPSECKEY RRset.
    The IPSECKEY RDATA are in DNS presentation format.

The A/AAAA answer is then cached and returned to the client.
If the external hook was called the TTL changes to ensure it doesn't surpass
:ref:`ipsecmod-max-ttl<unbound.conf.ipsecmod-max-ttl>`.

The same procedure is also followed when
:ref:`prefetch: yes<unbound.conf.prefetch>` is set, but the A/AAAA answer is
given to the client before the hook is called.
:ref:`ipsecmod-max-ttl<unbound.conf.ipsecmod-max-ttl>` ensures that the A/AAAA
answer given from cache is still relevant for opportunistic IPsec.


@@UAHL@unbound.conf@ipsecmod-enabled@@: *<yes or no>*
    Specifies whether the IPsec module is enabled or not.
    The IPsec module still needs to be defined in the
    :ref:`module-config<unbound.conf.module-config>` directive.
    This option facilitates turning on/off the module without
    restarting/reloading Unbound.

    Default: yes


@@UAHL@unbound.conf@ipsecmod-hook@@: *<filename>*
    Specifies the external hook that Unbound will call with *system(3)*.
    The file can be specified as an absolute/relative path.
    The file needs the proper permissions to be able to be executed by the same
    user that runs Unbound.
    It must be present when the IPsec module is defined in the
    :ref:`module-config<unbound.conf.module-config>` directive.


@@UAHL@unbound.conf@ipsecmod-strict@@: *<yes or no>*
    If enabled Unbound requires the external hook to return a success value of
    0.
    Failing to do so Unbound will reply with SERVFAIL.
    The A/AAAA answer will also not be cached.

    Default: no


@@UAHL@unbound.conf@ipsecmod-max-ttl@@: *<seconds>*
    Time to live maximum for A/AAAA cached records after calling the external
    hook.

    Default: 3600


@@UAHL@unbound.conf@ipsecmod-ignore-bogus@@: *<yes or no>*
    Specifies the behaviour of Unbound when the IPSECKEY answer is bogus.
    If set to yes, the hook will be called and the A/AAAA answer will be
    returned to the client.
    If set to no, the hook will not be called and the answer to the A/AAAA
    query will be SERVFAIL.
    Mainly used for testing.

    Default: no


@@UAHL@unbound.conf@ipsecmod-allow@@: *<domain>*
    Allow the IPsec module functionality for the domain so that the module
    logic will be executed.
    Can be given multiple times, for different domains.
    If the option is not specified, all domains are treated as being allowed
    (default).


@@UAHL@unbound.conf@ipsecmod-whitelist@@: *<domain>*
    Alternate syntax for :ref:`ipsecmod-allow<unbound.conf.ipsecmod-allow>`.

Cache DB Module Options
^^^^^^^^^^^^^^^^^^^^^^^

The Cache DB module must be configured in the
:ref:`module-config<unbound.conf.module-config>` directive, e.g.:

.. code-block:: text

    module-config: "validator cachedb iterator"

and be compiled into the daemon with ``--enable-cachedb``.

If this module is enabled and configured, the specified backend database works
as a second level cache; when Unbound cannot find an answer to a query in its
built-in in-memory cache, it consults the specified backend.
If it finds a valid answer in the backend, Unbound uses it to respond to the
query without performing iterative DNS resolution.
If Unbound cannot even find an answer in the backend, it resolves the query as
usual, and stores the answer in the backend.

This module interacts with the *serve-expired-\** options and will reply with
expired data if Unbound is configured for that.

If Unbound was built with ``--with-libhiredis`` on a system that has installed
the hiredis C client library of Redis, then the ``redis`` backend can be used.
This backend communicates with the specified Redis server over a TCP connection
to store and retrieve cache data.
It can be used as a persistent and/or shared cache backend.

.. note::
    Unbound never removes data stored in the Redis server, even if some data
    have expired in terms of DNS TTL or the Redis server has cached too much
    data; if necessary the Redis server must be configured to limit the cache
    size, preferably with some kind of least-recently-used eviction policy.

Additionally, the
:ref:`redis-expire-records<unbound.conf.cachedb.redis-expire-records>` option
can be used in order to set the relative DNS TTL of the message as timeout to
the Redis records; keep in mind that some additional memory is used per key and
that the expire information is stored as absolute Unix timestamps in Redis
(computer time must be stable).

This backend uses synchronous communication with the Redis server based on the
assumption that the communication is stable and sufficiently fast.
The thread waiting for a response from the Redis server cannot handle other DNS
queries.
Although the backend has the ability to reconnect to the server when the
connection is closed unexpectedly and there is a configurable timeout in case
the server is overly slow or hangs up, these cases are assumed to be very rare.
If connection close or timeout happens too often, Unbound will be effectively
unusable with this backend.
It's the administrator's responsibility to make the assumption hold.

The **cachedb:** clause gives custom settings of the cache DB module.


@@UAHL@unbound.conf.cachedb@backend@@: *<backend name>*
    Specify the backend database name.
    The default database is the in-memory backend named ``testframe``, which,
    as the name suggests, is not of any practical use.
    Depending on the build-time configuration, ``redis`` backend may also be
    used as described above.

    Default: testframe


@@UAHL@unbound.conf.cachedb@secret-seed@@: *"<secret string>"*
    Specify a seed to calculate a hash value from query information.
    This value will be used as the key of the corresponding answer for the
    backend database and can be customized if the hash should not be
    predictable operationally.
    If the backend database is shared by multiple Unbound instances, all
    instances must use the same secret seed.

    Default: "default"


@@UAHL@unbound.conf.cachedb@cachedb-no-store@@: *<yes or no>*
    If the backend should be read from, but not written to.
    This makes this instance not store dns messages in the backend.
    But if data is available it is retrieved.

    Default: no


@@UAHL@unbound.conf.cachedb@cachedb-check-when-serve-expired@@: *<yes or no>*
    If enabled, the cachedb is checked before an expired response is returned.
    When
    :ref:`serve-expired<unbound.conf.serve-expired>`
    is enabled, without
    :ref:`serve-expired-client-timeout<unbound.conf.serve-expired-client-timeout>`
    , it then does not immediately respond with an expired response from cache,
    but instead first checks the cachedb for valid contents, and if so returns it.
    If the cachedb also has no valid contents, the serve expired response is sent.
    If also
    :ref:`serve-expired-client-timeout<unbound.conf.serve-expired-client-timeout>`
    is enabled, the expired response is delayed until the timeout expires.
    Unless the lookup succeeds within the timeout.

    Default: yes

The following **cachedb:** options are specific to the ``redis`` backend.


@@UAHL@unbound.conf.cachedb@redis-server-host@@: *<server address or name>*
    The IP (either v6 or v4) address or domain name of the Redis server.
    In general an IP address should be specified as otherwise Unbound will have
    to resolve the name of the server every time it establishes a connection to
    the server.

    Default: 127.0.0.1


@@UAHL@unbound.conf.cachedb@redis-server-port@@: *<port number>*
    The TCP port number of the Redis server.

    Default: 6379


@@UAHL@unbound.conf.cachedb@redis-server-path@@: *<unix socket path>*
    The unix socket path to connect to the Redis server.
    Unix sockets may have better throughput than the IP address option.

    Default: "" (disabled)


@@UAHL@unbound.conf.cachedb@redis-server-password@@: *"<password>"*
    The Redis AUTH password to use for the Redis server.
    Only relevant if Redis is configured for client password authorisation.

    Default: "" (disabled)


@@UAHL@unbound.conf.cachedb@redis-timeout@@: *<msec>*
    The period until when Unbound waits for a response from the Redis server.
    If this timeout expires Unbound closes the connection, treats it as if the
    Redis server does not have the requested data, and will try to re-establish
    a new connection later.

    Default: 100


@@UAHL@unbound.conf.cachedb@redis-command-timeout@@: *<msec>*
    The timeout to use for Redis commands, in milliseconds.
    If ``0``, it uses the
    :ref:`redis-timeout<unbound.conf.cachedb.redis-timeout>`
    value.

    Default: 0


@@UAHL@unbound.conf.cachedb@redis-connect-timeout@@: *<msec>*
    The timeout to use for Redis connection set up, in milliseconds.
    If ``0``, it uses the
    :ref:`redis-timeout<unbound.conf.cachedb.redis-timeout>`
    value.

    Default: 0


@@UAHL@unbound.conf.cachedb@redis-expire-records@@: *<yes or no>*
    If Redis record expiration is enabled.
    If yes, Unbound sets timeout for Redis records so that Redis can evict keys
    that have expired automatically.
    If Unbound is configured with
    :ref:`serve-expired<unbound.conf.serve-expired>` and
    :ref:`serve-expired-ttl: 0<unbound.conf.serve-expired-ttl>`, this option is
    internally reverted to "no".

    .. note::
        Redis "SET ... EX" support is required for this option (Redis >= 2.6.12).

    Default: no


@@UAHL@unbound.conf.cachedb@redis-logical-db@@: *<logical database index>*
    The logical database in Redis to use.
    These are databases in the same Redis instance sharing the same
    configuration and persisted in the same RDB/AOF file.
    If unsure about using this option, Redis documentation
    (https://redis.io/commands/select/) suggests not to use a single Redis
    instance for multiple unrelated applications.
    The default database in Redis is 0 while other logical databases need to be
    explicitly SELECT'ed upon connecting.

    Default: 0


@@UAHL@unbound.conf.cachedb@redis-replica-server-host@@: *<server address or name>*
    The IP (either v6 or v4) address or domain name of the Redis server.
    In general an IP address should be specified as otherwise Unbound will have
    to resolve the name of the server every time it establishes a connection to
    the server.

    This server is treated as a read-only replica server
    (https://redis.io/docs/management/replication/#read-only-replica).
    If specified, all Redis read commands will go to this replica server, while
    the write commands will go to the
    :ref:`redis-server-host<unbound.conf.cachedb.redis-server-host>`.

    Default: "" (disabled).


@@UAHL@unbound.conf.cachedb@redis-replica-server-port@@: *<port number>*
    The TCP port number of the Redis replica server.

    Default: 6379


@@UAHL@unbound.conf.cachedb@redis-replica-server-path@@: *<unix socket path>*
    The unix socket path to connect to the Redis replica server.
    Unix sockets may have better throughput than the IP address option.

    Default: "" (disabled)


@@UAHL@unbound.conf.cachedb@redis-replica-server-password@@: *"<password>"*
    The Redis AUTH password to use for the Redis server.
    Only relevant if Redis is configured for client password authorisation.

    Default: "" (disabled)


@@UAHL@unbound.conf.cachedb@redis-replica-timeout@@: *<msec>*
    The period until when Unbound waits for a response from the Redis replica
    server.
    If this timeout expires Unbound closes the connection, treats it as if the
    Redis server does not have the requested data, and will try to re-establish
    a new connection later.

    Default: 100


@@UAHL@unbound.conf.cachedb@redis-replica-command-timeout@@: *<msec>*
    The timeout to use for Redis replica commands, in milliseconds.
    If ``0``, it uses the
    :ref:`redis-replica-timeout<unbound.conf.cachedb.redis-replica-timeout>`
    value.

    Default: 0


@@UAHL@unbound.conf.cachedb@redis-replica-connect-timeout@@: *<msec>*
    The timeout to use for Redis replica connection set up, in milliseconds.
    If ``0``, it uses the
    :ref:`redis-replica-timeout<unbound.conf.cachedb.redis-replica-timeout>`
    value.

    Default: 0


@@UAHL@unbound.conf.cachedb@redis-replica-logical-db@@: *<logical database index>*
    Same as :ref:`redis-logical-db<unbound.conf.cachedb.redis-logical-db>` but
    for the Redis replica server.

    Default: 0


.. _unbound.conf.dnstap:

DNSTAP Logging Options
^^^^^^^^^^^^^^^^^^^^^^

DNSTAP support, when compiled in by using ``--enable-dnstap``, is enabled in
the **dnstap:** section.
This starts an extra thread (when compiled with threading) that writes the log
information to the destination.
If Unbound is compiled without threading it does not spawn a thread, but
connects per-process to the destination.


@@UAHL@unbound.conf.dnstap@dnstap-enable@@: *<yes or no>*
    If dnstap is enabled.
    If yes, it connects to the DNSTAP server and if any of the
    *dnstap-log-..-messages:* options is enabled it sends logs for those
    messages to the server.

    Default: no


@@UAHL@unbound.conf.dnstap@dnstap-bidirectional@@: *<yes or no>*
    Use frame streams in bidirectional mode to transfer DNSTAP messages.

    Default: yes


@@UAHL@unbound.conf.dnstap@dnstap-socket-path@@: *<file name>*
    Sets the unix socket file name for connecting to the server that is
    listening on that socket.

    Default: @DNSTAP_SOCKET_PATH@


@@UAHL@unbound.conf.dnstap@dnstap-ip@@: *<IPaddress[@port]>*
    If ``""``, the unix socket is used, if set with an IP address (IPv4 or
    IPv6) that address is used to connect to the server.

    Default: ""


@@UAHL@unbound.conf.dnstap@dnstap-tls@@: *<yes or no>*
    Set this to use TLS to connect to the server specified in
    :ref:`dnstap-ip<unbound.conf.dnstap.dnstap-ip>`.
    If set to no, TCP is used to connect to the server.

    Default: yes


@@UAHL@unbound.conf.dnstap@dnstap-tls-server-name@@: *<name of TLS authentication>*
    The TLS server name to authenticate the server with.
    Used when :ref:`dnstap-tls: yes<unbound.conf.dnstap.dnstap-tls>` is set.
    If ``""`` it is ignored.

    Default: ""


@@UAHL@unbound.conf.dnstap@dnstap-tls-cert-bundle@@: *<file name of cert bundle>*
    The pem file with certs to verify the TLS server certificate.
    If ``""`` the server default cert bundle is used, or the windows cert
    bundle on windows.

    Default: ""


@@UAHL@unbound.conf.dnstap@dnstap-tls-client-key-file@@: *<file name>*
    The client key file for TLS client authentication.
    If ``""`` client authentication is not used.

    Default: ""


@@UAHL@unbound.conf.dnstap@dnstap-tls-client-cert-file@@: *<file name>*
    The client cert file for TLS client authentication.

    Default: ""


@@UAHL@unbound.conf.dnstap@dnstap-send-identity@@: *<yes or no>*
    If enabled, the server identity is included in the log messages.

    Default: no


@@UAHL@unbound.conf.dnstap@dnstap-send-version@@: *<yes or no>*
    If enabled, the server version if included in the log messages.

    Default: no


@@UAHL@unbound.conf.dnstap@dnstap-identity@@: *<string>*
    The identity to send with messages, if ``""`` the hostname is used.

    Default: ""


@@UAHL@unbound.conf.dnstap@dnstap-version@@: *<string>*
    The version to send with messages, if ``""`` the package version is used.

    Default: ""


@@UAHL@unbound.conf.dnstap@dnstap-sample-rate@@: *<number>*
    The sample rate for log of messages, it logs only 1/N messages.
    With 0 it is disabled.
    This is useful in a high volume environment, where log functionality would
    otherwise not be reliable.
    For example 10 would spend only 1/10th time on logging, and 100 would only
    spend a hundredth of the time on logging.

    Default: 0 (disabled)


@@UAHL@unbound.conf.dnstap@dnstap-log-resolver-query-messages@@: *<yes or no>*
    Enable to log resolver query messages.
    These are messages from Unbound to upstream servers.

    Default: no


@@UAHL@unbound.conf.dnstap@dnstap-log-resolver-response-messages@@: *<yes or no>*
    Enable to log resolver response messages.
    These are replies from upstream servers to Unbound.

    Default: no


@@UAHL@unbound.conf.dnstap@dnstap-log-client-query-messages@@: *<yes or no>*
    Enable to log client query messages.
    These are client queries to Unbound.

    Default: no


@@UAHL@unbound.conf.dnstap@dnstap-log-client-response-messages@@: *<yes or no>*
    Enable to log client response messages.
    These are responses from Unbound to clients.

    Default: no


@@UAHL@unbound.conf.dnstap@dnstap-log-forwarder-query-messages@@: *<yes or no>*
    Enable to log forwarder query messages.

    Default: no


@@UAHL@unbound.conf.dnstap@dnstap-log-forwarder-response-messages@@: *<yes or no>*
    Enable to log forwarder response messages.

    Default: no

.. _unbound.conf.rpz:

Response Policy Zone Options
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Response Policy Zones are configured with **rpz:**, and each one must have a
:ref:`name<unbound.conf.rpz.name>` attribute.
There can be multiple ones, by listing multiple RPZ clauses, each with a
different name.
RPZ clauses are applied in order of configuration and any match from an earlier
RPZ zone will terminate the RPZ lookup.
Note that a PASSTHRU action is still considered a match.
The respip module needs to be added to the
:ref:`module-config<unbound.conf.module-config>`, e.g.:

.. code-block:: text

    module-config: "respip validator iterator"

QNAME, Response IP Address, nsdname, nsip and clientip triggers are supported.
Supported actions are: NXDOMAIN, NODATA, PASSTHRU, DROP, Local Data, tcp-only
and drop.
RPZ QNAME triggers are applied after any
:ref:`local-zone<unbound.conf.local-zone>` and before any
:ref:`auth-zone<unbound.conf.auth>`.

The RPZ zone is a regular DNS zone formatted with a SOA start record as usual.
The items in the zone are entries, that specify what to act on (the trigger)
and what to do (the action).
The trigger to act on is recorded in the name, the action to do is recorded as
the resource record.
The names all end in the zone name, so you could type the trigger names without
a trailing dot in the zonefile.

An example RPZ record, that answers ``example.com`` with ``NXDOMAIN``:

.. code-block:: text

    example.com CNAME .

The triggers are encoded in the name on the left

.. code-block:: text

    name                          query name
    netblock.rpz-client-ip        client IP address
    netblock.rpz-ip               response IP address in the answer
    name.rpz-nsdname              nameserver name
    netblock.rpz-nsip             nameserver IP address

The netblock is written as ``<netblocklen>.<ip address in reverse>``.
For IPv6 use ``'zz'`` for ``'::'``.
Specify individual addresses with scope length of 32 or 128.
For example, ``24.10.100.51.198.rpz-ip`` is ``198.51.100.10/24`` and
``32.10.zz.db8.2001.rpz-ip`` is ``2001:db8:0:0:0:0:0:10/32``.

The actions are specified with the record on the right

.. code-block:: text

    CNAME .                      nxdomain reply
    CNAME *.                     nodata reply
    CNAME rpz-passthru.          do nothing, allow to continue
    CNAME rpz-drop.              the query is dropped
    CNAME rpz-tcp-only.          answer over TCP
    A 192.0.2.1                  answer with this IP address

Other records like AAAA, TXT and other CNAMEs (not rpz-..) can also be used to
answer queries with that content.

The RPZ zones can be configured in the config file with these settings in the
**rpz:** block.


@@UAHL@unbound.conf.rpz@name@@: *<zone name>*
    Name of the authority zone.


@@UAHL@unbound.conf.rpz@primary@@: *<IP address or host name>*
    Where to download a copy of the zone from, with AXFR and IXFR.
    Multiple primaries can be specified.
    They are all tried if one fails.

    To use a non-default port for DNS communication append ``'@'`` with the
    port number.

    You can append a ``'#'`` and a name, then AXFR over TLS can be used and the
    TLS authentication certificates will be checked with that name.

    If you combine the ``'@'`` and ``'#'``, the ``'@'`` comes first.
    If you point it at another Unbound instance, it would not work because that
    does not support AXFR/IXFR for the zone, but if you used
    :ref:`url<unbound.conf.rpz.url>` to download the zonefile as a text file
    from a webserver that would work.

    If you specify the hostname, you cannot use the domain from the zonefile,
    because it may not have that when retrieving that data, instead use a plain
    IP address to avoid a circular dependency on retrieving that IP address.


@@UAHL@unbound.conf.rpz@master@@: *<IP address or host name>*
    Alternate syntax for :ref:`primary<unbound.conf.rpz.primary>`.


@@UAHL@unbound.conf.rpz@url@@: *<url to zonefile>*
    Where to download a zonefile for the zone.
    With HTTP or HTTPS.
    An example for the url is:

    .. code-block:: text

        http://www.example.com/example.org.zone

    Multiple url statements can be given, they are tried in turn.

    If only urls are given the SOA refresh timer is used to wait for making new
    downloads.
    If also primaries are listed, the primaries are first probed with UDP SOA
    queries to see if the SOA serial number has changed, reducing the number of
    downloads.
    If none of the URLs work, the primaries are tried with IXFR and AXFR.

    For HTTPS, the :ref:`tls-cert-bundle<unbound.conf.tls-cert-bundle>` and
    the hostname from the url are used to authenticate the connection.


@@UAHL@unbound.conf.rpz@allow-notify@@: *<IP address or host name or netblockIP/prefix>*
    With :ref:`allow-notify<unbound.conf.rpz.allow-notify>` you can specify
    additional sources of notifies.
    When notified, the server attempts to first probe and then zone transfer.
    If the notify is from a primary, it first attempts that primary.
    Otherwise other primaries are attempted.
    If there are no primaries, but only urls, the file is downloaded when
    notified.

    .. note::
        The primaries from :ref:`primary<unbound.conf.rpz.primary>` and
        :ref:`url<unbound.conf.rpz.url>` statements are allowed notify by
        default.


@@UAHL@unbound.conf.rpz@zonefile@@: *<filename>*
    The filename where the zone is stored.
    If not given then no zonefile is used.
    If the file does not exist or is empty, Unbound will attempt to fetch zone
    data (eg. from the primary servers).


@@UAHL@unbound.conf.rpz@rpz-action-override@@: *<action>*
    Always use this RPZ action for matching triggers from this zone.
    Possible actions are: *nxdomain*, *nodata*, *passthru*, *drop*, *disabled*
    and *cname*.


@@UAHL@unbound.conf.rpz@rpz-cname-override@@: *<domain>*
    The CNAME target domain to use if the cname action is configured for
    :ref:`rpz-action-override<unbound.conf.rpz.rpz-action-override>`.


@@UAHL@unbound.conf.rpz@rpz-log@@: *<yes or no>*
    Log all applied RPZ actions for this RPZ zone.

    Default: no


@@UAHL@unbound.conf.rpz@rpz-log-name@@: *<name>*
    Specify a string to be part of the log line, for easy referencing.


@@UAHL@unbound.conf.rpz@rpz-signal-nxdomain-ra@@: *<yes or no>*
    Signal when a query is blocked by the RPZ with NXDOMAIN with an unset RA
    flag.
    This allows certain clients, like dnsmasq, to infer that the domain is
    externally blocked.

    Default: no


@@UAHL@unbound.conf.rpz@for-downstream@@: *<yes or no>*
    If enabled the zone is authoritatively answered for and queries for the RPZ
    zone information are answered to downstream clients.
    This is useful for monitoring scripts, that can then access the SOA
    information to check if the RPZ information is up to date.

    Default: no


@@UAHL@unbound.conf.rpz@tags@@: *"<list of tags>"*
    Limit the policies from this RPZ clause to clients with a matching tag.

    Tags need to be defined in :ref:`define-tag<unbound.conf.define-tag>` and
    can be assigned to client addresses using
    :ref:`access-control-tag<unbound.conf.access-control-tag>` or
    :ref:`interface-tag<unbound.conf.interface-tag>`.
    Enclose list of tags in quotes (``""``) and put spaces between tags.

    If no tags are specified the policies from this clause will be applied for
    all clients.

Memory Control Example
----------------------

In the example config settings below memory usage is reduced.
Some service levels are lower, notable very large data and a high TCP load are
no longer supported.
Very large data and high TCP loads are exceptional for the DNS.
DNSSEC validation is enabled, just add trust anchors.
If you do not have to worry about programs using more than 3 Mb of memory, the
below example is not for you.
Use the defaults to receive full service, which on BSD-32bit tops out at 30-40
Mb after heavy usage.

.. code-block:: text

        # example settings that reduce memory usage
        server:
            num-threads: 1
            outgoing-num-tcp: 1 # this limits TCP service, uses less buffers.
            incoming-num-tcp: 1
            outgoing-range: 60  # uses less memory, but less performance.
            msg-buffer-size: 8192   # note this limits service, 'no huge stuff'.
            msg-cache-size: 100k
            msg-cache-slabs: 1
            rrset-cache-size: 100k
            rrset-cache-slabs: 1
            infra-cache-numhosts: 200
            infra-cache-slabs: 1
            key-cache-size: 100k
            key-cache-slabs: 1
            neg-cache-size: 10k
            num-queries-per-thread: 30
            target-fetch-policy: "2 1 0 0 0 0"
            harden-large-queries: "yes"
            harden-short-bufsize: "yes"

Files
-----

@UNBOUND_RUN_DIR@
    default Unbound working directory.

@UNBOUND_CHROOT_DIR@
    default *chroot(2)* location.

@ub_conf_file@
    Unbound configuration file.

@UNBOUND_PIDFILE@
    default Unbound pidfile with process ID of the running daemon.

unbound.log
    Unbound log file.
    Default is to log to *syslog(3)*.

See Also
--------

:doc:`unbound(8)</manpages/unbound>`,
:doc:`unbound-checkonf(8)</manpages/unbound-checkconf>`.
