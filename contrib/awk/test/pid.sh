#! /bin/sh
AWK=${AWK-../gawk}
echo $$ > _pid.in
echo $1 >> _pid.in
exec $AWK -f pid.awk _pid.in
