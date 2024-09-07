#!/bin/bash
# Ask the user about the time zone, and output the resulting TZ value to stdout.
# Interact with the user via stderr and stdin.

PKGVERSION='(tzcode) '
TZVERSION=see_Makefile
REPORT_BUGS_TO=tz@iana.org

# Contributed by Paul Eggert.  This file is in the public domain.

# Porting notes:
#
# This script requires a POSIX-like shell and prefers the extension of a
# 'select' statement.  The 'select' statement was introduced in the
# Korn shell and is available in Bash and other shell implementations.
# If your host lacks both Bash and the Korn shell, you can get their
# source from one of these locations:
#
#	Bash <https://www.gnu.org/software/bash/>
#	Korn Shell <http://www.kornshell.com/>
#	MirBSD Korn Shell <http://www.mirbsd.org/mksh.htm>
#
# This script also uses several features of POSIX awk.
# If your host lacks awk, or has an old awk that does not conform to POSIX,
# you can use any of the following free programs instead:
#
#	Gawk (GNU awk) <https://www.gnu.org/software/gawk/>
#	mawk <https://invisible-island.net/mawk/>
#	nawk <https://github.com/onetrueawk/awk>
#
# Because 'awk "VAR=VALUE" ...' and 'awk -v "VAR=VALUE" ...' are not portable
# if VALUE contains \, ", or newline, awk scripts in this file use:
#   awk 'BEGIN { VAR = substr(ARGV[1], 2); ARGV[1] = "" } ...' ="VALUE"
# The substr avoids problems when VALUE is of the form X=Y and would be
# misinterpreted as an assignment.

# This script does not want path expansion.
set -f

# Specify default values for environment variables if they are unset.
: ${AWK=awk}
: ${TZDIR=$PWD}

# Output one argument as-is to standard output, with trailing newline.
# Safer than 'echo', which can mishandle '\' or leading '-'.
say() {
  printf '%s\n' "$1"
}

coord=
location_limit=10
zonetabtype=zone1970

usage="Usage: tzselect [--version] [--help] [-c COORD] [-n LIMIT]
Select a timezone interactively.

Options:

  -c COORD
    Instead of asking for continent and then country and then city,
    ask for selection from time zones whose largest cities
    are closest to the location with geographical coordinates COORD.
    COORD should use ISO 6709 notation, for example, '-c +4852+00220'
    for Paris (in degrees and minutes, North and East), or
    '-c -35-058' for Buenos Aires (in degrees, South and West).

  -n LIMIT
    Display at most LIMIT locations when -c is used (default $location_limit).

  --version
    Output version information.

  --help
    Output this help.

Report bugs to $REPORT_BUGS_TO."

# Ask the user to select from the function's arguments,
# and assign the selected argument to the variable 'select_result'.
# Exit on EOF or I/O error.  Use the shell's nicer 'select' builtin if
# available, falling back on a portable substitute otherwise.
if
  case $BASH_VERSION in
  ?*) :;;
  '')
    # '; exit' should be redundant, but Dash doesn't properly fail without it.
    (eval 'set --; select x; do break; done; exit') <>/dev/null 2>&0
  esac
then
  # Do this inside 'eval', as otherwise the shell might exit when parsing it
  # even though it is never executed.
  eval '
    doselect() {
      select select_result
      do
	case $select_result in
	"") echo >&2 "Please enter a number in range.";;
	?*) break
	esac
      done || exit
    }
  '
else
  doselect() {
    # Field width of the prompt numbers.
    select_width=${##}

    select_i=

    while :
    do
      case $select_i in
      '')
	select_i=0
	for select_word
	do
	  select_i=$(($select_i + 1))
	  printf >&2 "%${select_width}d) %s\\n" $select_i "$select_word"
	done;;
      *[!0-9]*)
	echo >&2 'Please enter a number in range.';;
      *)
	if test 1 -le $select_i && test $select_i -le $#; then
	  shift $(($select_i - 1))
	  select_result=$1
	  break
	fi
	echo >&2 'Please enter a number in range.'
      esac

      # Prompt and read input.
      printf >&2 %s "${PS3-#? }"
      read select_i || exit
    done
  }
fi

while getopts c:n:t:-: opt
do
  case $opt$OPTARG in
  c*)
    coord=$OPTARG;;
  n*)
    location_limit=$OPTARG;;
  t*) # Undocumented option, used for developer testing.
    zonetabtype=$OPTARG;;
  -help)
    exec echo "$usage";;
  -version)
    exec echo "tzselect $PKGVERSION$TZVERSION";;
  -*)
    say >&2 "$0: -$opt$OPTARG: unknown option; try '$0 --help'"; exit 1;;
  *)
    say >&2 "$0: try '$0 --help'"; exit 1
  esac
done

shift $(($OPTIND - 1))
case $# in
0) ;;
*) say >&2 "$0: $1: unknown argument"; exit 1
esac

# translit=true to try transliteration.
# This is false if U+12345 CUNEIFORM SIGN URU TIMES KI has length 1
# which means the shell and (presumably) awk do not need transliteration.
# It is true if the byte string has some other length in characters, or
# if this is a POSIX.1-2017 or earlier shell that does not support $'...'.
CUNEIFORM_SIGN_URU_TIMES_KI=$'\360\222\215\205'
if test ${#CUNEIFORM_SIGN_URU_TIMES_KI} = 1
then translit=false
else translit=true
fi

# Read into shell variable $1 the contents of file $2.
# Convert to the current locale's encoding if possible,
# as the shell aligns columns better that way.
# If GNU iconv's //TRANSLIT does not work, fall back on POSIXish iconv;
# if that does not work, fall back on 'cat'.
read_file() {
  { $translit && {
    eval "$1=\$( (iconv -f UTF-8 -t //TRANSLIT) 2>/dev/null <\"\$2\")" ||
    eval "$1=\$( (iconv -f UTF-8) 2>/dev/null <\"\$2\")"
  }; } ||
  eval "$1=\$(cat <\"\$2\")" || {
    say >&2 "$0: time zone files are not set up correctly"
    exit 1
  }
}
read_file TZ_COUNTRY_TABLE "$TZDIR/iso3166.tab"
read_file TZ_ZONETABTYPE_TABLE "$TZDIR/$zonetabtype.tab"
TZ_ZONENOW_TABLE=

newline='
'
IFS=$newline

# Awk script to output a country list.
output_country_list='
  BEGIN {
    continent_re = substr(ARGV[1], 2)
    TZ_COUNTRY_TABLE = substr(ARGV[2], 2)
    TZ_ZONE_TABLE = substr(ARGV[3], 2)
    ARGV[1] = ARGV[2] = ARGV[3] = ""
    FS = "\t"
    nlines = split(TZ_ZONE_TABLE, line, /\n/)
    for (iline = 1; iline <= nlines; iline++) {
      $0 = line[iline]
      commentary = $0 ~ /^#@/
      if (commentary) {
	if ($0 !~ /^#@/)
	  continue
	col1ccs = substr($1, 3)
	conts = $2
      } else {
	col1ccs = $1
	conts = $3
      }
      ncc = split(col1ccs, cc, /,/)
      ncont = split(conts, cont, /,/)
      for (i = 1; i <= ncc; i++) {
	elsewhere = commentary
	for (ci = 1; ci <= ncont; ci++) {
	  if (cont[ci] ~ continent_re) {
	    if (!cc_seen[cc[i]]++)
	      cc_list[++ccs] = cc[i]
	    elsewhere = 0
	  }
	}
	if (elsewhere)
	  for (i = 1; i <= ncc; i++)
	    cc_elsewhere[cc[i]] = 1
      }
    }
    nlines = split(TZ_COUNTRY_TABLE, line, /\n/)
    for (i = 1; i <= nlines; i++) {
      $0 = line[i]
      if ($0 !~ /^#/)
	cc_name[$1] = $2
    }
    for (i = 1; i <= ccs; i++) {
      country = cc_list[i]
      if (cc_elsewhere[country])
	continue
      if (cc_name[country])
	country = cc_name[country]
      print country
    }
  }
'

# Awk script to process a time zone table and output the same table,
# with each row preceded by its distance from 'here'.
# If output_times is set, each row is instead preceded by its local time
# and any apostrophes are escaped for the shell.
output_distances_or_times='
  BEGIN {
    coord = substr(ARGV[1], 2)
    TZ_COUNTRY_TABLE = substr(ARGV[2], 2)
    TZ_ZONE_TABLE = substr(ARGV[3], 2)
    ARGV[1] = ARGV[2] = ARGV[3] = ""
    FS = "\t"
    if (!output_times) {
      nlines = split(TZ_COUNTRY_TABLE, line, /\n/)
      for (i = 1; i <= nlines; i++) {
	$0 = line[i]
	if ($0 ~ /^#/)
	  continue
	country[$1] = $2
      }
      country["US"] = "US" # Otherwise the strings get too long.
    }
  }
  function abs(x) {
    return x < 0 ? -x : x;
  }
  function min(x, y) {
    return x < y ? x : y;
  }
  function convert_coord(coord, deg, minute, ilen, sign, sec) {
    if (coord ~ /^[-+]?[0-9]?[0-9][0-9][0-9][0-9][0-9][0-9]([^0-9]|$)/) {
      degminsec = coord
      intdeg = degminsec < 0 ? -int(-degminsec / 10000) : int(degminsec / 10000)
      minsec = degminsec - intdeg * 10000
      intmin = minsec < 0 ? -int(-minsec / 100) : int(minsec / 100)
      sec = minsec - intmin * 100
      deg = (intdeg * 3600 + intmin * 60 + sec) / 3600
    } else if (coord ~ /^[-+]?[0-9]?[0-9][0-9][0-9][0-9]([^0-9]|$)/) {
      degmin = coord
      intdeg = degmin < 0 ? -int(-degmin / 100) : int(degmin / 100)
      minute = degmin - intdeg * 100
      deg = (intdeg * 60 + minute) / 60
    } else
      deg = coord
    return deg * 0.017453292519943296
  }
  function convert_latitude(coord) {
    match(coord, /..*[-+]/)
    return convert_coord(substr(coord, 1, RLENGTH - 1))
  }
  function convert_longitude(coord) {
    match(coord, /..*[-+]/)
    return convert_coord(substr(coord, RLENGTH))
  }
  # Great-circle distance between points with given latitude and longitude.
  # Inputs and output are in radians.  This uses the great-circle special
  # case of the Vicenty formula for distances on ellipsoids.
  function gcdist(lat1, long1, lat2, long2, dlong, x, y, num, denom) {
    dlong = long2 - long1
    x = cos(lat2) * sin(dlong)
    y = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dlong)
    num = sqrt(x * x + y * y)
    denom = sin(lat1) * sin(lat2) + cos(lat1) * cos(lat2) * cos(dlong)
    return atan2(num, denom)
  }
  # Parallel distance between points with given latitude and longitude.
  # This is the product of the longitude difference and the cosine
  # of the latitude of the point that is further from the equator.
  # I.e., it considers longitudes to be further apart if they are
  # nearer the equator.
  function pardist(lat1, long1, lat2, long2) {
    return abs(long1 - long2) * min(cos(lat1), cos(lat2))
  }
  # The distance function is the sum of the great-circle distance and
  # the parallel distance.  It could be weighted.
  function dist(lat1, long1, lat2, long2) {
    return gcdist(lat1, long1, lat2, long2) + pardist(lat1, long1, lat2, long2)
  }
  BEGIN {
    coord_lat = convert_latitude(coord)
    coord_long = convert_longitude(coord)
    nlines = split(TZ_ZONE_TABLE, line, /\n/)
    for (h = 1; h <= nlines; h++) {
      $0 = line[h]
      if ($0 ~ /^#/)
	continue
      inline[inlines++] = $0
      ncc = split($1, cc, /,/)
      for (i = 1; i <= ncc; i++)
	cc_used[cc[i]]++
    }
    for (h = 0; h < inlines; h++) {
      $0 = inline[h]
      outline = $1 "\t" $2 "\t" $3
      sep = "\t"
      ncc = split($1, cc, /,/)
      split("", item_seen)
      item_seen[""] = 1
      for (i = 1; i <= ncc; i++) {
	item = cc_used[cc[i]] <= 1 ? country[cc[i]] : $4
	if (item_seen[item]++)
	  continue
	outline = outline sep item
	sep = "; "
      }
      if (output_times) {
	fmt = "TZ='\''%s'\'' date +'\''%d %%Y %%m %%d %%H:%%M %%a %%b\t%s'\''\n"
	gsub(/'\''/, "&\\\\&&", outline)
	printf fmt, $3, h, outline
      } else {
	here_lat = convert_latitude($2)
	here_long = convert_longitude($2)
	printf "%g\t%s\n", dist(coord_lat, coord_long, here_lat, here_long), \
	  outline
      }
    }
  }
'

# Begin the main loop.  We come back here if the user wants to retry.
while

  echo >&2 'Please identify a location' \
    'so that time zone rules can be set correctly.'

  continent=
  country=
  country_result=
  region=
  time=
  TZ_ZONE_TABLE=$TZ_ZONETABTYPE_TABLE

  case $coord in
  ?*)
    continent=coord;;
  '')

    # Ask the user for continent or ocean.

    echo >&2 \
      'Please select a continent, ocean, "coord", "TZ", "time", or "now".'

    quoted_continents=$(
      $AWK '
	function handle_entry(entry) {
	  entry = substr(entry, 1, index(entry, "/") - 1)
	  if (entry == "America")
	    entry = entry "s"
	  if (entry ~ /^(Arctic|Atlantic|Indian|Pacific)$/)
	    entry = entry " Ocean"
	  printf "'\''%s'\''\n", entry
	}
	BEGIN {
	  TZ_ZONETABTYPE_TABLE = substr(ARGV[1], 2)
	  ARGV[1] = ""
	  FS = "\t"
	  nlines = split(TZ_ZONETABTYPE_TABLE, line, /\n/)
	  for (i = 1; i <= nlines; i++) {
	    $0 = line[i]
	    if ($0 ~ /^[^#]/)
	      handle_entry($3)
	    else if ($0 ~ /^#@/) {
	      ncont = split($2, cont, /,/)
	      for (ci = 1; ci <= ncont; ci++)
		handle_entry(cont[ci])
	    }
	  }
	}
      ' ="$TZ_ZONETABTYPE_TABLE" |
      sort -u |
      tr '\n' ' '
      echo ''
    )

    eval '
      doselect '"$quoted_continents"' \
	"coord - I want to use geographical coordinates." \
	"TZ - I want to specify the timezone using a proleptic TZ string." \
	"time - I know local time already." \
	"now - Like \"time\", but configure only for timestamps from now on."
      continent=$select_result
      case $continent in
      Americas) continent=America;;
      *)
	# Get the first word of $continent.  Path expansion is disabled
	# so this works even with "*", which should not happen.
	IFS=" "
	for continent in $continent ""; do break; done
	IFS=$newline;;
      esac
      case $zonetabtype,$continent in
      zonenow,*) ;;
      *,now)
	${TZ_ZONENOW_TABLE:+:} read_file TZ_ZONENOW_TABLE "$TZDIR/zonenow.tab"
	TZ_ZONE_TABLE=$TZ_ZONENOW_TABLE
      esac
    '
  esac

  case $continent in
  TZ)
    # Ask the user for a proleptic TZ string.  Check that it conforms.
    check_POSIX_TZ_string='
      BEGIN {
	tz = substr(ARGV[1], 2)
	ARGV[1] = ""
	tzname = ("(<[[:alnum:]+-][[:alnum:]+-][[:alnum:]+-]+>" \
		  "|[[:alpha:]][[:alpha:]][[:alpha:]]+)")
	sign = "[-+]?"
	hhmm = "(:[0-5][0-9](:[0-5][0-9])?)?"
	offset = sign "(2[0-4]|[0-1]?[0-9])" hhmm
	time = sign "(16[0-7]|(1[0-5]|[0-9]?)[0-9])" hhmm
	mdate = "M([1-9]|1[0-2])\\.[1-5]\\.[0-6]"
	jdate = ("((J[1-9]|[0-9]|J?[1-9][0-9]" \
		 "|J?[1-2][0-9][0-9])|J?3[0-5][0-9]|J?36[0-5])")
	datetime = ",(" mdate "|" jdate ")(/" time ")?"
	tzpattern = ("^(:.*|" tzname offset "(" tzname \
		     "(" offset ")?(" datetime datetime ")?)?)$")
	exit tz ~ tzpattern
      }
    '

    while
      echo >&2 'Please enter the desired value' \
	'of the TZ environment variable.'
      echo >&2 'For example, AEST-10 is abbreviated' \
	'AEST and is 10 hours'
      echo >&2 'ahead (east) of Greenwich,' \
	'with no daylight saving time.'
      read tz
      $AWK "$check_POSIX_TZ_string" ="$tz"
    do
      say >&2 "'$tz' is not a conforming POSIX proleptic TZ string."
    done
    TZ_for_date=$tz;;
  *)
    case $continent in
    coord)
      case $coord in
      '')
	echo >&2 'Please enter coordinates' \
	  'in ISO 6709 notation.'
	echo >&2 'For example, +4042-07403 stands for'
	echo >&2 '40 degrees 42 minutes north,' \
	  '74 degrees 3 minutes west.'
	read coord
      esac
      distance_table=$(
	$AWK \
	  "$output_distances_or_times" \
	  ="$coord" ="$TZ_COUNTRY_TABLE" ="$TZ_ZONE_TABLE" |
	sort -n |
	$AWK "{print} NR == $location_limit { exit }"
      )
      regions=$(
	$AWK '
	  BEGIN {
	    distance_table = substr(ARGV[1], 2)
	    ARGV[1] = ""
	    nlines = split(distance_table, line, /\n/)
	    for (nr = 1; nr <= nlines; nr++) {
	      nf = split(line[nr], f, /\t/)
	      print f[nf]
	    }
	  }
	' ="$distance_table"
      )
      echo >&2 'Please select one of the following timezones,'
      echo >&2 'listed roughly in increasing order' \
	"of distance from $coord".
      doselect $regions
      region=$select_result
      tz=$(
	$AWK '
	  BEGIN {
	    distance_table = substr(ARGV[1], 2)
	    region = substr(ARGV[2], 2)
	    ARGV[1] = ARGV[2] = ""
	    nlines = split(distance_table, line, /\n/)
	    for (nr = 1; nr <= nlines; nr++) {
	      nf = split(line[nr], f, /\t/)
	      if (f[nf] == region)
		print f[4]
	    }
	  }
	' ="$distance_table" ="$region"
      );;
    *)
      case $continent in
      now|time)
	minute_format='%a %b %d %H:%M'
	old_minute=$(TZ=UTC0 date +"$minute_format")
	for i in 1 2 3
	do
	  time_table_command=$(
	    $AWK \
	      -v output_times=1 \
	      "$output_distances_or_times" \
	      = = ="$TZ_ZONE_TABLE"
	  )
	  time_table=$(eval "$time_table_command")
	  new_minute=$(TZ=UTC0 date +"$minute_format")
	  case $old_minute in
	  "$new_minute") break
	  esac
	  old_minute=$new_minute
	done
	echo >&2 "The system says Universal Time is $new_minute."
	echo >&2 "Assuming that's correct, what is the local time?"
	sorted_table=$(say "$time_table" | sort -k2n -k2,5 -k1n) || {
	  say >&2 "$0: cannot sort time table"
	  exit 1
	}
	eval doselect $(
	  $AWK '
	    BEGIN {
	      sorted_table = substr(ARGV[1], 2)
	      ARGV[1] = ""
	      nlines = split(sorted_table, line, /\n/)
	      for (i = 1; i <= nlines; i++) {
		$0 = line[i]
		outline = $6 " " $7 " " $4 " " $5
		if (outline == oldline)
		  continue
		oldline = outline
		gsub(/'\''/, "&\\\\&&", outline)
		printf "'\''%s'\''\n", outline
	      }
	    }
	  ' ="$sorted_table"
	)
	time=$select_result
	continent_re='^'
	zone_table=$(
	  $AWK '
	    BEGIN {
	      time = substr(ARGV[1], 2)
	      time_table = substr(ARGV[2], 2)
	      ARGV[1] = ARGV[2] = ""
	      nlines = split(time_table, line, /\n/)
	      for (i = 1; i <= nlines; i++) {
		$0 = line[i]
		if ($6 " " $7 " " $4 " " $5 == time) {
		  sub(/[^\t]*\t/, "")
		  print
		}
	      }
	    }
	  ' ="$time" ="$time_table"
	)
	countries=$(
	  $AWK \
	    "$output_country_list" \
	    ="$continent_re" ="$TZ_COUNTRY_TABLE" ="$zone_table" |
	  sort -f
	)
	;;
      *)
	continent_re="^$continent/"
	zone_table=$TZ_ZONE_TABLE
      esac

      # Get list of names of countries in the continent or ocean.
      countries=$(
	$AWK \
	  "$output_country_list" \
	  ="$continent_re" ="$TZ_COUNTRY_TABLE" ="$zone_table" |
	sort -f
      )
      # If all zone table entries have comments, and there are
      # at most 22 entries, asked based on those comments.
      # This fits the prompt onto old-fashioned 24-line screens.
      regions=$(
	$AWK '
	  BEGIN {
	    TZ_ZONE_TABLE = substr(ARGV[1], 2)
	    ARGV[1] = ""
	    FS = "\t"
	    nlines = split(TZ_ZONE_TABLE, line, /\n/)
	    for (i = 1; i <= nlines; i++) {
	      $0 = line[i]
	      if ($0 ~ /^[^#]/ && !missing_comment) {
		if ($4)
		  comment[++inlines] = $4
		else
		  missing_comment = 1
	      }
	    }
	    if (!missing_comment && inlines <= 22)
	      for (i = 1; i <= inlines; i++)
		print comment[i]
	  }
	' ="$zone_table"
      )

      # If there's more than one country, ask the user which one.
      case $countries in
      *"$newline"*)
	echo >&2 'Please select a country' \
	  'whose clocks agree with yours.'
	doselect $countries
	country_result=$select_result
	country=$select_result;;
      *)
	country=$countries
      esac


      # Get list of timezones in the country.
      regions=$(
	$AWK '
	  BEGIN {
	    country = substr(ARGV[1], 2)
	    TZ_COUNTRY_TABLE = substr(ARGV[2], 2)
	    TZ_ZONE_TABLE = substr(ARGV[3], 2)
	    ARGV[1] = ARGV[2] = ARGV[3] = ""
	    FS = "\t"
	    cc = country
	    nlines = split(TZ_COUNTRY_TABLE, line, /\n/)
	    for (i = 1; i <= nlines; i++) {
	      $0 = line[i]
	      if ($0 !~ /^#/  &&  country == $2) {
		cc = $1
		break
	      }
	    }
	    nlines = split(TZ_ZONE_TABLE, line, /\n/)
	    for (i = 1; i <= nlines; i++) {
	      $0 = line[i]
	      if ($0 ~ /^#/)
		continue
	      if ($1 ~ cc)
		print $4
	    }
	  }
	' ="$country" ="$TZ_COUNTRY_TABLE" ="$zone_table"
      )

      # If there's more than one region, ask the user which one.
      case $regions in
      *"$newline"*)
	echo >&2 'Please select one of the following timezones.'
	doselect $regions
	region=$select_result
      esac

      # Determine tz from country and region.
      tz=$(
	$AWK '
	  BEGIN {
	    country = substr(ARGV[1], 2)
	    region = substr(ARGV[2], 2)
	    TZ_COUNTRY_TABLE = substr(ARGV[3], 2)
	    TZ_ZONE_TABLE = substr(ARGV[4], 2)
	    ARGV[1] = ARGV[2] = ARGV[3] = ARGV[4] = ""
	    FS = "\t"
	    cc = country
	    nlines = split(TZ_COUNTRY_TABLE, line, /\n/)
	    for (i = 1; i <= nlines; i++) {
	      $0 = line[i]
	      if ($0 !~ /^#/  &&  country == $2) {
		cc = $1
		break
	      }
	    }
	    nlines = split(TZ_ZONE_TABLE, line, /\n/)
	    for (i = 1; i <= nlines; i++) {
	      $0 = line[i]
	      if ($0 ~ /^#/)
		continue
	      if ($1 ~ cc && ($4 == region || !region))
		print $3
	    }
	  }
	' ="$country" ="$region" ="$TZ_COUNTRY_TABLE" ="$zone_table"
      )
    esac

    # Make sure the corresponding zoneinfo file exists.
    TZ_for_date=$TZDIR/$tz
    <"$TZ_for_date" || {
      say >&2 "$0: time zone files are not set up correctly"
      exit 1
    }
  esac


  # Use the proposed TZ to output the current date relative to UTC.
  # Loop until they agree in seconds.
  # Give up after 8 unsuccessful tries.

  extra_info=
  for i in 1 2 3 4 5 6 7 8
  do
    TZdate=$(LANG=C TZ="$TZ_for_date" date)
    UTdate=$(LANG=C TZ=UTC0 date)
    TZsecsetc=${TZdate##*[0-5][0-9]:}
    UTsecsetc=${UTdate##*[0-5][0-9]:}
    if test "${TZsecsetc%%[!0-9]*}" = "${UTsecsetc%%[!0-9]*}"
    then
      extra_info="
Selected time is now:	$TZdate.
Universal Time is now:	$UTdate."
      break
    fi
  done


  # Output TZ info and ask the user to confirm.

  echo >&2 ""
  echo >&2 "Based on the following information:"
  echo >&2 ""
  case $time%$country_result%$region%$coord in
  ?*%?*%?*%)
    say >&2 "	$time$newline	$country_result$newline	$region";;
  ?*%?*%%|?*%%?*%) say >&2 "	$time$newline	$country_result$region";;
  ?*%%%)	say >&2 "	$time";;
  %?*%?*%)	say >&2 "	$country_result$newline	$region";;
  %?*%%)	say >&2 "	$country_result";;
  %%?*%?*)	say >&2 "	coord $coord$newline	$region";;
  %%%?*)	say >&2 "	coord $coord";;
  *)		say >&2 "	TZ='$tz'"
  esac
  say >&2 ""
  say >&2 "TZ='$tz' will be used.$extra_info"
  say >&2 "Is the above information OK?"

  doselect Yes No
  ok=$select_result
  case $ok in
  Yes) break
  esac
do coord=
done

case $SHELL in
*csh) file=.login line="setenv TZ '$tz'";;
*)    file=.profile line="export TZ='$tz'"
esac

test -t 1 && say >&2 "
You can make this change permanent for yourself by appending the line
	$line
to the file '$file' in your home directory; then log out and log in again.

Here is that TZ value again, this time on standard output so that you
can use the $0 command in shell scripts:"

say "$tz"
