# -*- coding: utf-8 -*-
'''
 inplace_callbacks.py: python module showcasing inplace callback function
                       registration and functionality.

 Copyright (c) 2016, NLnet Labs.

 This software is open source.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions
 are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

    * Neither the name of the organization nor the names of its
      contributors may be used to endorse or promote products derived from this
      software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
'''
#Try:
# - dig @localhost nlnetlabs.nl +ednsopt=65002:
#       This query *could* be answered from cache. If so, unbound will reply
#       with the same EDNS option 65002, but with hexdata 'deadbeef' as data.
#
# - dig @localhost bogus.nlnetlabs.nl txt:
#       This query returns SERVFAIL as the txt record of bogus.nlnetlabs.nl is
#       intentionally bogus. The reply will contain an empty EDNS option
#       with option code 65003.
#       Unbound will also log the source address(es) of the client(s) that made
#       the request.
#       (unbound needs to be validating for this example to work)

# Useful functions:
#   register_inplace_cb_reply(inplace_reply_callback, env, id):
#       Register the reply_callback function as an inplace callback function
#       when answering with a resolved query.
#       Return True on success, False on failure.
#
#   register_inplace_cb_reply_cache(inplace_reply_cache_callback, env, id):
#       Register the reply_cache_callback function as an inplace callback
#       function when answering from cache.
#       Return True on success, False on failure.
#
#   register_inplace_cb_reply_local(inplace_reply_local_callback, env, id):
#       Register the reply_local_callback function as an inplace callback
#       function when answering from local data or chaos reply.
#       Return True on success, False on failure.
#
#   register_inplace_cb_reply_servfail(inplace_reply_servfail_callback, env, id):
#       Register the reply_servfail_callback function as an inplace callback
#       function when answering with servfail.
#       Return True on success, False on failure.
#
# Examples on how to use the functions are given in this file.


def inplace_reply_callback(qinfo, qstate, rep, rcode, edns, opt_list_out,
                           region, **kwargs):
    """
    Function that will be registered as an inplace callback function.
    It will be called when answering with a resolved query.

    :param qinfo: query_info struct;
    :param qstate: module qstate. It contains the available opt_lists; It
                   SHOULD NOT be altered;
    :param rep: reply_info struct;
    :param rcode: return code for the query;
    :param edns: edns_data to be sent to the client side. It SHOULD NOT be
                 altered;
    :param opt_list_out: the list with the EDNS options that will be sent as a
                         reply. It can be populated with EDNS options;
    :param region: region to allocate temporary data. Needs to be used when we
                   want to append a new option to opt_list_out.
    :param **kwargs: Dictionary that may contain parameters added in a future
                     release. Current parameters:
        ``repinfo``: Reply information for a communication point (comm_reply).
                     It is None when the callback happens in the mesh
                     states(modules).

    :return: True on success, False on failure.

    """
    log_info("python: called back while replying.")
    return True


def inplace_cache_callback(qinfo, qstate, rep, rcode, edns, opt_list_out,
                           region, **kwargs):
    """
    Function that will be registered as an inplace callback function.
    It will be called when answering from the cache.

    :param qinfo: query_info struct;
    :param qstate: module qstate. None;
    :param rep: reply_info struct;
    :param rcode: return code for the query;
    :param edns: edns_data sent from the client side. The list with the EDNS
                 options is accessible through edns.opt_list. It SHOULD NOT be
                 altered;
    :param opt_list_out: the list with the EDNS options that will be sent as a
                         reply. It can be populated with EDNS options;
    :param region: region to allocate temporary data. Needs to be used when we
                   want to append a new option to opt_list_out.
    :param **kwargs: Dictionary that may contain parameters added in a future
                     release. Current parameters:
        ``repinfo``: Reply information for a communication point (comm_reply).
                     It is None when the callback happens in the mesh
                     states(modules).

    :return: True on success, False on failure.

    For demonstration purposes we want to see if EDNS option 65002 is present
    and reply with a new value.

    """
    log_info("python: called back while answering from cache.")
    # Inspect the incoming EDNS options.
    if not edns_opt_list_is_empty(edns.opt_list):
        log_info("python: available EDNS options:")
        for o in edns.opt_list_iter:
            log_info("python:    Code: {}, Data: '{}'".format(o.code,
                "".join('{:02x}'.format(x) for x in o.data)))
            if o.code == 65002:
                log_info("python: *found option code 65002*")

                # add to opt_list
                # Data MUST be represented in a bytearray.
                b = bytearray.fromhex("deadbeef")
                if edns_opt_list_append(opt_list_out, o.code, b, region):
                    log_info("python: *added new option code 65002*")
                else:
                    log_info("python: *failed to add new option code 65002*")
                    return False
                break

    return True


def inplace_local_callback(qinfo, qstate, rep, rcode, edns, opt_list_out,
                           region, **kwargs):
    """
    Function that will be registered as an inplace callback function.
    It will be called when answering from local data.

    :param qinfo: query_info struct;
    :param qstate: module qstate. None;
    :param rep: reply_info struct;
    :param rcode: return code for the query;
    :param edns: edns_data sent from the client side. The list with the
                 EDNS options is accessible through edns.opt_list. It
                 SHOULD NOT be altered;
    :param opt_list_out: the list with the EDNS options that will be sent as a
                         reply. It can be populated with EDNS options;
    :param region: region to allocate temporary data. Needs to be used when we
                   want to append a new option to opt_list_out.
    :param **kwargs: Dictionary that may contain parameters added in a future
                     release. Current parameters:
        ``repinfo``: Reply information for a communication point (comm_reply).
                     It is None when the callback happens in the mesh
                     states(modules).

    :return: True on success, False on failure.

    """
    log_info("python: called back while replying with local data or chaos"
             " reply.")
    return True


def inplace_servfail_callback(qinfo, qstate, rep, rcode, edns, opt_list_out,
                              region, **kwargs):
    """
    Function that will be registered as an inplace callback function.
    It will be called when answering with SERVFAIL.

    :param qinfo: query_info struct;
    :param qstate: module qstate. If not None the relevant opt_lists are
                   available here;
    :param rep: reply_info struct. None;
    :param rcode: return code for the query. LDNS_RCODE_SERVFAIL;
    :param edns: edns_data to be sent to the client side. If qstate is None
                 edns.opt_list contains the EDNS options sent from the client
                 side. It SHOULD NOT be altered;
    :param opt_list_out: the list with the EDNS options that will be sent as a
                         reply. It can be populated with EDNS options;
    :param region: region to allocate temporary data. Needs to be used when we
                   want to append a new option to opt_list_out.
    :param **kwargs: Dictionary that may contain parameters added in a future
                     release. Current parameters:
        ``repinfo``: Reply information for a communication point (comm_reply).
                     It is None when the callback happens in the mesh
                     states(modules).

    :return: True on success, False on failure.

    For demonstration purposes we want to reply with an empty EDNS code '65003'
    and log the IP address(es) of the client(s).

    """
    log_info("python: called back while servfail.")
    # Append the example ENDS option
    b = bytearray.fromhex("")
    edns_opt_list_append(opt_list_out, 65003, b, region)

    # Log the client(s) IP address(es)
    comm_reply = kwargs['repinfo']
    if comm_reply:
        # If it is not None this callback was called before the query reached
        # the mesh states(modules). There is only one client associated with
        # this query.
        addr = comm_reply.addr
        port = comm_reply.port
        addr_family = comm_reply.family
        log_info("python: Client IP: {}({}), port: {}"
                 "".format(addr, addr_family, port))
    else:
        # If it is not None this callback was called while the query is in the
        # mesh states(modules). In this case they may be multiple clients
        # waiting for this query.
        # The following code is the same as with the resip.py example.
        rl = qstate.mesh_info.reply_list
        while (rl):
            if rl.query_reply:
                q = rl.query_reply
                log_info("python: Client IP: {}({}), port: {}"
                         "".format(q.addr, q.family, q.port))
            rl = rl.next


    return True


def init_standard(id, env):
    """
    New version of the init function.

    The function's signature is the same as the C counterpart and allows for
    extra functionality during init.

    ..note:: This function is preferred by unbound over the old init function.
    ..note:: The previously accessible configuration options can now be found in
             env.cgf.

    """
    log_info("python: inited script {}".format(env.cfg.python_script))

    # Register the inplace_reply_callback function as an inplace callback
    # function when answering a resolved query.
    if not register_inplace_cb_reply(inplace_reply_callback, env, id):
        return False

    # Register the inplace_cache_callback function as an inplace callback
    # function when answering from cache.
    if not register_inplace_cb_reply_cache(inplace_cache_callback, env, id):
        return False

    # Register the inplace_local_callback function as an inplace callback
    # function when answering from local data.
    if not register_inplace_cb_reply_local(inplace_local_callback, env, id):
        return False

    # Register the inplace_servfail_callback function as an inplace callback
    # function when answering with SERVFAIL.
    if not register_inplace_cb_reply_servfail(inplace_servfail_callback, env, id):
        return False

    return True


def init(id, cfg):
    """
    Previous version of the init function.

    ..note:: This function is still supported for backwards compatibility when
             the init_standard function is missing. When init_standard is
             present this function SHOULD be omitted to avoid confusion to the
             reader.

    """
    return True


def deinit(id): return True


def inform_super(id, qstate, superqstate, qdata): return True


def operate(id, event, qstate, qdata):
    if (event == MODULE_EVENT_NEW) or (event == MODULE_EVENT_PASS):
        qstate.ext_state[id] = MODULE_WAIT_MODULE 
        return True

    elif event == MODULE_EVENT_MODDONE:
        qstate.ext_state[id] = MODULE_FINISHED
        return True

    log_err("pythonmod: Unknown event")
    qstate.ext_state[id] = MODULE_ERROR
    return True
