#! /bin/sh
#
# $FreeBSD$

# Print the modification date of $1 `nicely'.

# Don't want foreign dates.

LANGUAGE=
LC_ALL=C; export LC_ALL


(date;
if ls -L /dev/null 1>/dev/null 2>&1; then ls -L -l $1; else ls -l $1; fi
) | awk '
BEGIN {
	full["Jan"] = "January"; number["Jan"] = 1;
	full["Feb"] = "February"; number["Feb"] = 2;
	full["Mar"] = "March"; number["Mar"] = 3;
	full["Apr"] = "April"; number["Apr"] = 4;
	full["May"] = "May"; number["May"] = 5;
	full["Jun"] = "June"; number["Jun"] = 6;
	full["Jul"] = "July"; number["Jul"] = 7;
	full["Aug"] = "August"; number["Aug"] = 8;
	full["Sep"] = "September"; number["Sep"] = 9;
	full["Oct"] = "October"; number["Oct"] = 10;
	full["Nov"] = "November"; number["Nov"] = 11;
	full["Dec"] = "December"; number["Dec"] = 12;
}

NR == 1 {
	month = $2;
	year = $NF;
}

NR == 2 {
	if ($(NF-1) ~ /:/) {
		if (number[$(NF-3)] > number[month])
			year--;
	}
	else
		year = $(NF-1);
	print $(NF-2), full[$(NF-3)], year
}'
