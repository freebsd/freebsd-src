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

libunbound(3)
=============

Synopsis
--------

.. only:: html

    .. code-block:: c

        #include <unbound.h>

        struct ub_ctx * ub_ctx_create(void);

        void ub_ctx_delete(struct ub_ctx* ctx);

        int ub_ctx_set_option(struct ub_ctx* ctx, char* opt, char* val);

        int ub_ctx_get_option(struct ub_ctx* ctx, char* opt, char** val);

        int ub_ctx_config(struct ub_ctx* ctx, char* fname);

        int ub_ctx_set_fwd(struct ub_ctx* ctx, char* addr);

        int ub_ctx_set_stub(struct ub_ctx* ctx, char* zone, char* addr,
                            int isprime);

        int ub_ctx_set_tls(struct ub_ctx* ctx, int tls);

        int ub_ctx_resolvconf(struct ub_ctx* ctx, char* fname);

        int ub_ctx_hosts(struct ub_ctx* ctx, char* fname);

        int ub_ctx_add_ta(struct ub_ctx* ctx, char* ta);

        int ub_ctx_add_ta_autr(struct ub_ctx* ctx, char* fname);

        int ub_ctx_add_ta_file(struct ub_ctx* ctx, char* fname);

        int ub_ctx_trustedkeys(struct ub_ctx* ctx, char* fname);

        int ub_ctx_debugout(struct ub_ctx* ctx, FILE* out);

        int ub_ctx_debuglevel(struct ub_ctx* ctx, int d);

        int ub_ctx_async(struct ub_ctx* ctx, int dothread);

        int ub_poll(struct ub_ctx* ctx);

        int ub_wait(struct ub_ctx* ctx);

        int ub_fd(struct ub_ctx* ctx);

        int ub_process(struct ub_ctx* ctx);

        int ub_resolve(struct ub_ctx* ctx, char* name, int rrtype,
                       int rrclass, struct ub_result** result);

        int ub_resolve_async(struct ub_ctx* ctx, char* name, int rrtype,
                             int rrclass, void* mydata, ub_callback_type callback,
                             int* async_id);

        int ub_cancel(struct ub_ctx* ctx, int async_id);

        void ub_resolve_free(struct ub_result* result);

        const char * ub_strerror(int err);

        int ub_ctx_print_local_zones(struct ub_ctx* ctx);

        int ub_ctx_zone_add(struct ub_ctx* ctx, char* zone_name, char* zone_type);

        int ub_ctx_zone_remove(struct ub_ctx* ctx, char* zone_name);

        int ub_ctx_data_add(struct ub_ctx* ctx, char* data);

        int ub_ctx_data_remove(struct ub_ctx* ctx, char* data);

.. only:: man

    **#include <unbound.h>**

    struct ub_ctx \* **ub_ctx_create**\ (void);

    void **ub_ctx_delete**\ (struct ub_ctx\* ctx);

    int **ub_ctx_set_option**\ (struct ub_ctx\* ctx, char\* opt, char\* val);

    int **ub_ctx_get_option**\ (struct ub_ctx\* ctx, char\* opt, char\*\* val);

    int **ub_ctx_config**\ (struct ub_ctx\* ctx, char* fname);

    int **ub_ctx_set_fwd**\ (struct ub_ctx\* ctx, char\* addr);

    int **ub_ctx_set_stub**\ (struct ub_ctx\* ctx, char\* zone, char\* addr,
        int isprime);

    int **ub_ctx_set_tls**\ (struct ub_ctx\* ctx, int tls);

    int **ub_ctx_resolvconf**\ (struct ub_ctx\* ctx, char\* fname);

    int **ub_ctx_hosts**\ (struct ub_ctx\* ctx, char\* fname);

    int **ub_ctx_add_ta**\ (struct ub_ctx\* ctx, char\* ta);

    int **ub_ctx_add_ta_autr**\ (struct ub_ctx\* ctx, char\* fname);

    int **ub_ctx_add_ta_file**\ (struct ub_ctx\* ctx, char\* fname);

    int **ub_ctx_trustedkeys**\ (struct ub_ctx\* ctx, char\* fname);

    int **ub_ctx_debugout**\ (struct ub_ctx\* ctx, FILE\* out);

    int **ub_ctx_debuglevel**\ (struct ub_ctx\* ctx, int d);

    int **ub_ctx_async**\ (struct ub_ctx\* ctx, int dothread);

    int **ub_poll**\ (struct ub_ctx\* ctx);

    int **ub_wait**\ (struct ub_ctx\* ctx);

    int **ub_fd**\ (struct ub_ctx\* ctx);

    int **ub_process**\ (struct ub_ctx\* ctx);

    int **ub_resolve**\ (struct ub_ctx\* ctx, char\* name,
        int rrtype, int rrclass, struct ub_result\*\* result);

    int **ub_resolve_async**\ (struct ub_ctx\* ctx, char\* name,
        int rrtype, int rrclass, void\* mydata,
        ub_callback_type\* callback, int\* async_id);

    int **ub_cancel**\ (struct ub_ctx\* ctx, int async_id);

    void **ub_resolve_free**\ (struct ub_result\* result);

    const char \* **ub_strerror**\ (int err);

    int **ub_ctx_print_local_zones**\ (struct ub_ctx\* ctx);

    int **ub_ctx_zone_add**\ (struct ub_ctx\* ctx, char\* zone_name, char\* zone_type);

    int **ub_ctx_zone_remove**\ (struct ub_ctx\* ctx, char\* zone_name);

    int **ub_ctx_data_add**\ (struct ub_ctx\* ctx, char\* data);

    int **ub_ctx_data_remove**\ (struct ub_ctx\* ctx, char\* data);

Description
-----------

Unbound is an implementation of a DNS resolver, that does caching and DNSSEC
validation.
This is the library API, for using the ``-lunbound`` library.
The server daemon is described in :doc:`unbound(8)</manpages/unbound>`.
The library works independent from a running unbound server, and can be used to
convert hostnames to ip addresses, and back, and obtain other information from
the DNS.
The library performs public-key validation of results with DNSSEC.

The library uses a variable of type *struct ub_ctx* to keep context between
calls.
The user must maintain it, creating it with **ub_ctx_create** and deleting it
with **ub_ctx_delete**.
It can be created and deleted at any time.
Creating it anew removes any previous configuration (such as trusted keys) and
clears any cached results.

The functions are thread-safe, and a context can be used in a threaded (as well
as in a non-threaded) environment.
Also resolution (and validation) can be performed blocking and non-blocking
(also called asynchronous).
The async method returns from the call immediately, so that processing can go
on, while the results become available later.

The functions are discussed in turn below.

Functions
---------

.. glossary::

    ub_ctx_create
        Create a new context, initialised with defaults.
        The information from :file:`/etc/resolv.conf` and :file:`/etc/hosts` is
        not utilised by default.
        Use **ub_ctx_resolvconf** and **ub_ctx_hosts** to read them.
        Before you call this, use the openssl functions
        **CRYPTO_set_id_callback** and **CRYPTO_set_locking_callback** to set
        up asynchronous operation if you use lib openssl (the application calls
        these functions once for initialisation).
        Openssl 1.0.0 or later uses the **CRYPTO_THREADID_set_callback**
        function.

    ub_ctx_delete
        Delete validation context and free associated resources.
        Outstanding async queries are killed and callbacks are not called for
        them.

    ub_ctx_set_option
        A power-user interface that lets you specify one of the options from
        the config file format, see :doc:`unbound.conf(5)</manpages/unbound.conf>`.
        Not all options are relevant.
        For some specific options, such as adding trust anchors, special
        routines exist.
        Pass the option name with the trailing ``':'``.

    ub_ctx_get_option
        A power-user interface that gets an option value.
        Some options cannot be gotten, and others return a newline separated
        list.
        Pass the option name without trailing ``':'``.
        The returned value must be free(2)d by the caller.

    ub_ctx_config
        A power-user interface that lets you specify an unbound config file,
        see :doc:`unbound.conf(5)</manpages/unbound.conf>`, which is read for
        configuration.
        Not all options are relevant.
        For some specific options, such as adding trust anchors, special
        routines exist.
        This function is thread-safe only if a single instance of **ub_ctx**\*
        exists in the application.
        If several instances exist the application has to ensure that
        **ub_ctx_config** is not called in parallel by the different instances.

    ub_ctx_set_fwd
        Set machine to forward DNS queries to, the caching resolver to use.
        IP4 or IP6 address.
        Forwards all DNS requests to that machine, which is expected to run a
        recursive resolver.
        If the proxy is not DNSSEC capable, validation may fail.
        Can be called several times, in that case the addresses are used as
        backup servers.
        At this time it is only possible to set configuration before the first
        resolve is done.

    ub_ctx_set_stub
        Set a stub zone, authoritative dns servers to use for a particular
        zone.
        IP4 or IP6 address.
        If the address is NULL the stub entry is removed.
        Set isprime true if you configure root hints with it.
        Otherwise similar to the stub zone item from unbound's config file.
        Can be called several times, for different zones, or to add multiple
        addresses for a particular zone.
        At this time it is only possible to set configuration before the first
        resolve is done.

    ub_ctx_set_tls
        Enable DNS over TLS (DoT) for machines set with **ub_ctx_set_fwd**.
        At this time it is only possible to set configuration before the first
        resolve is done.

    ub_ctx_resolvconf
        By default the root servers are queried and full resolver mode is used,
        but you can use this call to read the list of nameservers to use from
        the filename given.
        Usually :file:`"/etc/resolv.conf"`.
        Uses those nameservers as caching proxies.
        If they do not support DNSSEC, validation may fail.
        Only nameservers are picked up, the searchdomain, ndots and other
        settings from *resolv.conf(5)* are ignored.
        If fname NULL is passed, :file:`"/etc/resolv.conf"` is used (if on
        Windows, the system-wide configured nameserver is picked instead).
        At this time it is only possible to set configuration before the first
        resolve is done.

    ub_ctx_hosts
        Read list of hosts from the filename given.
        Usually :file:`"/etc/hosts"`.
        When queried for, these addresses are not marked DNSSEC secure.
        If fname NULL is passed, :file:`"/etc/hosts"` is used (if on Windows,
        :file:`etc/hosts` from WINDIR is picked instead).
        At this time it is only possible to set configuration before the first
        resolve is done.

    ub_ctx_add_ta
        Add a trust anchor to the given context.
        At this time it is only possible to add trusted keys before the first
        resolve is done.
        The format is a string, similar to the zone-file format,
        **[domainname]** **[type]** **[rdata contents]**.
        Both DS and DNSKEY records are accepted.

    ub_ctx_add_ta_autr
        Add filename with automatically tracked trust anchor to the given
        context.
        Pass name of a file with the managed trust anchor.
        You can create this file with
        :doc:`unbound-anchor(8)</manpages/unbound-anchor>` for the root anchor.
        You can also create it with an initial file with one line with a DNSKEY
        or DS record.
        If the file is writable, it is updated when the trust anchor changes.
        At this time it is only possible to add trusted keys before the first
        resolve is done.

    ub_ctx_add_ta_file
        Add trust anchors to the given context.
        Pass name of a file with DS and DNSKEY records in zone file format.
        At this time it is only possible to add trusted keys before the first
        resolve is done.

    ub_ctx_trustedkeys
        Add trust anchors to the given context.
        Pass the name of a bind-style config file with ``trusted-keys{}``.
        At this time it is only possible to add trusted keys before the first
        resolve is done.

    ub_ctx_debugout
        Set debug and error log output to the given stream.
        Pass NULL to disable output.
        Default is stderr.
        File-names or using syslog can be enabled using config options, this
        routine is for using your own stream.

    ub_ctx_debuglevel
        Set debug verbosity for the context.
        Output is directed to stderr.
        Higher debug level gives more output.

    ub_ctx_async
        Set a context behaviour for asynchronous action.
        if set to true, enables threading and a call to **ub_resolve_async**
        creates a thread to handle work in the background.
        If false, a process is forked to handle work in the background.
        Changes to this setting after **ub_resolve_async** calls have been made
        have no effect (delete and re-create the context to change).

    ub_poll
        Poll a context to see if it has any new results.
        Do not poll in a loop, instead extract the **fd** below to poll for
        readiness, and then check, or wait using the wait routine.
        Returns 0 if nothing to read, or nonzero if a result is available.
        If nonzero, call **ub_process** to do callbacks.

    ub_wait
        Wait for a context to finish with results.
        Calls **ub_process** after the wait for you.
        After the wait, there are no more outstanding asynchronous queries.

    ub_fd 
        Get file descriptor.
        Wait for it to become readable, at this point answers are returned from
        the asynchronous validating resolver.
        Then call the **ub_process** to continue processing.

    ub_process
        Call this routine to continue processing results from the validating
        resolver (when the **fd** becomes readable).
        Will perform necessary callbacks.

    ub_resolve
        Perform resolution and validation of the target name.
        The name is a domain name in a zero terminated text string.
        The rrtype and rrclass are DNS type and class codes.
        The result structure is newly allocated with the resulting data.

    ub_resolve_async
        Perform asynchronous resolution and validation of the target name.
        Arguments mean the same as for **ub_resolve** except no data is
        returned immediately, instead a callback is called later.
        The callback receives a copy of the mydata pointer, that you can use to
        pass information to the callback.
        The callback type is a function pointer to a function declared as:

        .. code-block:: c

            void my_callback_function(void* my_arg, int err,
                            struct ub_result* result);

        The **async_id** is returned so you can (at your option) decide to
        track it and cancel the request if needed.
        If you pass a NULL pointer the **async_id** is not returned.

    ub_cancel
        Cancel an async query in progress.
        This may return an error if the query does not exist, or the query is
        already being delivered, in that case you may still get a callback for
        the query.

    ub_resolve_free
        Free struct **ub_result** contents after use.

    ub_strerror
        Convert error value from one of the unbound library functions to a
        human readable string.

    ub_ctx_print_local_zones
        Debug printout the local authority information to debug output.

    ub_ctx_zone_add
        Add new zone to local authority info, like local-zone
        :doc:`unbound.conf(5)</manpages/unbound.conf>` statement.

    ub_ctx_zone_remove
        Delete zone from local authority info.

    ub_ctx_data_add
        Add resource record data to local authority info, like local-data
        :doc:`unbound.conf(5)</manpages/unbound.conf>` statement.

    ub_ctx_data_remove
        Delete local authority data from the name given.

Result Data structure
---------------------

The result of the DNS resolution and validation is returned as *struct
ub_result*.
The result structure contains the following entries:

.. code-block:: c

    struct ub_result {
         char* qname;         /* text string, original question */
         int qtype;           /* type code asked for */
         int qclass;          /* class code asked for */
         char** data;         /* array of rdata items, NULL terminated*/
         int* len;            /* array with lengths of rdata items */
         char* canonname;     /* canonical name of result */
         int rcode;           /* additional error code in case of no data */
         void* answer_packet; /* full network format answer packet */
         int answer_len;      /* length of packet in octets */
         int havedata;        /* true if there is data */
         int nxdomain;        /* true if nodata because name does not exist */
         int secure;          /* true if result is secure */
         int bogus;           /* true if a security failure happened */
         char* why_bogus;     /* string with error if bogus */
         int was_ratelimited; /* true if the query was ratelimited (SERVFAIL) by unbound */
         int ttl;             /* number of seconds the result is valid */
    };

If both secure and bogus are false, security was not enabled for the domain of
the query.
Else, they are not both true, one of them is true.

Return Values
-------------

Many routines return an error code.
The value 0 (zero) denotes no error happened.
Other values can be passed to **ub_strerror** to obtain a readable error
string.
**ub_strerror** returns a zero terminated string.
**ub_ctx_create** returns NULL on an error (a malloc failure).
**ub_poll** returns true if some information may be available, false otherwise.
**ub_fd** returns a file descriptor or -1 on error.
**ub_ctx_config** and **ub_ctx_resolvconf** attempt to leave errno informative
on a function return with file read failure.

See Also
--------

:doc:`unbound.conf(5)</manpages/unbound.conf>`, :doc:`unbound(8)</manpages/unbound>`.
