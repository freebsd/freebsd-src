#!/usr/bin/env python
#
# Test for pyunbound lookup.
# BSD licensed.
#
import sys
import time

import unbound

qname = "www.example.com"
qname2 = "www2.example.com"
qtype = unbound.RR_TYPE_A
qclass = unbound.RR_CLASS_IN


def create_context(config_file="ub.lookup.conf", asyncflag=False):
    """
    Create an unbound context to use for testing.

    """
    ctx = unbound.ub_ctx()
    status = ctx.config(config_file)
    if status != 0:
        print("read config failed with status: {}".format(status))
        sys.exit(1)
    ctx.set_async(asyncflag)
    return ctx


def callback(data, status, result):
    """
    Callback for background workers.

    """
    if status == 0:
        data['rcode'] = result.rcode
        data['secure'] = result.secure
        if result.havedata:
            data['data'] = result.data
        data['was_ratelimited'] = result.was_ratelimited
    data['done'] = True


def test_resolve(ctx):
    """
    Test resolving a domain with a foreground worker.

    """
    status, result = ctx.resolve(qname, qtype, qclass)
    if status == 0 and result.havedata:
        print("Resolve: {}".format(result.data.address_list))
    else:
        print("Failed resolve with: {}".format(status))


def test_async_resolve(ctx):
    """
    Test resolving a domain with a background worker.

    """
    cb_data = dict(done=False)
    retval, async_id = ctx.resolve_async(qname, cb_data, callback, qtype, qclass)
    while retval == 0 and not cb_data['done']:
        time.sleep(0.1)
        retval = ctx.process()

    if cb_data.get('data'):
        print("Async resolve: {}".format(cb_data['data'].address_list))
    else:
        print("Failed async resolve with: {}".format(retval))


def test_ratelimit_bg_on(ctx):
    """
    Test resolving a ratelimited domain with a background worker.

    """
    ctx.set_option("ratelimit:", "1")
    ctx.set_option("ratelimit-factor:", "0")
    total_runs = 6
    success_threshold = 4  # 2/3*total_runs
    successes = 0
    for i in range(total_runs):
        cb_data = dict(done=False)
        cb_data2 = dict(done=False)
        retval, async_id = ctx.resolve_async(qname, cb_data, callback, qtype, qclass)
        retval, async_id = ctx.resolve_async(qname2, cb_data2, callback, qtype, qclass)

        while retval == 0 and not (cb_data['done'] and cb_data['done']):
            time.sleep(0.1)
            retval = ctx.process()

        if bool(cb_data.get('was_ratelimited')) ^ bool(cb_data2.get('was_ratelimited')):
            successes += 1
        if successes >= success_threshold:
            break
        time.sleep(1)
    if successes >= success_threshold:
        print("Ratelimit-bg-on: pass")
    else:
        print("Failed ratelimit-bg-on")


test_resolve(create_context())
test_async_resolve(create_context(asyncflag=True))
test_ratelimit_bg_on(create_context(asyncflag=True))

sys.exit(0)
