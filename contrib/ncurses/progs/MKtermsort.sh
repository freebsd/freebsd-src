#!/bin/sh
# $Id: MKtermsort.sh,v 1.7 2001/05/26 23:37:57 tom Exp $
#
# MKtermsort.sh -- generate indirection vectors for the various sort methods
#
# The output of this script is C source for nine arrays that list three sort
# orders for each of the three different classes of terminfo capabilities.
#
# keep the order independent of locale:
LANGUAGE=C
LC_ALL=C
export LANGUAGE
export LC_ALL
#
AWK=${1-awk}
DATA=${2-../include/Caps}

data=data$$
trap 'rm -f $data' 1 2 5 15
sed -e 's/[	]\+/	/g' < $DATA >$data
DATA=$data

echo "/*";
echo " * termsort.c --- sort order arrays for use by infocmp.";
echo " *";
echo " * Note: this file is generated using MKtermsort.sh, do not edit by hand.";
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

echo "static const bool bool_from_termcap[] = {";
$AWK <$DATA '
$3 == "bool" && substr($7, 1, 1) == "-"       {print "\tFALSE,\t/* ", $2, " */";}
$3 == "bool" && substr($7, 1, 1) == "Y"       {print "\tTRUE,\t/* ", $2, " */";}
'
echo "};";
echo "";

echo "static const bool num_from_termcap[] = {";
$AWK <$DATA '
$3 == "num" && substr($7, 1, 1) == "-"        {print "\tFALSE,\t/* ", $2, " */";}
$3 == "num" && substr($7, 1, 1) == "Y"        {print "\tTRUE,\t/* ", $2, " */";}
'
echo "};";
echo "";

echo "static const bool str_from_termcap[] = {";
$AWK <$DATA '
$3 == "str" && substr($7, 1, 1) == "-"        {print "\tFALSE,\t/* ", $2, " */";}
$3 == "str" && substr($7, 1, 1) == "Y"        {print "\tTRUE,\t/* ", $2, " */";}
'
echo "};";
echo "";

rm -f $data
