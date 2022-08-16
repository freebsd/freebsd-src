# Check tz tables for consistency.

# @(#)checktab.awk	8.1

# Contributed by Paul Eggert.

BEGIN {
	FS = "\t"

	if (!iso_table) iso_table = "iso3166.tab"
	if (!zone_table) zone_table = "zone.tab"
	if (!want_warnings) want_warnings = -1

	while (getline <iso_table) {
		iso_NR++
		if ($0 ~ /^#/) continue
		if (NF != 2) {
			printf "%s:%d: wrong number of columns\n", \
				iso_table, iso_NR >>"/dev/stderr"
			status = 1
		}
		cc = $1
		name = $2
		if (cc !~ /^[A-Z][A-Z]$/) {
			printf "%s:%d: invalid country code `%s'\n", \
				iso_table, iso_NR, cc >>"/dev/stderr"
			status = 1
		}
		if (cc <= cc0) {
			if (cc == cc0) {
				s = "duplicate";
			} else {
				s = "out of order";
			}

			printf "%s:%d: country code `%s' is %s\n", \
				iso_table, iso_NR, cc, s \
				>>"/dev/stderr"
			status = 1
		}
		cc0 = cc
		if (name2cc[name]) {
			printf "%s:%d: `%s' and `%s' have the sname name\n", \
				iso_table, iso_NR, name2cc[name], cc \
				>>"/dev/stderr"
			status = 1
		}
		name2cc[name] = cc
		cc2name[cc] = name
		cc2NR[cc] = iso_NR
	}

	zone_table = "zone.tab"
	cc0 = ""

	while (getline <zone_table) {
		zone_NR++
		if ($0 ~ /^#/) continue
		if (NF != 3 && NF != 4) {
			printf "%s:%d: wrong number of columns\n", \
				zone_table, zone_NR >>"/dev/stderr"
			status = 1
		}
		cc = $1
		coordinates = $2
		tz = $3
		comments = $4
		if (cc < cc0) {
			printf "%s:%d: country code `%s' is out of order\n", \
				zone_table, zone_NR, cc >>"/dev/stderr"
			status = 1
		}
		cc0 = cc
		if (tz2cc[tz]) {
			printf "%s:%d: %s: duplicate TZ column\n", \
				zone_table, zone_NR, tz >>"/dev/stderr"
			status = 1
		}
		tz2cc[tz] = cc
		tz2comments[tz] = comments
		tz2NR[tz] = zone_NR
		if (cc2name[cc]) {
			cc_used[cc]++
		} else {
			printf "%s:%d: %s: unknown country code\n", \
				zone_table, zone_NR, cc >>"/dev/stderr"
			status = 1
		}
		if (coordinates !~ /^[-+][0-9][0-9][0-5][0-9][-+][01][0-9][0-9][0-5][0-9]$/ \
		    && coordinates !~ /^[-+][0-9][0-9][0-5][0-9][0-5][0-9][-+][01][0-9][0-9][0-5][0-9][0-5][0-9]$/) {
			printf "%s:%d: %s: invalid coordinates\n", \
				zone_table, zone_NR, coordinates >>"/dev/stderr"
			status = 1
		}
	}

	for (tz in tz2cc) {
		if (cc_used[tz2cc[tz]] == 1) {
			if (tz2comments[tz]) {
				printf "%s:%d: unnecessary comment `%s'\n", \
					zone_table, tz2NR[tz], tz2comments[tz] \
					>>"/dev/stderr"
				status = 1
			}
		} else {
			if (!tz2comments[tz]) {
				printf "%s:%d: missing comment\n", \
					zone_table, tz2NR[tz] >>"/dev/stderr"
				status = 1
			}
		}
	}

	FS = " "
}

{
	tz = ""
	if ($1 == "Zone") tz = $2
	if ($1 == "Link") {
		# Ignore Link commands if source and destination basenames
		# are identical, e.g. Europe/Istanbul versus Asia/Istanbul.
		src = $2
		dst = $3
		while ((i = index(src, "/"))) src = substr(src, i+1)
		while ((i = index(dst, "/"))) dst = substr(dst, i+1)
		if (src != dst) tz = $3
	}
	if (tz && tz ~ /\//) {
		if (!tz2cc[tz]) {
			printf "%s: no data for `%s'\n", zone_table, tz \
				>>"/dev/stderr"
			status = 1
		}
		zoneSeen[tz] = 1
	}
}

END {
	for (tz in tz2cc) {
		if (!zoneSeen[tz]) {
			printf "%s:%d: no Zone table for `%s'\n", \
				zone_table, tz2NR[tz], tz >>"/dev/stderr"
			status = 1
		}
	}

	if (0 < want_warnings) {
		for (cc in cc2name) {
			if (!cc_used[cc]) {
				printf "%s:%d: warning: " \
					"no Zone entries for %s (%s)\n", \
					iso_table, cc2NR[cc], cc, cc2name[cc]
			}
		}
	}

	exit status
}
