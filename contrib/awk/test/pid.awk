# From: John C. Oppenheimer <jco@slinky.convex.com>
# Subject: gawk-3.0.2 pid test
# To: arnold@skeeve.atl.ga.us
# Date: Mon, 10 Feb 1997 08:31:55 -0600 (CST)
# 
# Thanks for the very quick reply.
# 
# This all started when I was looking for how to do the equivalent of
# "nextfile." I was after documentation and found our gawk down a few
# revs.
# 
# Looks like the nextfile functionality was added somewhere around
# 2.15.5.  There wasn't a way to do it, until now! Thanks for the
# functionality!
# 
# Saw the /dev/xxx capability and just tried it.
# 
# Anyway, I wrote a pid test.  I hope that it is portable.  Wanted to
# make a user test, but looks like id(1) is not very portable.  But a
# little test is better than none.
# 
# John
# 
# pid.ok is a zero length file
# 
# ================== pid.awk ============
BEGIN {
	getline pid <"/dev/pid"
	getline ppid <"/dev/ppid"
}
NR == 1 {
	if (pid != $0) {
		printf "Bad pid %d, wanted %d\n", $0, pid
	}
}
NR == 2 {
	if (ppid != $0) {
		printf "Bad ppid %d, wanted %d\n", $0, ppid
	}
}
END {	# ADR --- added
	close("/dev/pid")
	close("/dev/ppid")
}
