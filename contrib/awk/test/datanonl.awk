# example program from alex@bofh.torun.pl
BEGIN { IGNORECASE=1 }
/\w+@([[:alnum:]]+\.)+[[:alnum:]]+[[:blank:]]+/ {print $0}
