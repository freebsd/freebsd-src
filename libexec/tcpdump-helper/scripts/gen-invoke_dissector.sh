grep DISSECTOR_DEC contrib/tcpdump/interface.h  | cut -d\( -f2 | awk  '{ print "\telse if (func == (void *)_" $1 ")\n" "\t\top = TCPDUMP_HELPER_OP_" toupper($1) ";"}'
