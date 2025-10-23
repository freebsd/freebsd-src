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

.. program:: unbound-control

unbound-control(8)
==================

Synopsis
--------

**unbound-control** [``-hq``] [``-c cfgfile``] [``-s server``] command

Description
-----------

``unbound-control`` performs remote administration on the
:doc:`unbound(8)</manpages/unbound>` DNS server.
It reads the configuration file, contacts the Unbound server over TLS sends the
command and displays the result.

The available options are:

.. option:: -h

    Show the version and commandline option help.

.. option:: -c <cfgfile>

    The config file to read with settings.
    If not given the default config file
    :file:`@ub_conf_file@` is used.

.. option:: -s <server[@port]>

    IPv4 or IPv6 address of the server to contact.
    If not given, the address is read from the config file.

.. option:: -q

    Quiet, if the option is given it does not print anything if it works ok.

Commands
--------

There are several commands that the server understands.


@@UAHL@unbound-control.commands@start@@
    Start the server.
    Simply execs :doc:`unbound(8)</manpages/unbound>`.
    The ``unbound`` executable is searched for in the **PATH** set in the
    environment.
    It is started with the config file specified using :option:`-c` or the
    default config file.


@@UAHL@unbound-control.commands@stop@@
    Stop the server.
    The server daemon exits.


@@UAHL@unbound-control.commands@reload@@
    Reload the server.
    This flushes the cache and reads the config file fresh.


@@UAHL@unbound-control.commands@reload_keep_cache@@
    Reload the server but try to keep the RRset and message cache if
    (re)configuration allows for it.
    That means the caches sizes and the number of threads must not change
    between reloads.


@@UAHL@unbound-control.commands@fast_reload@@ [``+dpv``]
    Reload the server, but keep downtime to a minimum, so that user queries
    keep seeing service.
    This needs the code compiled with threads.
    The config is loaded in a thread, and prepared, then it briefly pauses the
    existing server and updates config options.
    The intent is that the pause does not impact the service of user queries.
    The cache is kept.
    Also user queries worked on are kept and continue, but with the new config
    options.

    .. note::
        This command is experimental at this time.

    The amount of temporal memory needed during a fast_reload is twice the
    amount needed for configuration.
    This is because Unbound temporarily needs to store both current
    configuration values and new ones while trying to fast_reload.
    Zones loaded from disk (authority zones and RPZ zones) are included in such
    memory needs.

    Options that can be changed are for
    :ref:`forwards<unbound.conf.forward>`,
    :ref:`stubs<unbound.conf.stub>`,
    :ref:`views<unbound.conf.view>`,
    :ref:`authority zones<unbound.conf.auth>`,
    :ref:`RPZ zones<unbound.conf.rpz>` and
    :ref:`local zones<unbound.conf.local-zone>`.

    Also
    :ref:`access-control<unbound.conf.access-control>` and similar options,
    :ref:`interface-action<unbound.conf.interface-action>` and similar
    options and
    :ref:`tcp-connection-limit<unbound.conf.tcp-connection-limit>`.
    It can reload some
    :ref:`define-tag<unbound.conf.define-tag>`
    changes, more on that below.
    Further options include
    :ref:`insecure-lan-zones<unbound.conf.insecure-lan-zones>`,
    :ref:`domain-insecure<unbound.conf.domain-insecure>`,
    :ref:`trust-anchor-file<unbound.conf.trust-anchor-file>`,
    :ref:`trust-anchor<unbound.conf.trust-anchor>`,
    :ref:`trusted-keys-file<unbound.conf.trusted-keys-file>`,
    :ref:`auto-trust-anchor-file<unbound.conf.auto-trust-anchor-file>`,
    :ref:`edns-client-string<unbound.conf.edns-client-string>`,
    ipset,
    :ref:`log-identity<unbound.conf.log-identity>`,
    :ref:`infra-cache-numhosts<unbound.conf.infra-cache-numhosts>`,
    :ref:`msg-cache-size<unbound.conf.msg-cache-size>`,
    :ref:`rrset-cache-size<unbound.conf.rrset-cache-size>`,
    :ref:`key-cache-size<unbound.conf.key-cache-size>`,
    :ref:`ratelimit-size<unbound.conf.ratelimit-size>`,
    :ref:`neg-cache-size<unbound.conf.neg-cache-size>`,
    :ref:`num-queries-per-thread<unbound.conf.num-queries-per-thread>`,
    :ref:`jostle-timeout<unbound.conf.jostle-timeout>`,
    :ref:`use-caps-for-id<unbound.conf.use-caps-for-id>`,
    :ref:`unwanted-reply-threshold<unbound.conf.unwanted-reply-threshold>`,
    :ref:`tls-use-sni<unbound.conf.tls-use-sni>`,
    :ref:`outgoing-tcp-mss<unbound.conf.outgoing-tcp-mss>`,
    :ref:`ip-dscp<unbound.conf.ip-dscp>`,
    :ref:`max-reuse-tcp-queries<unbound.conf.max-reuse-tcp-queries>`,
    :ref:`tcp-reuse-timeout<unbound.conf.tcp-reuse-timeout>`,
    :ref:`tcp-auth-query-timeout<unbound.conf.tcp-auth-query-timeout>`,
    :ref:`delay-close<unbound.conf.delay-close>`.
    :ref:`iter-scrub-promiscuous<unbound.conf.iter-scrub-promiscuous>`.

    It does not work with
    :ref:`interface<unbound.conf.interface>` and
    :ref:`outgoing-interface<unbound.conf.outgoing-interface>` changes,
    also not with
    :ref:`remote control<unbound.conf.remote>`,
    :ref:`outgoing-port-permit<unbound.conf.outgoing-port-permit>`,
    :ref:`outgoing-port-avoid<unbound.conf.outgoing-port-avoid>`,
    :ref:`msg-buffer-size<unbound.conf.msg-buffer-size>`,
    any **\*-slabs** options and
    :ref:`statistics-interval<unbound.conf.statistics-interval>` changes.

    For :ref:`dnstap<unbound.conf.dnstap>` these options can be changed:
    :ref:`dnstap-log-resolver-query-messages<unbound.conf.dnstap.dnstap-log-resolver-query-messages>`,
    :ref:`dnstap-log-resolver-response-messages<unbound.conf.dnstap.dnstap-log-resolver-response-messages>`,
    :ref:`dnstap-log-client-query-messages<unbound.conf.dnstap.dnstap-log-client-query-messages>`,
    :ref:`dnstap-log-client-response-messages<unbound.conf.dnstap.dnstap-log-client-response-messages>`,
    :ref:`dnstap-log-forwarder-query-messages<unbound.conf.dnstap.dnstap-log-forwarder-query-messages>` and
    :ref:`dnstap-log-forwarder-response-messages<unbound.conf.dnstap.dnstap-log-forwarder-response-messages>`.

    It does not work with these options:
    :ref:`dnstap-enable<unbound.conf.dnstap.dnstap-enable>`,
    :ref:`dnstap-bidirectional<unbound.conf.dnstap.dnstap-bidirectional>`,
    :ref:`dnstap-socket-path<unbound.conf.dnstap.dnstap-socket-path>`,
    :ref:`dnstap-ip<unbound.conf.dnstap.dnstap-ip>`,
    :ref:`dnstap-tls<unbound.conf.dnstap.dnstap-tls>`,
    :ref:`dnstap-tls-server-name<unbound.conf.dnstap.dnstap-tls-server-name>`,
    :ref:`dnstap-tls-cert-bundle<unbound.conf.dnstap.dnstap-tls-cert-bundle>`,
    :ref:`dnstap-tls-client-key-file<unbound.conf.dnstap.dnstap-tls-client-key-file>` and
    :ref:`dnstap-tls-client-cert-file<unbound.conf.dnstap.dnstap-tls-client-cert-file>`.

    The options
    :ref:`dnstap-send-identity<unbound.conf.dnstap.dnstap-send-identity>`,
    :ref:`dnstap-send-version<unbound.conf.dnstap.dnstap-send-version>`,
    :ref:`dnstap-identity<unbound.conf.dnstap.dnstap-identity>`, and
    :ref:`dnstap-version<unbound.conf.dnstap.dnstap-version>` can be loaded
    when ``+p`` is not used.

    The ``+v`` option makes the output verbose which includes the time it took
    to do the reload.
    With ``+vv`` it is more verbose which includes the amount of memory that
    was allocated temporarily to perform the reload; this amount of memory can
    be big if the config has large contents.
    In the timing output the 'reload' time is the time during which the server
    was paused.

    The ``+p`` option makes the reload not pause threads, they keep running.
    Locks are acquired, but items are updated in sequence, so it is possible
    for threads to see an inconsistent state with some options from the old
    and some options from the new config, such as cache TTL parameters from the
    old config and forwards from the new config.
    The stubs and forwards are updated at the same time, so that they are
    viewed consistently, either old or new values together.
    The option makes the reload time take eg. 3 microseconds instead of 0.3
    milliseconds during which the worker threads are interrupted.
    So, the interruption is much shorter, at the expense of some inconsistency.
    After the reload itself, every worker thread is briefly contacted to make
    them release resources, this makes the delete timing a little longer, and
    takes up time from the remote control servicing worker thread.

    With the nopause option (``+p``), the reload does not work to reload some
    options, that fast reload works on without the nopause option:
    :ref:`val-bogus-ttl<unbound.conf.val-bogus-ttl>`,
    :ref:`val-override-date<unbound.conf.val-override-date>`,
    :ref:`val-sig-skew-min<unbound.conf.val-sig-skew-min>`,
    :ref:`val-sig-skew-max<unbound.conf.val-sig-skew-max>`,
    :ref:`val-max-restart<unbound.conf.val-max-restart>`,
    :ref:`val-nsec3-keysize-iterations<unbound.conf.val-nsec3-keysize-iterations>`,
    :ref:`target-fetch-policy<unbound.conf.target-fetch-policy>`,
    :ref:`outbound-msg-retry<unbound.conf.outbound-msg-retry>`,
    :ref:`max-sent-count<unbound.conf.max-sent-count>`,
    :ref:`max-query-restarts<unbound.conf.max-query-restarts>`,
    :ref:`do-not-query-address<unbound.conf.do-not-query-address>`,
    :ref:`do-not-query-localhost<unbound.conf.do-not-query-localhost>`,
    :ref:`private-address<unbound.conf.private-address>`,
    :ref:`private-domain<unbound.conf.private-domain>`,
    :ref:`caps-exempt<unbound.conf.caps-exempt>`,
    :ref:`nat64-prefix<unbound.conf.nat64.nat64-prefix>`,
    :ref:`do-nat64<unbound.conf.nat64.do-nat64>`,
    :ref:`infra-host-ttl<unbound.conf.infra-host-ttl>`,
    :ref:`infra-keep-probing<unbound.conf.infra-keep-probing>`,
    :ref:`ratelimit<unbound.conf.ratelimit>`,
    :ref:`ip-ratelimit<unbound.conf.ip-ratelimit>`,
    :ref:`ip-ratelimit-cookie<unbound.conf.ip-ratelimit-cookie>`,
    :ref:`wait-limit-netblock<unbound.conf.wait-limit-netblock>`,
    :ref:`wait-limit-cookie-netblock<unbound.conf.wait-limit-cookie-netblock>`,
    :ref:`ratelimit-below-domain<unbound.conf.ratelimit-below-domain>`,
    :ref:`ratelimit-for-domain<unbound.conf.ratelimit-for-domain>`.

    The ``+d`` option makes the reload drop queries that the worker threads are
    working on.
    This is like
    :ref:`flush_requestlist<unbound-control.commands.flush_requestlist>`.
    Without it the queries are kept so that users keep getting answers for
    those queries that are currently processed.
    The drop makes it so that queries during the life time of the
    query processing see only old, or only new config options.

    When there are changes to the config tags, from the
    :ref:`define-tag<unbound.conf.define-tag>` option,
    then the ``+d`` option is implicitly turned on with a warning printout, and
    queries are dropped.
    This is to stop references to the old tag information, by the old
    queries.
    If the number of tags is increased in the newly loaded config, by
    adding tags at the end, then the implicit ``+d`` option is not needed.

    For response ip, that is actions associated with IP addresses, and perhaps
    intersected with access control tag and action information, those settings
    are stored with a query when it comes in based on its source IP address.
    The old information is kept with the query until the queries are done.
    This is gone when those queries are resolved and finished, or it is
    possible to flush the requestlist with ``+d``.


@@UAHL@unbound-control.commands@verbosity@@ *number*
    Change verbosity value for logging.
    Same values as the **verbosity:** keyword in
    :doc:`unbound.conf(5)</manpages/unbound.conf>`.
    This new setting lasts until the server is issued a reload (taken from
    config file again), or the next verbosity control command.


@@UAHL@unbound-control.commands@log_reopen@@
    Reopen the logfile, close and open it.
    Useful for logrotation to make the daemon release the file it is logging
    to.
    If you are using syslog it will attempt to close and open the syslog (which
    may not work if chrooted).


@@UAHL@unbound-control.commands@stats@@
    Print statistics.
    Resets the internal counters to zero, this can be controlled using the
    **statistics-cumulative:** config statement.
    Statistics are printed with one ``[name]: [value]`` per line.


@@UAHL@unbound-control.commands@stats_noreset@@
    Peek at statistics.
    Prints them like the stats command does, but does not reset the internal
    counters to zero.


@@UAHL@unbound-control.commands@status@@
    Display server status.
    Exit code 3 if not running (the connection to the port is refused), 1 on
    error, 0 if running.


@@UAHL@unbound-control.commands@local_zone@@ *name type*
    Add new local zone with name and type.
    Like local-zone config statement.
    If the zone already exists, the type is changed to the given argument.


@@UAHL@unbound-control.commands@local_zone_remove@@ *name*
    Remove the local zone with the given name.
    Removes all local data inside it.
    If the zone does not exist, the command succeeds.


@@UAHL@unbound-control.commands@local_data@@ *RR data...*
    Add new local data, the given resource record.
    Like **local-data:** keyword, except for when no covering zone exists.
    In that case this remote control command creates a transparent zone with
    the same name as this record.


@@UAHL@unbound-control.commands@local_data_remove@@ *name*
    Remove all RR data from local name.
    If the name already has no items, nothing happens.
    Often results in NXDOMAIN for the name (in a static zone), but if the name
    has become an empty nonterminal (there is still data in domain names below
    the removed name), NOERROR nodata answers are the result for that name.


@@UAHL@unbound-control.commands@local_zones@@
    Add local zones read from stdin of unbound-control.
    Input is read per line, with name space type on a line.
    For bulk additions.


@@UAHL@unbound-control.commands@local_zones_remove@@
    Remove local zones read from stdin of unbound-control.
    Input is one name per line.
    For bulk removals.


@@UAHL@unbound-control.commands@local_datas@@
    Add local data RRs read from stdin of unbound-control.
    Input is one RR per line.
    For bulk additions.


@@UAHL@unbound-control.commands@local_datas_remove@@
    Remove local data RRs read from stdin of unbound-control.
    Input is one name per line.
    For bulk removals.


@@UAHL@unbound-control.commands@dump_cache@@
    The contents of the cache is printed in a text format to stdout.
    You can redirect it to a file to store the cache in a file.
    Not supported in remote Unbounds in multi-process operation.


@@UAHL@unbound-control.commands@load_cache@@
    The contents of the cache is loaded from stdin.
    Uses the same format as dump_cache uses.
    Loading the cache with old, or wrong data can result in old or wrong data
    returned to clients.
    Loading data into the cache in this way is supported in order to aid with
    debugging.
    Not supported in remote Unbounds in multi-process operation.


@@UAHL@unbound-control.commands@cache_lookup@@ [``+t``] *names*
    Print to stdout the RRsets and messages that are in the cache.
    For every name listed the content at or under the name is printed.
    Several names separated by spaces can be given, each is printed.
    When subnetcache is enabled, also matching entries from the subnet
    cache are printed.

    The ``+t`` option allows tld and root names.
    With it names like 'com' and '.' can be used, but it takes a lot of
    effort to look up in the cache.


@@UAHL@unbound-control.commands@lookup@@ *name*
    Print to stdout the name servers that would be used to look up the name
    specified.


@@UAHL@unbound-control.commands@flush@@ [``+c``] *name*
    Remove the name from the cache.
    Removes the types A, AAAA, NS, SOA, CNAME, DNAME, MX, PTR, SRV, NAPTR,
    SVCB and HTTPS.
    Because that is fast to do.
    Other record types can be removed using **flush_type** or **flush_zone**.

    The ``+c`` option removes the items also from the cachedb cache.
    If cachedb is in use.


@@UAHL@unbound-control.commands@flush_type@@ [``+c``] *name type*
    Remove the name, type information from the cache.

    The ``+c`` option removes the items also from the cachedb cache.
    If cachedb is in use.


@@UAHL@unbound-control.commands@flush_zone@@ [``+c``] name
    Remove all information at or below the name from the cache.
    The rrsets and key entries are removed so that new lookups will be
    performed.
    This needs to walk and inspect the entire cache, and is a slow operation.
    The entries are set to expired in the implementation of this command (so,
    with serve-expired enabled, it'll serve that information but schedule a
    prefetch for new information).

    The ``+c`` option removes the items also from the cachedb cache.
    If cachedb is in use.


@@UAHL@unbound-control.commands@flush_bogus@@ [``+c``]
    Remove all bogus data from the cache.

    The ``+c`` option removes the items also from the cachedb cache.
    If cachedb is in use.


@@UAHL@unbound-control.commands@flush_negative@@ [``+c``]
    Remove all negative data from the cache.
    This is nxdomain answers, nodata answers and servfail answers.
    Also removes bad key entries (which could be due to failed lookups) from
    the dnssec key cache, and iterator last-resort lookup failures from the
    rrset cache.

    The ``+c`` option removes the items also from the cachedb cache.
    If cachedb is in use.


@@UAHL@unbound-control.commands@flush_stats@@
    Reset statistics to zero.


@@UAHL@unbound-control.commands@flush_requestlist@@
    Drop the queries that are worked on.
    Stops working on the queries that the server is working on now.
    The cache is unaffected.
    No reply is sent for those queries, probably making those users request
    again later.
    Useful to make the server restart working on queries with new settings,
    such as a higher verbosity level.


@@UAHL@unbound-control.commands@dump_requestlist@@
    Show what is worked on.
    Prints all queries that the server is currently working on.
    Prints the time that users have been waiting.
    For internal requests, no time is printed.
    And then prints out the module status.
    This prints the queries from the first thread, and not queries that are
    being serviced from other threads.


@@UAHL@unbound-control.commands@flush_infra@@ *all|IP*
    If all then entire infra cache is emptied.
    If a specific IP address, the entry for that address is removed from the
    cache.
    It contains EDNS, ping and lameness data.


@@UAHL@unbound-control.commands@dump_infra@@
    Show the contents of the infra cache.


@@UAHL@unbound-control.commands@set_option@@ *opt: val*
    Set the option to the given value without a reload.
    The cache is therefore not flushed.
    The option must end with a ``':'`` and whitespace must be between the
    option and the value.
    Some values may not have an effect if set this way, the new values are not
    written to the config file, not all options are supported.
    This is different from the set_option call in libunbound, where all values
    work because Unbound has not been initialized.

    The values that work are: statistics-interval, statistics-cumulative,
    do-not-query-localhost,  harden-short-bufsize, harden-large-queries,
    harden-glue, harden-dnssec-stripped, harden-below-nxdomain,
    harden-referral-path,  prefetch, prefetch-key, log-queries, hide-identity,
    hide-version, identity, version, val-log-level, val-log-squelch,
    ignore-cd-flag, add-holddown, del-holddown, keep-missing, tcp-upstream,
    ssl-upstream, max-udp-size, ratelimit, ip-ratelimit, cache-max-ttl,
    cache-min-ttl, cache-max-negative-ttl.


@@UAHL@unbound-control.commands@get_option@@ *opt*
    Get the value of the option.
    Give the option name without a trailing ``':'``.
    The value is printed.
    If the value is ``""``, nothing is printed and the connection closes.
    On error ``'error ...'`` is printed (it gives a syntax error on unknown
    option).
    For some options a list of values, one on each line, is printed.
    The options are shown from the config file as modified with set_option.
    For some options an override may have been taken that does not show up with
    this command, not results from e.g. the verbosity and forward control
    commands.
    Not all options work, see list_stubs, list_forwards, list_local_zones and
    list_local_data for those.


@@UAHL@unbound-control.commands@list_stubs@@
    List the stub zones in use.
    These are printed one by one to the output.
    This includes the root hints in use.


@@UAHL@unbound-control.commands@list_forwards@@
    List the forward zones in use.
    These are printed zone by zone to the output.


@@UAHL@unbound-control.commands@list_insecure@@
    List the zones with domain-insecure.


@@UAHL@unbound-control.commands@list_local_zones@@
    List the local zones in use.
    These are printed one per line with zone type.


@@UAHL@unbound-control.commands@list_local_data@@
    List the local data RRs in use.
    The resource records are printed.


@@UAHL@unbound-control.commands@insecure_add@@ *zone*
    Add a domain-insecure for the given zone, like the statement in
    unbound.conf.
    Adds to the running Unbound without affecting the cache
    contents (which may still be bogus, use flush_zone to remove it), does not
    affect the config file.


@@UAHL@unbound-control.commands@insecure_remove@@ *zone*
    Removes domain-insecure for the given zone.


@@UAHL@unbound-control.commands@forward_add@@ [``+it``] *zone addr ...*
    Add a new forward zone to running Unbound.
    With ``+i`` option also adds a domain-insecure for the zone (so it can
    resolve insecurely if you have a DNSSEC root trust anchor configured for
    other names).
    The addr can be IP4, IP6 or nameserver names, like forward-zone config in
    unbound.conf.
    The ``+t`` option sets it to use TLS upstream, like
    :ref:`forward-tls-upstream: yes<unbound.conf.forward.forward-tls-upstream>`.


@@UAHL@unbound-control.commands@forward_remove@@ [``+i``] *zone*
    Remove a forward zone from running Unbound.
    The ``+i`` also removes a domain-insecure for the zone.


@@UAHL@unbound-control.commands@stub_add@@ [``+ipt``] *zone addr ...*
    Add a new stub zone to running Unbound.
    With ``+i`` option also adds a domain-insecure for the zone.
    With ``+p`` the stub zone is set to prime, without it it is set to
    notprime.
    The addr can be IP4, IP6 or nameserver names, like the **stub-zone:**
    config in unbound.conf.
    The ``+t`` option sets it to use TLS upstream, like
    :ref:`stub-tls-upstream: yes<unbound.conf.stub.stub-tls-upstream>`.


@@UAHL@unbound-control.commands@stub_remove@@ [``+i``] *zone*
    Remove a stub zone from running Unbound.
    The ``+i`` also removes a domain-insecure for the zone.


@@UAHL@unbound-control.commands@forward@@ [*off* | *addr ...* ]
    Setup forwarding mode.
    Configures if the server should ask other upstream nameservers, should go
    to the internet root nameservers itself, or show the current config.
    You could pass the nameservers after a DHCP update.

    Without arguments the current list of addresses used to forward all queries
    to is printed.
    On startup this is from the forward-zone ``"."`` configuration.
    Afterwards it shows the status.
    It prints off when no forwarding is used.

    If off is passed, forwarding is disabled and the root nameservers are
    used.
    This can be used to avoid to avoid buggy or non-DNSSEC supporting
    nameservers returned from DHCP.
    But may not work in hotels or hotspots.

    If one or more IPv4 or IPv6 addresses are given, those are then used to
    forward queries to.
    The addresses must be separated with spaces.
    With ``'@port'`` the port number can be set explicitly (default port is 53
    (DNS)).

    By default the forwarder information from the config file for the root
    ``"."`` is used.
    The config file is not changed, so after a reload these changes are gone.
    Other forward zones from the config file are not affected by this command.


@@UAHL@unbound-control.commands@ratelimit_list@@ [``+a``]
    List the domains that are ratelimited.
    Printed one per line with current estimated qps and qps limit from config.
    With ``+a`` it prints all domains, not just the ratelimited domains, with
    their estimated qps.
    The ratelimited domains return an error for uncached (new) queries, but
    cached queries work as normal.


@@UAHL@unbound-control.commands@ip_ratelimit_list@@ [``+a``]
    List the ip addresses that are ratelimited.
    Printed one per line with current estimated qps and qps limit from config.
    With ``+a`` it prints all ips, not just the ratelimited ips, with their
    estimated qps.
    The ratelimited ips are dropped before checking the cache.


@@UAHL@unbound-control.commands@list_auth_zones@@
    List the auth zones that are configured.
    Printed one per line with a status, indicating if the zone is expired and
    current serial number.
    Configured RPZ zones are included.


@@UAHL@unbound-control.commands@auth_zone_reload@@ *zone*
    Reload the auth zone (or RPZ zone) from zonefile.
    The zonefile is read in overwriting the current contents of the zone in
    memory.
    This changes the auth zone contents itself, not the cache contents.
    Such cache contents exists if you set Unbound to validate with
    **for-upstream: yes** and that can be cleared with **flush_zone** *zone*.


@@UAHL@unbound-control.commands@auth_zone_transfer@@ *zone*
    Transfer the auth zone (or RPZ zone) from master.
    The auth zone probe sequence is started, where the masters are probed to
    see if they have an updated zone (with the SOA serial check).
    And then the zone is transferred for a newer zone version.


@@UAHL@unbound-control.commands@rpz_enable@@ *zone*
    Enable the RPZ zone if it had previously been disabled.


@@UAHL@unbound-control.commands@rpz_disable@@ *zone*
    Disable the RPZ zone.


@@UAHL@unbound-control.commands@view_list_local_zones@@ *view*
    *list_local_zones* for given view.


@@UAHL@unbound-control.commands@view_local_zone@@ *view name type*
    *local_zone* for given view.


@@UAHL@unbound-control.commands@view_local_zone_remove@@ *view name*
    *local_zone_remove* for given view.


@@UAHL@unbound-control.commands@view_list_local_data@@ *view*
    *list_local_data* for given view.


@@UAHL@unbound-control.commands@view_local_data@@ *view RR data...*
    *local_data* for given view.


@@UAHL@unbound-control.commands@view_local_data_remove@@ *view name*
    *local_data_remove* for given view.


@@UAHL@unbound-control.commands@view_local_datas_remove@@ *view*
    Remove a list of *local_data* for given view from stdin.
    Like *local_datas_remove*.


@@UAHL@unbound-control.commands@view_local_datas@@ *view*
    Add a list of *local_data* for given view from stdin.
    Like *local_datas*.


@@UAHL@unbound-control.commands@add_cookie_secret@@ *secret*
    Add or replace a cookie secret persistently.
    *secret* needs to be an 128 bit hex string.

    Cookie secrets can be either **active** or **staging**.
    **Active** cookie secrets are used to create DNS Cookies, but verification
    of a DNS Cookie succeeds with any of the **active** or **staging** cookie
    secrets.
    The state of the current cookie secrets can be printed with the
    :ref:`print_cookie_secrets<unbound-control.commands.print_cookie_secrets>`
    command.

    When there are no cookie secrets configured yet, the *secret* is added as
    **active**.
    If there is already an **active** cookie secret, the *secret* is added as
    **staging** or replacing an existing **staging** secret.

    To "roll" a cookie secret used in an anycast set.
    The new secret has to be added as **staging** secret to **all** nodes in
    the anycast set.
    When **all** nodes can verify DNS Cookies with the new secret, the new
    secret can be activated with the
    :ref:`activate_cookie_secret<unbound-control.commands.activate_cookie_secret>`
    command.
    After **all** nodes have the new secret **active** for at least one hour,
    the previous secret can be dropped with the
    :ref:`drop_cookie_secret<unbound-control.commands.drop_cookie_secret>`
    command.

    Persistence is accomplished by writing to a file which is configured with
    the
    :ref:`cookie-secret-file<unbound.conf.cookie-secret-file>`
    option in the server section of the config file.
    This is disabled by default, "".


@@UAHL@unbound-control.commands@drop_cookie_secret@@
    Drop the **staging** cookie secret.


@@UAHL@unbound-control.commands@activate_cookie_secret@@
    Make the current **staging** cookie secret **active**, and the current
    **active** cookie secret **staging**.


@@UAHL@unbound-control.commands@print_cookie_secrets@@
    Show the current configured cookie secrets with their status.

Exit Code
---------

The ``unbound-control`` program exits with status code 1 on error, 0 on
success.

Set Up
------

The setup requires a self-signed certificate and private keys for both the
server and client.
The script ``unbound-control-setup`` generates these in the default run
directory, or with ``-d`` in another directory.
If you change the access control permissions on the key files you can decide
who can use ``unbound-control``, by default owner and group but not all users.
Run the script under the same username as you have configured in
:file:`unbound.conf` or as root, so that the daemon is permitted to read the
files, for example with:

.. code-block:: bash

    sudo -u unbound unbound-control-setup

If you have not configured a username in :file:`unbound.conf`, the keys need
read permission for the user credentials under which the daemon is started.
The script preserves private keys present in the directory.
After running the script as root, turn on
:ref:`control-enable<unbound.conf.remote.control-enable>` in
:file:`unbound.conf`.

Statistic Counters
------------------

The :ref:`stats<unbound-control.commands.stats>` and
:ref:`stats_noreset<unbound-control.commands.stats_noreset>` commands show a
number of statistic counters:


@@UAHL@unbound-control.stats@threadX.num.queries@@
    number of queries received by thread


@@UAHL@unbound-control.stats@threadX.num.queries_ip_ratelimited@@
    number of queries rate limited by thread


@@UAHL@unbound-control.stats@threadX.num.queries_cookie_valid@@
    number of queries with a valid DNS Cookie by thread


@@UAHL@unbound-control.stats@threadX.num.queries_cookie_client@@
    number of queries with a client part only DNS Cookie by thread


@@UAHL@unbound-control.stats@threadX.num.queries_cookie_invalid@@
    number of queries with an invalid DNS Cookie by thread


@@UAHL@unbound-control.stats@threadX.num.queries_discard_timeout@@
    number of queries removed due to discard-timeout by thread


@@UAHL@unbound-control.stats@threadX.num.queries_wait_limit@@
    number of queries removed due to wait-limit by thread


@@UAHL@unbound-control.stats@threadX.num.cachehits@@
    number of queries that were successfully answered using a cache lookup


@@UAHL@unbound-control.stats@threadX.num.cachemiss@@
    number of queries that needed recursive processing


@@UAHL@unbound-control.stats@threadX.num.dnscrypt.crypted@@
    number of queries that were encrypted and successfully decapsulated by
    dnscrypt.


@@UAHL@unbound-control.stats@threadX.num.dnscrypt.cert@@
    number of queries that were requesting dnscrypt certificates.


@@UAHL@unbound-control.stats@threadX.num.dnscrypt.cleartext@@
    number of queries received on dnscrypt port that were cleartext and not a
    request for certificates.


@@UAHL@unbound-control.stats@threadX.num.dnscrypt.malformed@@
    number of request that were neither cleartext, not valid dnscrypt messages.


@@UAHL@unbound-control.stats@threadX.num.dns_error_reports@@
    number of DNS Error Reports generated by thread


@@UAHL@unbound-control.stats@threadX.num.prefetch@@
    number of cache prefetches performed.
    This number is included in cachehits, as the original query had the
    unprefetched answer from cache, and resulted in recursive processing,
    taking a slot in the requestlist.
    Not part of the recursivereplies (or the histogram thereof) or cachemiss,
    as a cache response was sent.


@@UAHL@unbound-control.stats@threadX.num.expired@@
    number of replies that served an expired cache entry.


@@UAHL@unbound-control.stats@threadX.num.queries_timed_out@@
    number of queries that are dropped because they waited in the UDP socket
    buffer for too long.


@@UAHL@unbound-control.stats@threadX.query.queue_time_us.max@@
    The maximum wait time for packets in the socket buffer, in microseconds.
    This is only reported when
    :ref:`sock-queue-timeout<unbound.conf.sock-queue-timeout>` is enabled.


@@UAHL@unbound-control.stats@threadX.num.recursivereplies@@
    The number of replies sent to queries that needed recursive processing.
    Could be smaller than threadX.num.cachemiss if due to timeouts no replies
    were sent for some queries.


@@UAHL@unbound-control.stats@threadX.requestlist.avg@@
    The average number of requests in the internal recursive processing request
    list on insert of a new incoming recursive processing query.


@@UAHL@unbound-control.stats@threadX.requestlist.max@@
    Maximum size attained by the internal recursive processing request list.


@@UAHL@unbound-control.stats@threadX.requestlist.overwritten@@
    Number of requests in the request list that were overwritten by newer
    entries.
    This happens if there is a flood of queries that recursive processing and
    the server has a hard time.


@@UAHL@unbound-control.stats@threadX.requestlist.exceeded@@
    Queries that were dropped because the request list was full.
    This happens if a flood of queries need recursive processing, and the
    server can not keep up.


@@UAHL@unbound-control.stats@threadX.requestlist.current.all@@
    Current size of the request list, includes internally generated queries
    (such as priming queries and glue lookups).


@@UAHL@unbound-control.stats@threadX.requestlist.current.user@@
    Current size of the request list, only the requests from client queries.


@@UAHL@unbound-control.stats@threadX.recursion.time.avg@@
    Average time it took to answer queries that needed recursive processing.
    Note that queries that were answered from the cache are not in this average.


@@UAHL@unbound-control.stats@threadX.recursion.time.median@@
    The median of the time it took to answer queries that needed recursive
    processing.
    The median means that 50% of the user queries were answered in less than
    this time.
    Because of big outliers (usually queries to non responsive servers), the
    average can be bigger than the median.
    This median has been calculated by interpolation from a histogram.


@@UAHL@unbound-control.stats@threadX.tcpusage@@
    The currently held tcp buffers for incoming connections.
    A spot value on the time of the request.
    This helps you spot if the incoming-num-tcp buffers are full.


@@UAHL@unbound-control.stats@total.num.queries@@
    summed over threads.


@@UAHL@unbound-control.stats@total.num.queries_ip_ratelimited@@
    summed over threads.


@@UAHL@unbound-control.stats@total.num.queries_cookie_valid@@
    summed over threads.


@@UAHL@unbound-control.stats@total.num.queries_cookie_client@@
    summed over threads.


@@UAHL@unbound-control.stats@total.num.queries_cookie_invalid@@
    summed over threads.


@@UAHL@unbound-control.stats@total.num.queries_discard_timeout@@
    summed over threads.


@@UAHL@unbound-control.stats@total.num.queries_wait_limit@@
    summed over threads.


@@UAHL@unbound-control.stats@total.num.cachehits@@
    summed over threads.


@@UAHL@unbound-control.stats@total.num.cachemiss@@
    summed over threads.


@@UAHL@unbound-control.stats@total.num.dnscrypt.crypted@@
    summed over threads.


@@UAHL@unbound-control.stats@total.num.dnscrypt.cert@@
    summed over threads.


@@UAHL@unbound-control.stats@total.num.dnscrypt.cleartext@@
    summed over threads.


@@UAHL@unbound-control.stats@total.num.dnscrypt.malformed@@
    summed over threads.


@@UAHL@unbound-control.stats@total.num.dns_error_reports@@
    summed over threads.


@@UAHL@unbound-control.stats@total.num.prefetch@@
    summed over threads.


@@UAHL@unbound-control.stats@total.num.expired@@
    summed over threads.


@@UAHL@unbound-control.stats@total.num.queries_timed_out@@
    summed over threads.


@@UAHL@unbound-control.stats@total.query.queue_time_us.max@@
    the maximum of the thread values.


@@UAHL@unbound-control.stats@total.num.recursivereplies@@
    summed over threads.


@@UAHL@unbound-control.stats@total.requestlist.avg@@
    averaged over threads.


@@UAHL@unbound-control.stats@total.requestlist.max@@
    the maximum of the thread requestlist.max values.


@@UAHL@unbound-control.stats@total.requestlist.overwritten@@
    summed over threads.


@@UAHL@unbound-control.stats@total.requestlist.exceeded@@
    summed over threads.


@@UAHL@unbound-control.stats@total.requestlist.current.all@@
    summed over threads.


@@UAHL@unbound-control.stats@total.recursion.time.median@@
    averaged over threads.


@@UAHL@unbound-control.stats@total.tcpusage@@
    summed over threads.


@@UAHL@unbound-control.stats@time.now@@
    current time in seconds since 1970.


@@UAHL@unbound-control.stats@time.up@@
    uptime since server boot in seconds.


@@UAHL@unbound-control.stats@time.elapsed@@
    time since last statistics printout, in seconds.

Extended Statistics
-------------------


@@UAHL@unbound-control.stats@mem.cache.rrset@@
    Memory in bytes in use by the RRset cache.


@@UAHL@unbound-control.stats@mem.cache.message@@
    Memory in bytes in use by the message cache.


@@UAHL@unbound-control.stats@mem.cache.dnscrypt_shared_secret@@
    Memory in bytes in use by the dnscrypt shared secrets cache.


@@UAHL@unbound-control.stats@mem.cache.dnscrypt_nonce@@
    Memory in bytes in use by the dnscrypt nonce cache.


@@UAHL@unbound-control.stats@mem.mod.iterator@@
    Memory in bytes in use by the iterator module.


@@UAHL@unbound-control.stats@mem.mod.validator@@
    Memory in bytes in use by the validator module.
    Includes the key cache and negative cache.


@@UAHL@unbound-control.stats@mem.streamwait@@
    Memory in bytes in used by the TCP and TLS stream wait buffers.
    These are answers waiting to be written back to the clients.


@@UAHL@unbound-control.stats@mem.http.query_buffer@@
    Memory in bytes used by the HTTP/2 query buffers.
    Containing (partial) DNS queries waiting for request stream completion.


@@UAHL@unbound-control.stats@mem.http.response_buffer@@
    Memory in bytes used by the HTTP/2 response buffers.
    Containing DNS responses waiting to be written back to the clients.


@@UAHL@unbound-control.stats@mem.quic@@
    Memory in bytes used by QUIC.
    Containing connection information, stream information, queries read and
    responses written back to the clients.

@@UAHL@unbound-control.stats@histogram@@.<sec>.<usec>.to.<sec>.<usec>
    Shows a histogram, summed over all threads.
    Every element counts the recursive queries whose reply time fit between the
    lower and upper bound.
    Times larger or equal to the lowerbound, and smaller than the upper bound.
    There are 40 buckets, with bucket sizes doubling.


@@UAHL@unbound-control.stats@num.query.type.A@@
    The total number of queries over all threads with query type A.
    Printed for the other query types as well, but only for the types for which
    queries were received, thus =0 entries are omitted for brevity.


@@UAHL@unbound-control.stats@num.query.type.other@@
    Number of queries with query types 256-65535.


@@UAHL@unbound-control.stats@num.query.class.IN@@
    The total number of queries over all threads with query class IN
    (internet).
    Also printed for other classes (such as CH (CHAOS) sometimes used for
    debugging), or NONE, ANY, used by dynamic update.
    num.query.class.other is printed for classes 256-65535.


@@UAHL@unbound-control.stats@num.query.opcode.QUERY@@
    The total number of queries over all threads with query opcode QUERY.
    Also printed for other opcodes, UPDATE, ...


@@UAHL@unbound-control.stats@num.query.tcp@@
    Number of queries that were made using TCP towards the Unbound server.


@@UAHL@unbound-control.stats@num.query.tcpout@@
    Number of queries that the Unbound server made using TCP outgoing towards
    other servers.


@@UAHL@unbound-control.stats@num.query.udpout@@
    Number of queries that the Unbound server made using UDP outgoing towards
    other servers.


@@UAHL@unbound-control.stats@num.query.tls@@
    Number of queries that were made using TLS towards the Unbound server.
    These are also counted in num.query.tcp, because TLS uses TCP.


@@UAHL@unbound-control.stats@num.query.tls.resume@@
    Number of TLS session resumptions, these are queries over TLS towards the
    Unbound server where the client negotiated a TLS session resumption key.


@@UAHL@unbound-control.stats@num.query.https@@
    Number of queries that were made using HTTPS towards the Unbound server.
    These are also counted in num.query.tcp and num.query.tls, because HTTPS
    uses TLS and TCP.


@@UAHL@unbound-control.stats@num.query.quic@@
    Number of queries that were made using QUIC towards the Unbound server.
    These are also counted in num.query.tls, because TLS is used for these
    queries.


@@UAHL@unbound-control.stats@num.query.ipv6@@
    Number of queries that were made using IPv6 towards the Unbound server.


@@UAHL@unbound-control.stats@num.query.flags.RD@@
    The number of queries that had the RD flag set in the header.
    Also printed for flags QR, AA, TC, RA, Z, AD, CD.
    Note that queries with flags QR, AA or TC may have been rejected because of
    that.


@@UAHL@unbound-control.stats@num.query.edns.present@@
    number of queries that had an EDNS OPT record present.


@@UAHL@unbound-control.stats@num.query.edns.DO@@
    number of queries that had an EDNS OPT record with the DO (DNSSEC OK) bit
    set.
    These queries are also included in the num.query.edns.present number.


@@UAHL@unbound-control.stats@num.query.ratelimited@@
    The number of queries that are turned away from being send to nameserver
    due to ratelimiting.


@@UAHL@unbound-control.stats@num.query.dnscrypt.shared_secret.cachemiss@@
    The number of dnscrypt queries that did not find a shared secret in the
    cache.
    This can be use to compute the shared secret hitrate.


@@UAHL@unbound-control.stats@num.query.dnscrypt.replay@@
    The number of dnscrypt queries that found a nonce hit in the nonce cache
    and hence are considered a query replay.


@@UAHL@unbound-control.stats@num.answer.rcode.NXDOMAIN@@
    The number of answers to queries, from cache or from recursion, that had
    the return code NXDOMAIN.
    Also printed for the other return codes.


@@UAHL@unbound-control.stats@num.answer.rcode.nodata@@
    The number of answers to queries that had the pseudo return code nodata.
    This means the actual return code was NOERROR, but additionally, no data
    was carried in the answer (making what is called a NOERROR/NODATA answer).
    These queries are also included in the num.answer.rcode.NOERROR number.
    Common for AAAA lookups when an A record exists, and no AAAA.


@@UAHL@unbound-control.stats@num.answer.secure@@
    Number of answers that were secure.
    The answer validated correctly.
    The AD bit might have been set in some of these answers, where the client
    signalled (with DO or AD bit in the query) that they were ready to accept
    the AD bit in the answer.


@@UAHL@unbound-control.stats@num.answer.bogus@@
    Number of answers that were bogus.
    These answers resulted in SERVFAIL to the client because the answer failed
    validation.


@@UAHL@unbound-control.stats@num.rrset.bogus@@
    The number of rrsets marked bogus by the validator.
    Increased for every RRset inspection that fails.


@@UAHL@unbound-control.stats@num.valops@@
    The number of validation operations performed by the validator.
    Increased for every RRSIG verification operation regardless of the
    validation result.
    The RRSIG and key combination needs to first pass some sanity checks before
    Unbound even performs the verification, e.g., length/protocol checks.


@@UAHL@unbound-control.stats@unwanted.queries@@
    Number of queries that were refused or dropped because they failed the
    access control settings.


@@UAHL@unbound-control.stats@unwanted.replies@@
    Replies that were unwanted or unsolicited.
    Could have been random traffic, delayed duplicates, very late answers, or
    could be spoofing attempts.
    Some low level of late answers and delayed duplicates are to be expected
    with the UDP protocol.
    Very high values could indicate a threat (spoofing).


@@UAHL@unbound-control.stats@msg.cache.count@@
    The number of items (DNS replies) in the message cache.


@@UAHL@unbound-control.stats@rrset.cache.count@@
    The number of RRsets in the rrset cache.
    This includes rrsets used by the messages in the message cache, but also
    delegation information.


@@UAHL@unbound-control.stats@infra.cache.count@@
    The number of items in the infra cache.
    These are IP addresses with their timing and protocol support information.


@@UAHL@unbound-control.stats@key.cache.count@@
    The number of items in the key cache.
    These are DNSSEC keys, one item per delegation point, and their validation
    status.


@@UAHL@unbound-control.stats@msg.cache.max_collisions@@
    The maximum number of hash table collisions in the msg cache.
    This is the number of hashes that are identical when a new element is
    inserted in the hash table.
    If the value is very large, like hundreds, something is wrong with the
    performance of the hash table, hash values are incorrect or malicious.


@@UAHL@unbound-control.stats@rrset.cache.max_collisions@@
    The maximum number of hash table collisions in the rrset cache.
    This is the number of hashes that are identical when a new element is
    inserted in the hash table.
    If the value is very large, like hundreds, something is wrong with the
    performance of the hash table, hash values are incorrect or malicious.


@@UAHL@unbound-control.stats@dnscrypt_shared_secret.cache.count@@
    The number of items in the shared secret cache.
    These are precomputed shared secrets for a given client public key/server
    secret key pair.
    Shared secrets are CPU intensive and this cache allows Unbound to avoid
    recomputing the shared secret when multiple dnscrypt queries are sent from
    the same client.


@@UAHL@unbound-control.stats@dnscrypt_nonce.cache.count@@
    The number of items in the client nonce cache.
    This cache is used to prevent dnscrypt queries replay.
    The client nonce must be unique for each client public key/server secret
    key pair.
    This cache should be able to host QPS * `replay window` interval keys to
    prevent replay of a query during `replay window` seconds.


@@UAHL@unbound-control.stats@num.query.authzone.up@@
    The number of queries answered from auth-zone data, upstream queries.
    These queries would otherwise have been sent (with fallback enabled) to the
    internet, but are now answered from the auth zone.


@@UAHL@unbound-control.stats@num.query.authzone.down@@
    The number of queries for downstream answered from auth-zone data.
    These queries are from downstream clients, and have had an answer from the
    data in the auth zone.


@@UAHL@unbound-control.stats@num.query.aggressive.NOERROR@@
    The number of queries answered using cached NSEC records with NODATA RCODE.
    These queries would otherwise have been sent to the internet, but are now
    answered using cached data.


@@UAHL@unbound-control.stats@num.query.aggressive.NXDOMAIN@@
    The number of queries answered using cached NSEC records with NXDOMAIN
    RCODE.
    These queries would otherwise have been sent to the internet, but are now
    answered using cached data.


@@UAHL@unbound-control.stats@num.query.subnet@@
    Number of queries that got an answer that contained EDNS client subnet
    data.


@@UAHL@unbound-control.stats@num.query.subnet_cache@@
    Number of queries answered from the edns client subnet cache.
    These are counted as cachemiss by the main counters, but hit the client
    subnet specific cache after getting processed by the edns client subnet
    module.


@@UAHL@unbound-control.stats@num.query.cachedb@@
    Number of queries answered from the external cache of cachedb.
    These are counted as cachemiss by the main counters, but hit the cachedb
    external cache after getting processed by the cachedb module.

@@UAHL@unbound-control.stats@num.rpz.action@@.<rpz_action>
    Number of queries answered using configured RPZ policy, per RPZ action
    type.
    Possible actions are: nxdomain, nodata, passthru, drop, tcp-only,
    local-data, disabled, and cname-override.

Files
-----

@ub_conf_file@
    Unbound configuration file.

@UNBOUND_RUN_DIR@
    directory with private keys (:file:`unbound_server.key` and
    :file:`unbound_control.key`) and self-signed certificates
    (:file:`unbound_server.pem` and :file:`unbound_control.pem`).

See Also
--------

:doc:`unbound.conf(5)</manpages/unbound.conf>`,
:doc:`unbound(8)</manpages/unbound>`.
