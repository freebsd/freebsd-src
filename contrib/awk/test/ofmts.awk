BEGIN { OFMT= "%s" }
{ $1 + $2; print $1, $2 }
