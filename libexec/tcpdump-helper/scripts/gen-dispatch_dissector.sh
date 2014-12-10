grep ^DISSECTOR_DEC contrib/tcpdump/interface.h  | cut -d\( -f2 | awk '{ print "\tcase TCPDUMP_HELPER_OP_" toupper($1) ":\n" "\t\t_" $1 "(bp, lengthXXX);\n\t\tbreak;\n"}'
