BEGIN { RS = "" }
{ printf("<%s>\n", $0) }
