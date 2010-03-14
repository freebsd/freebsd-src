# $FreeBSD$

CAL_BIN="ncal"
CAL="${CAL_BIN}"
YEARS="2008 2009 2010 2011"
ONEYEAR="2009"

REGRESSION_START($1)

#
# The first tests are layout tests, to make sure that the output is still the
# same despite varying months.
#

# Full year calendars

echo 1..16

for y in ${YEARS}; do
	# Regular calendar, Month days, No-highlight
	REGRESSION_TEST(`r-y${y}-md-nhl', `$CAL -h ${y}')
	# Backwards calendar, Month days, No-highlight
	REGRESSION_TEST(`b-y${y}-md-nhl', `$CAL -bh ${y}')
	# Regular calendar, Julian days, No-highlight
	REGRESSION_TEST(`r-y${y}-jd-nhl', `$CAL -jh ${y}')
	# Backwards calendar, Julian days, No-highlight
	REGRESSION_TEST(`b-y${y}-jd-nhl', `$CAL -jbh ${y}')
done

# 3 month calendars

echo 17 .. 29

for m in $(jot -w %02d 12); do
	# Regular calendar, Month days, No-highlight
	REGRESSION_TEST(`r-3m${ONEYEAR}${m}-md-nhl', `$CAL -h3 ${m} ${ONEYEAR}')
	# Backwards calendar, Month days, No-highlight
	REGRESSION_TEST(`b-3m${ONEYEAR}${m}-md-nhl', `$CAL -bh3 ${m} ${ONEYEAR}')
	# Regular calendar, Julian days, No-highlight
	REGRESSION_TEST(`r-3m${ONEYEAR}${m}-jd-nhl', `$CAL -jh3 ${m} ${ONEYEAR}')
	# Backwards calendar, Julian days, No-highlight
	REGRESSION_TEST(`b-3m${ONEYEAR}${m}-jd-nhl', `$CAL -jbh3 ${m} ${ONEYEAR}')
done

#
# The next tests are combinations of the various arguments.
#

# These should fail
REGRESSION_TEST(`f-3y-nhl',  `$CAL -3 -y 2>&1')
REGRESSION_TEST(`f-3A-nhl',  `$CAL -3 -A 3 2>&1')
REGRESSION_TEST(`f-3B-nhl',  `$CAL -3 -B 3 2>&1')
REGRESSION_TEST(`f-3gy-nhl', `$CAL -3 2008 2>&1')
REGRESSION_TEST(`f-3AB-nhl', `$CAL -3 -A 3 -B 3 2>&1')
REGRESSION_TEST(`f-mgm-nhl', `$CAL -m 3 2 2008 2>&1')
REGRESSION_TEST(`f-ym-nhl',  `$CAL -y -m 2 2>&1')
REGRESSION_TEST(`f-ygm-nhl', `$CAL -y 2 2008 2>&1')
REGRESSION_TEST(`f-yA-nhl',  `$CAL -y -A 3 2>&1')
REGRESSION_TEST(`f-yB-nhl',  `$CAL -y -B 3 2>&1')
REGRESSION_TEST(`f-yAB-nhl', `$CAL -y -A 3 -B 3 2>&1')

# These should be successful

REGRESSION_TEST(`s-b-3-nhl',    `$CAL -b -d 2008.03 -3')
REGRESSION_TEST(`s-b-A-nhl',    `$CAL -b -d 2008.03 -A 1')
REGRESSION_TEST(`s-b-B-nhl',    `$CAL -b -d 2008.03 -B 1')
REGRESSION_TEST(`s-b-AB-nhl',   `$CAL -b -d 2008.03 -A 1 -B 1')
REGRESSION_TEST(`s-b-m-nhl',    `$CAL -b -d 2008.03 -m 1')
REGRESSION_TEST(`s-b-mgy-nhl',  `$CAL -b -d 2008.03 -m 1 2007')
REGRESSION_TEST(`s-b-gmgy-nhl', `$CAL -b -d 2008.03 1 2007')
REGRESSION_TEST(`s-r-3-nhl',    `$CAL -d 2008.03 -3')
REGRESSION_TEST(`s-r-A-nhl',    `$CAL -d 2008.03 -A 1')
REGRESSION_TEST(`s-r-B-nhl',    `$CAL -d 2008.03 -B 1')
REGRESSION_TEST(`s-r-AB-nhl',   `$CAL -d 2008.03 -A 1 -B 1')
REGRESSION_TEST(`s-r-m-nhl',    `$CAL -d 2008.03 -m 1')
REGRESSION_TEST(`s-r-mgy-nhl',  `$CAL -d 2008.03 -m 1 2007')
REGRESSION_TEST(`s-r-gmgy-nhl', `$CAL -d 2008.03 1 2007')

REGRESSION_END()
