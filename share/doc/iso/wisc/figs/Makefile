# $FreeBSD$
#
.SUFFIXES: .nr .grn

PRINTER = ba

ALL = \
	func_units.nr unix_ipc.nr osi_addr.nr trans_flow.nr clnp_output.nr\
	clnp_input.nr mbufsnd.nr mbufrcv.nr\
	ecn_vc.nr ecn_network.nr ecn_queue.nr tppt.nr

all: $(ALL)

clean:
	rm -f $(ALL)

.grn.nr:
	grn -P$(PRINTER) $*.grn > $*.nr
