# $Id: ld_script.awk 2493 2012-04-15 18:59:09Z kaiwang27 $

BEGIN {
    split(ARGV[1], s, ".");
    printf "const char *%s = ", s[1];
}

{
    printf "\"";
    gsub("\"", "\\\"");
    printf "%s\\n\"\n", $0;
}

END {
    print ";";
}
