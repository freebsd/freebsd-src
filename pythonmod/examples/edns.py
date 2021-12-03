# -*- coding: utf-8 -*-
'''
 edns.py: python module showcasing EDNS option functionality.

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
# - dig @localhost nlnetlabs.nl +ednsopt=65001:c001
#       This query will always reach the modules stage as EDNS option 65001 is
#       registered to bypass the cache response stage. It will also be handled
#       as a unique query because of the no_aggregation flag. This means that
#       it will not be aggregated with other queries for the same qinfo.
#       For demonstration purposes when option 65001 with hexdata 'c001' is
#       sent from the client side this module will reply with the same code and
#       data 'deadbeef'.

# Useful functions:
#   edns_opt_list_is_empty(edns_opt_list):
#       Check if the option list is empty.
#       Return True if empty, False otherwise.
#
#   edns_opt_list_append(edns_opt_list, code, data_bytearray, region): 
#       Append the EDNS option with code and data_bytearray to the given
#           edns_opt_list.
#       NOTE: data_bytearray MUST be a Python bytearray.
#       Return True on success, False on failure.
#
#   edns_opt_list_remove(edns_opt_list, code):
#       Remove all occurrences of the given EDNS option code from the
#           edns_opt_list.
#       Return True when at least one EDNS option was removed, False otherwise.
#
#   register_edns_option(env, code, bypass_cache_stage=True,
#                        no_aggregation=True):
#       Register EDNS option code as a known EDNS option.
#       bypass_cache_stage:
#           bypasses answering from cache and allows the query to reach the
#           modules for further EDNS handling.
#       no_aggregation:
#           makes every query with the said EDNS option code unique.
#       Return True on success, False on failure.
#
# Examples on how to use the functions are given in this file.


def init_standard(id, env):
    """New version of the init function.
    The function's signature is the same as the C counterpart and allows for
    extra functionality during init.
    ..note:: This function is preferred by unbound over the old init function.
    ..note:: The previously accessible configuration options can now be found in
             env.cfg.
    """
    log_info("python: inited script {}".format(env.cfg.python_script))

    # Register EDNS option 65001 as a known EDNS option.
    if not register_edns_option(env, 65001, bypass_cache_stage=True,
                                no_aggregation=True):
        return False

    return True


def init(id, cfg):
    """Previous version init function.
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
        # Detect if EDNS option code 56001 is present from the client side. If
        # so turn on the flags for cache management.
        if not edns_opt_list_is_empty(qstate.edns_opts_front_in):
            log_info("python: searching for EDNS option code 65001 during NEW "
                     "or PASS event ")
            for o in qstate.edns_opts_front_in_iter:
                if o.code == 65001:
                    log_info("python: found EDNS option code 65001")
                    # Instruct other modules to not lookup for an
                    # answer in the cache.
                    qstate.no_cache_lookup = 1
                    log_info("python: enabled no_cache_lookup")

                    # Instruct other modules to not store the answer in
                    # the cache.
                    qstate.no_cache_store = 1
                    log_info("python: enabled no_cache_store")

        #Pass on the query
        qstate.ext_state[id] = MODULE_WAIT_MODULE 
        return True

    elif event == MODULE_EVENT_MODDONE:
        # If the client sent EDNS option code 65001 and data 'c001' reply
        # with the same code and data 'deadbeef'.
        if not edns_opt_list_is_empty(qstate.edns_opts_front_in):
            log_info("python: searching for EDNS option code 65001 during "
                     "MODDONE")
            for o in qstate.edns_opts_front_in_iter:
                if o.code == 65001 and o.data == bytearray.fromhex("c001"):
                    b = bytearray.fromhex("deadbeef")
                    if not edns_opt_list_append(qstate.edns_opts_front_out,
                                           o.code, b, qstate.region):
                        qstate.ext_state[id] = MODULE_ERROR
                        return False

        # List every EDNS option in all lists.
        # The available lists are:
        #   - qstate.edns_opts_front_in:  EDNS options that came from the
        #                                 client side. SHOULD NOT be changed;
        #
        #   - qstate.edns_opts_back_out:  EDNS options that will be sent to the
        #                                 server side. Can be populated by
        #                                 EDNS literate modules;
        #
        #   - qstate.edns_opts_back_in:   EDNS options that came from the
        #                                 server side. SHOULD NOT be changed;
        #
        #   - qstate.edns_opts_front_out: EDNS options that will be sent to the
        #                                 client side. Can be populated by
        #                                 EDNS literate modules;
        #
        # The lists' contents can be accessed in python by their _iter
        # counterpart as an iterator.
        if not edns_opt_list_is_empty(qstate.edns_opts_front_in):
            log_info("python: EDNS options in edns_opts_front_in:")
            for o in qstate.edns_opts_front_in_iter:
                log_info("python:    Code: {}, Data: '{}'".format(o.code,
                                "".join('{:02x}'.format(x) for x in o.data)))

        if not edns_opt_list_is_empty(qstate.edns_opts_back_out):
            log_info("python: EDNS options in edns_opts_back_out:")
            for o in qstate.edns_opts_back_out_iter:
                log_info("python:    Code: {}, Data: '{}'".format(o.code,
                                "".join('{:02x}'.format(x) for x in o.data)))

        if not edns_opt_list_is_empty(qstate.edns_opts_back_in):
            log_info("python: EDNS options in edns_opts_back_in:")
            for o in qstate.edns_opts_back_in_iter:
                log_info("python:    Code: {}, Data: '{}'".format(o.code,
                                "".join('{:02x}'.format(x) for x in o.data)))

        if not edns_opt_list_is_empty(qstate.edns_opts_front_out):
            log_info("python: EDNS options in edns_opts_front_out:")
            for o in qstate.edns_opts_front_out_iter:
                log_info("python:    Code: {}, Data: '{}'".format(o.code,
                                "".join('{:02x}'.format(x) for x in o.data)))

        qstate.ext_state[id] = MODULE_FINISHED
        return True

    log_err("pythonmod: Unknown event")
    qstate.ext_state[id] = MODULE_ERROR
    return True
