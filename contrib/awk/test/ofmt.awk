# From dragon!knorke.saar.de!florian Wed Jul 16 10:47:27 1997
# Return-Path: <dragon!knorke.saar.de!florian>
# Message-ID: <19970716164451.63610@knorke.saar.de>
# Date: Wed, 16 Jul 1997 16:44:51 +0200
# From: Florian La Roche <florian@knorke.saar.de>
# To: bug-gnu-utils@prep.ai.mit.edu
# CC: arnold@gnu.ai.mit.edu
# Subject: bug in gawk 3.0.3
# MIME-Version: 1.0
# Content-Type: text/plain; charset=us-ascii
# X-Mailer: Mutt 0.76
# Status: R
# Content-Length: 1725
# X-Lines: 177
# X-Display-Position: 0
# 
# I have a problem with gawk 3.0.3 on linux with libc 5.4.33.
# The memory is corrupted, if I use OFMT = "%.12g".
# With OFMT = "%.6g" evrything works fine, but I don't have enough
# digits for the computation.
# 
# Thanks a lot,
# Florian La Roche
# 
# Here is the sample awk-Script together with sample data:
# 
BEGIN {
		OFMT = "%.12g"
		big = 99999999999
		lowest = big
		small = 0
		highest = small
		dir = ""
	}
$0 ~ /^[0-9]+$/ {
	# some old awks do not think $0 is numeric, so use $1
	if ($1 < lowest)
		lowest = $1
	if ($1 > highest)
		highest = $1
	next
}
$0 ~ /\/\.:$/ {
	if (dir != "") {
		if (highest != small)
			print dir, highest, lowest
		else
			print dir, "-", "-"
	}
	dir = substr($0, 1, length($0)-3)	# trim off /.:
	lowest = big
	highest = small
}
