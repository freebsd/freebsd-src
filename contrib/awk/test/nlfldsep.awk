BEGIN { RS = "A" }
{print NF; for (i = 1; i <= NF; i++) print $i ; print ""}
