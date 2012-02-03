The contents of this directory sub tree is dedicated to regression testing
of IPFilter.

The tests are broken down into these groups:
f    - filter rule tests
i    - parsing & printing test of ipf rules
in   - parsing & printing test of ipnat rules
ipv6 - ipv6 filter rule tests
l    - logging test
n    - NAT testing
ni   - combined NAT & IPF tests

      TEST
f1  - block/pass, in/out.
f2  - proto
f3  - from IP#
f4  - to #IP
f5  - source port
f6  - destination port
f7  - icmp-type, code
f8  - flags
f9  - ipoptions
f10 - ipoptions
f11 - keep frag/state
f12 - short/frag
f13 - keep frag/state (fragmented packets)
f14 - from !host, to !host
f15 - groups
f16 - skip
f17 - TCP state transition on flags
