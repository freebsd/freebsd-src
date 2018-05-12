#!/usr/bin/env python
'''
Test for unbound lookup.
BSD licensed.
'''
import unbound

ctx = unbound.ub_ctx()
status = ctx.config("ub.conf")
if status != 0:
	print "read config failed ", status
	exit(1)

print "config created"

status, result = ctx.resolve("www.example.com", unbound.RR_TYPE_A, unbound.RR_CLASS_IN);
if status == 0 and result.havedata:
	print "Result: ", result.data.address_list
else:
	print "Failed ", status, " and data ", result

ctx = None

exit(0)
