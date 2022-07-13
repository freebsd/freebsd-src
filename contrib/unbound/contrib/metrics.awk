# read output of unbound-control stats
# and output prometheus metrics style output.
# use these options:
#	server:		extended-statistics: yes
#			statistics-cumulative: no
#			statistics-interval: 0
#	remote-control: control-enable: yes
# Can use it like unbound-control stats | awk -f "metrics.awk"

BEGIN {
	FS="=";
}
# everything like total.num.queries=value is put in val["total.num.queries"]
/^.*\..*=/ {
	val[$1]=$2;
}
# print the output metrics
END {
	print "# HELP unbound_hits_queries Unbound DNS traffic and cache hits"
	print "# TYPE unbound_hits_queries gauge"
	print "unbound_hits_queries{type=\"total.num.queries\"} " val["total.num.queries"];
	for (x=0; x<99; x++) {
		if(val["thread" $x ".num.queries"] != "") {
			print "unbound_hits_queries{type=\"thread" $x ".num.queries\"} " val["thread" $x ".num.queries"];
		}
	}
	print "unbound_hits_queries{type=\"total.num.cachehits\"} " val["total.num.cachehits"];
	print "unbound_hits_queries{type=\"total.num.prefetch\"} " val["total.num.prefetch"];
	print "unbound_hits_queries{type=\"num.query.tcp\"} " val["num.query.tcp"];
	print "unbound_hits_queries{type=\"num.query.tcpout\"} " val["num.query.tcpout"];
	print "unbound_hits_queries{type=\"num.query.udpout\"} " val["num.query.udpout"];
	print "unbound_hits_queries{type=\"num.query.tls\"} " val["num.query.tls"];
	print "unbound_hits_queries{type=\"num.query.tls.resume\"} " val["num.query.tls.resume"];
	print "unbound_hits_queries{type=\"num.query.ipv6\"} " val["num.query.ipv6"];
	print "unbound_hits_queries{type=\"unwanted.queries\"} " val["unwanted.queries"];
	print ""

	print "# HELP unbound_queue_queries Unbound requestlist size"
	print "# TYPE unbound_queue_queries gauge"
	print "unbound_queue_queries{type=\"total.requestlist.avg\"} " val["total.requestlist.avg"];
	print "unbound_queue_queries{type=\"total.requestlist.max\"} " val["total.requestlist.max"];
	print "unbound_queue_queries{type=\"total.requestlist.overwritten\"} " val["total.requestlist.overwritten"];
	print "unbound_queue_queries{type=\"total.requestlist.exceeded\"} " val["total.requestlist.exceeded"];
	print ""

	print "# HELP unbound_memory_bytes Unbound memory usage"
	print "# TYPE unbound_memory_bytes gauge"
	print "unbound_memory_bytes{type=\"mem.cache.rrset\"} " val["mem.cache.rrset"];
	print "unbound_memory_bytes{type=\"mem.cache.message\"} " val["mem.cache.message"];
	print "unbound_memory_bytes{type=\"mem.mod.iterator\"} " val["mem.mod.iterator"];
	if(val["mem.mod.validator"] != "") {
		print "unbound_memory_bytes{type=\"mem.mod.validator\"} " val["mem.mod.validator"];
	}
	if(val["mem.mod.respip"] != "") {
		print "unbound_memory_bytes{type=\"mem.mod.respip\"} " val["mem.mod.respip"];
	}
	if(val["mem.mod.subnet"] != "") {
		print "unbound_memory_bytes{type=\"mem.mod.subnet\"} " val["mem.mod.subnet"];
	}
	if(val["mem.mod.ipsecmod"] != "") {
		print "unbound_memory_bytes{type=\"mem.mod.ipsecmod\"} " val["mem.mod.ipsecmod"];
	}
	if(val["mem.mod.dynlibmod"] != "") {
		print "unbound_memory_bytes{type=\"mem.mod.dynlibmod\"} " val["mem.mod.dynlibmod"];
	}
	print "unbound_memory_bytes{type=\"msg.cache.count\"} " val["msg.cache.count"];
	print "unbound_memory_bytes{type=\"rrset.cache.count\"} " val["rrset.cache.count"];
	print "unbound_memory_bytes{type=\"infra.cache.count\"} " val["infra.cache.count"];
	print "unbound_memory_bytes{type=\"key.cache.count\"} " val["key.cache.count"];
	print ""

	print "# HELP unbound_by_type_queries Unbound DNS queries by type"
	print "# TYPE unbound_by_type_queries gauge"
	for(x in val) {
		if(x ~ /^num.query.type./) {
			if(val[x] != "") {
				split(x, a, ".");
				print "unbound_by_type_queries{type=\"" a[4] "\"} " val[x];
			}
		}
	}
	print ""

	print "# HELP unbound_by_class_queries Unbound DNS queries by class"
	print "# TYPE unbound_by_class_queries gauge"
	for(x in val) {
		if(x ~ /^num.query.class./) {
			if(val[x] != "") {
				split(x, a, ".");
				print "unbound_by_class_queries{class=\"" a[4] "\"} " val[x];
			}
		}
	}
	print ""

	print "# HELP unbound_by_opcode_queries Unbound DNS queries by opcode"
	print "# TYPE unbound_by_opcode_queries gauge"
	for(x in val) {
		if(x ~ /^num.query.opcode./) {
			if(val[x] != "") {
				split(x, a, ".");
				print "unbound_by_opcode_queries{opcode=\"" a[4] "\"} " val[x];
			}
		}
	}
	print ""

	print "# HELP unbound_by_rcode_queries Unbound DNS answers by rcode"
	print "# TYPE unbound_by_rcode_queries gauge"
	for(x in val) {
		if(x ~ /^num.answer.rcode./) {
			if(val[x] != "") {
				split(x, a, ".");
				print "unbound_by_rcode_queries{rcode=\"" a[4] "\"} " val[x];
			}
		}
	}
	print ""

	print "# HELP unbound_by_flags_queries Unbound DNS queries by flags"
	print "# TYPE unbound_by_flags_queries gauge"
	for(x in val) {
		if(x ~ /^num.query.flags./) {
			if(val[x] != "") {
				split(x, a, ".");
				print "unbound_by_flags_queries{flag=\"" a[4] "\"} " val[x];
			}
		}
	}
	if(val["num.query.edns.present"] != "") {
		print "unbound_by_flags_queries{flag=\"num.query.edns.present\"} " val["num.query.edns.present"];
	}
	if(val["num.query.edns.DO"] != "") {
		print "unbound_by_flags_queries{flag=\"num.query.edns.DO\"} " val["num.query.edns.DO"];
	}
	print ""

	print "# HELP unbound_histogram_seconds Unbound DNS histogram of reply time"
	print "# TYPE unbound_histogram_seconds gauge"
	print "unbound_histogram_seconds{bucket=\"000000.000000.to.000000.000001\"} " val["histogram.000000.000000.to.000000.000001"];
	print "unbound_histogram_seconds{bucket=\"000000.000001.to.000000.000002\"} " val["histogram.000000.000001.to.000000.000002"];
	print "unbound_histogram_seconds{bucket=\"000000.000002.to.000000.000004\"} " val["histogram.000000.000002.to.000000.000004"];
	print "unbound_histogram_seconds{bucket=\"000000.000004.to.000000.000008\"} " val["histogram.000000.000004.to.000000.000008"];
	print "unbound_histogram_seconds{bucket=\"000000.000008.to.000000.000016\"} " val["histogram.000000.000008.to.000000.000016"];
	print "unbound_histogram_seconds{bucket=\"000000.000016.to.000000.000032\"} " val["histogram.000000.000016.to.000000.000032"];
	print "unbound_histogram_seconds{bucket=\"000000.000032.to.000000.000064\"} " val["histogram.000000.000032.to.000000.000064"];
	print "unbound_histogram_seconds{bucket=\"000000.000064.to.000000.000128\"} " val["histogram.000000.000064.to.000000.000128"];
	print "unbound_histogram_seconds{bucket=\"000000.000128.to.000000.000256\"} " val["histogram.000000.000128.to.000000.000256"];
	print "unbound_histogram_seconds{bucket=\"000000.000256.to.000000.000512\"} " val["histogram.000000.000256.to.000000.000512"];
	print "unbound_histogram_seconds{bucket=\"000000.000512.to.000000.001024\"} " val["histogram.000000.000512.to.000000.001024"];
	print "unbound_histogram_seconds{bucket=\"000000.001024.to.000000.002048\"} " val["histogram.000000.001024.to.000000.002048"];
	print "unbound_histogram_seconds{bucket=\"000000.002048.to.000000.004096\"} " val["histogram.000000.002048.to.000000.004096"];
	print "unbound_histogram_seconds{bucket=\"000000.004096.to.000000.008192\"} " val["histogram.000000.004096.to.000000.008192"];
	print "unbound_histogram_seconds{bucket=\"000000.008192.to.000000.016384\"} " val["histogram.000000.008192.to.000000.016384"];
	print "unbound_histogram_seconds{bucket=\"000000.016384.to.000000.032768\"} " val["histogram.000000.016384.to.000000.032768"];
	print "unbound_histogram_seconds{bucket=\"000000.032768.to.000000.065536\"} " val["histogram.000000.032768.to.000000.065536"];
	print "unbound_histogram_seconds{bucket=\"000000.065536.to.000000.131072\"} " val["histogram.000000.065536.to.000000.131072"];
	print "unbound_histogram_seconds{bucket=\"000000.131072.to.000000.262144\"} " val["histogram.000000.131072.to.000000.262144"];
	print "unbound_histogram_seconds{bucket=\"000000.262144.to.000000.524288\"} " val["histogram.000000.262144.to.000000.524288"];
	print "unbound_histogram_seconds{bucket=\"000000.524288.to.000001.000000\"} " val["histogram.000000.524288.to.000001.000000"];
	print "unbound_histogram_seconds{bucket=\"000001.000000.to.000002.000000\"} " val["histogram.000001.000000.to.000002.000000"];
	print "unbound_histogram_seconds{bucket=\"000002.000000.to.000004.000000\"} " val["histogram.000002.000000.to.000004.000000"];
	print "unbound_histogram_seconds{bucket=\"000004.000000.to.000008.000000\"} " val["histogram.000004.000000.to.000008.000000"];
	print "unbound_histogram_seconds{bucket=\"000008.000000.to.000016.000000\"} " val["histogram.000008.000000.to.000016.000000"];
	print "unbound_histogram_seconds{bucket=\"000016.000000.to.000032.000000\"} " val["histogram.000016.000000.to.000032.000000"];
	print "unbound_histogram_seconds{bucket=\"000032.000000.to.000064.000000\"} " val["histogram.000032.000000.to.000064.000000"];
	print "unbound_histogram_seconds{bucket=\"000064.000000.to.000128.000000\"} " val["histogram.000064.000000.to.000128.000000"];
	print "unbound_histogram_seconds{bucket=\"000128.000000.to.000256.000000\"} " val["histogram.000128.000000.to.000256.000000"];
	print "unbound_histogram_seconds{bucket=\"000256.000000.to.000512.000000\"} " val["histogram.000256.000000.to.000512.000000"];
	print "unbound_histogram_seconds{bucket=\"000512.000000.to.001024.000000\"} " val["histogram.000512.000000.to.001024.000000"];
	print "unbound_histogram_seconds{bucket=\"001024.000000.to.002048.000000\"} " val["histogram.001024.000000.to.002048.000000"];
	print "unbound_histogram_seconds{bucket=\"002048.000000.to.004096.000000\"} " val["histogram.002048.000000.to.004096.000000"];
	print "unbound_histogram_seconds{bucket=\"004096.000000.to.008192.000000\"} " val["histogram.004096.000000.to.008192.000000"];
	print "unbound_histogram_seconds{bucket=\"008192.000000.to.016384.000000\"} " val["histogram.008192.000000.to.016384.000000"];
	print "unbound_histogram_seconds{bucket=\"016384.000000.to.032768.000000\"} " val["histogram.016384.000000.to.032768.000000"];
	print "unbound_histogram_seconds{bucket=\"032768.000000.to.065536.000000\"} " val["histogram.032768.000000.to.065536.000000"];
	print "unbound_histogram_seconds{bucket=\"065536.000000.to.131072.000000\"} " val["histogram.065536.000000.to.131072.000000"];
	print "unbound_histogram_seconds{bucket=\"131072.000000.to.262144.000000\"} " val["histogram.131072.000000.to.262144.000000"];
	print "unbound_histogram_seconds{bucket=\"262144.000000.to.524288.000000\"} " val["histogram.262144.000000.to.524288.000000"];
	print ""
}
