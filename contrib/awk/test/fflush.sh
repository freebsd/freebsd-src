#! /bin/sh
../gawk 'BEGIN{print "1st";fflush("/dev/stdout");print "2nd"|"cat"}'

../gawk 'BEGIN{print "1st";fflush("/dev/stdout");print "2nd"|"cat"}'|cat

../gawk 'BEGIN{print "1st";fflush("/dev/stdout");close("/dev/stdout");print "2nd"|"cat"}'|cat

../gawk 'BEGIN{print "1st";fflush("/dev/stdout");print "2nd"|"cat";close("cat")}'|cat

../gawk 'BEGIN{print "1st";fflush("/dev/stdout");print "2nd"|"cat";close("cat")}'|cat

../gawk 'BEGIN{print "1st";fflush("/dev/stdout");print "2nd"|"cat";close("cat")}'|cat

../gawk 'BEGIN{print "1st";fflush("/dev/stdout");print "2nd"|"sort"}'|cat

../gawk 'BEGIN{print "1st";fflush("/dev/stdout");print "2nd"|"sort";close("sort")}'|cat
