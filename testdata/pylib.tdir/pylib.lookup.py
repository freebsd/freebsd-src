#!/usr/bin/env python
#
# Test for pyunbound lookup.
# BSD licensed.
#
import sys
import time

import unbound

qname = "www.example.com"
qtype = unbound.RR_TYPE_A
qclass = unbound.RR_CLASS_IN

def create_context(config_file="ub.lookup.conf", async=False):
    """
    Create an unbound context to use for testing.

    """
    ctx = unbound.ub_ctx()
    status = ctx.config(config_file)
    if status != 0:
        print("read config failed with status: {}".format(status))
        sys.exit(1)
    ctx.set_async(async)
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


def test_ratelimit_fg_on(ctx):
    """
    Test resolving a ratelimited domain with a foreground worker.

    """
    ctx.set_option("ratelimit:", "1")
    ctx.set_option("ratelimit-factor:", "0")
    status, result = ctx.resolve(qname, qtype, qclass)
    if status == 0 and result.was_ratelimited:
        print("Ratelimit-fg-on: pass")
    else:
        print("Failed ratelimit-fg-on with: {}".format(status))


def test_ratelimit_fg_off(ctx):
    """
    Test resolving a non-ratelimited domain with a foreground worker.

    """
    status, result = ctx.resolve(qname, qtype, qclass)
    if status == 0 and result.havedata:
        print("Ratelimit-fg-off: {}".format(result.data.address_list))
    else:
        print("Failed ratelimit-fg-off with: {}".format(status))


def test_ratelimit_bg_on(ctx):
    """
    Test resolving a ratelimited domain with a background worker.

    """
    ctx.set_option("ratelimit:", "1")
    ctx.set_option("ratelimit-factor:", "0")
    cb_data = dict(done=False)
    retval, async_id = ctx.resolve_async(qname, cb_data, callback, qtype, qclass)
    while retval == 0 and not cb_data['done']:
        time.sleep(0.1)
        retval = ctx.process()

    if cb_data.get('was_ratelimited'):
        print("Ratelimit-bg-on: pass")
    else:
        print("Failed ratelimit-bg-on with: {}".format(status))


def test_ratelimit_bg_off(ctx):
    """
    Test resolving a non-ratelimited domain with a background worker.

    """
    cb_data = dict(done=False)
    retval, async_id = ctx.resolve_async(qname, cb_data, callback, qtype, qclass)
    while retval == 0 and not cb_data['done']:
        time.sleep(0.1)
        retval = ctx.process()

    if cb_data.get('data'):
        print("Ratelimit-bg-off: {}".format(cb_data['data'].address_list))
    else:
        print("Failed ratelimit-bg-off with: {}".format(status))


test_resolve(create_context())
test_async_resolve(create_context(async=True))
test_ratelimit_fg_on(create_context())
test_ratelimit_fg_off(create_context())
test_ratelimit_bg_on(create_context(async=True))
test_ratelimit_bg_off(create_context(async=True))

sys.exit(0)
