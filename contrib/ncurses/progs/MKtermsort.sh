#!/bin/sh
#
# MKtermsort.sh -- generate indirection vectors for the various sort methods
#
# The output of this script is C source for nine arrays that list three sort
# orders for each of the three different classes of terminfo capabilities.
#
AWK=${1-awk}
DATA=${2-../include/Caps}

echo "/*";
echo " * termsort.c --- sort order arrays for use by infocmp.";
echo " *";
echo " * Note: this file is generated using termsort.sh, do not edit by hand.";
echo " */";

echo "static const int bool_terminfo_sort[] = {";
$AWK <$DATA '
BEGIN           {i = 0;}
/^#/            {next;}
$3 == "bool"    {printf("%s\t%d\n", $2, i++);}
' | sort | $AWK '{print "\t", $2, ",\t/* ", $1, " */";}';
echo "};";
echo "";

echo "static const int num_terminfo_sort[] = {";
$AWK <$DATA '
BEGIN           {i = 0;}
/^#/            {next;}
$3 == "num"     {printf("%s\t%d\n", $2, i++);}
' | sort | $AWK '{print "\t", $2, ",\t/* ", $1, " */";}';
echo "};";
echo "";

echo "static const int str_terminfo_sort[] = {";
$AWK <$DATA '
BEGIN           {i = 0;}
/^#/            {next;}
$3 == "str"     {printf("%s\t%d\n", $2, i++);}
' | sort | $AWK '{print "\t", $2, ",\t/* ", $1, " */";}';
echo "};";
echo "";

echo "static const int bool_variable_sort[] = {";
$AWK <$DATA '
BEGIN           {i = 0;}
/^#/            {next;}
$3 == "bool"    {printf("%s\t%d\n", $1, i++);}
' | sort | $AWK '{print "\t", $2, ",\t/* ", $1, " */";}';
echo "};";
echo "";

echo "static const int num_variable_sort[] = {";
$AWK <$DATA '
BEGIN           {i = 0;}
/^#/            {next;}
$3 == "num"     {printf("%s\t%d\n", $1, i++);}
' | sort | $AWK '{print "\t", $2, ",\t/* ", $1, " */";}';
echo "};";
echo "";

echo "static const int str_variable_sort[] = {";
$AWK <$DATA '
BEGIN           {i = 0;}
/^#/            {next;}
$3 == "str"     {printf("%s\t%d\n", $1, i++);}
' | sort | $AWK '{print "\t", $2, ",\t/* ", $1, " */";}';
echo "};";
echo "";

echo "static const int bool_termcap_sort[] = {";
$AWK <$DATA '
BEGIN           {i = 0;}
/^#/            {next;}
$3 == "bool"    {printf("%s\t%d\n", $4, i++);}
' | sort | $AWK '{print "\t", $2, ",\t/* ", $1, " */";}';
echo "};";
echo "";

echo "static const int num_termcap_sort[] = {";
$AWK <$DATA '
BEGIN           {i = 0;}
/^#/            {next;}
$3 == "num"     {printf("%s\t%d\n", $4, i++);}
' | sort | $AWK '{print "\t", $2, ",\t/* ", $1, " */";}';
echo "};";
echo "";

echo "static const int str_termcap_sort[] = {";
$AWK <$DATA '
BEGIN           {i = 0;}
/^#/            {next;}
$3 == "str"     {printf("%s\t%d\n", $4, i++);}
' | sort | $AWK '{print "\t", $2, ",\t/* ", $1, " */";}';
echo "};";
echo "";

echo "static const int bool_from_termcap[] = {";
$AWK <$DATA '
$3 == "bool" && substr($5, 1, 1) == "-"       {print "0,\t/* ", $2, " */";}
$3 == "bool" && substr($5, 1, 1) == "Y"       {print "1,\t/* ", $2, " */";}
'
echo "};";
echo "";

echo "static const int num_from_termcap[] = {";
$AWK <$DATA '
$3 == "num" && substr($5, 1, 1) == "-"        {print "0,\t/* ", $2, " */";}
$3 == "num" && substr($5, 1, 1) == "Y"        {print "1,\t/* ", $2, " */";}
'
echo "};";
echo "";

echo "static const int str_from_termcap[] = {";
$AWK <$DATA '
$3 == "str" && substr($5, 1, 1) == "-"        {print "0,\t/* ", $2, " */";}
$3 == "str" && substr($5, 1, 1) == "Y"        {print "1,\t/* ", $2, " */";}
'
echo "};";
echo "";

