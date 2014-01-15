#!/bin/sh
#
# $Id: tc.sh 2378 2012-01-03 08:59:56Z jkoshy $

tp1()
{
    test_format_bsd1 $TEST_FILE "$TEST_FILE-format-bsd.txt"
}

tp2()
{
    test_format_bsd2 $TEST_FILE "$TEST_FILE-format-bsd.txt"
}

tp3()
{
    test_dynamic1 $TEST_FILE "$TEST_FILE-dynamic.txt"
}

tp4()
{
    test_dynamic2 $TEST_FILE "$TEST_FILE-dynamic.txt"
}

tp5()
{
    test_external $TEST_FILE "$TEST_FILE-external.txt"
}

tp6()
{
    test_hexa1 $TEST_FILE "$TEST_FILE-radix-hexa.txt"
}

tp7()
{
    test_hexa2 $TEST_FILE "$TEST_FILE-radix-hexa.txt"
}

tp8()
{
    test_hexa3 $TEST_FILE "$TEST_FILE-radix-hexa.txt"
}

tp9()
{
    test_no_sort1 $TEST_FILE "$TEST_FILE-sort-no.txt"
}

tp10()
{
    test_no_sort2 $TEST_FILE "$TEST_FILE-sort-no.txt"
}

tp11()
{
    test_num_sort1 $TEST_FILE "$TEST_FILE-sort-num.txt"
}

tp12()
{
    test_num_sort2 $TEST_FILE "$TEST_FILE-sort-num.txt"
}

tp14()
{
    test_octal2 $TEST_FILE "$TEST_FILE-radix-octal.txt"
}

tp15()
{
    test_octal3 $TEST_FILE "$TEST_FILE-radix-octal.txt"
}

tp16()
{
    test_posix1 $TEST_FILE "$TEST_FILE-format-posix.txt"
}

tp17()
{
    test_posix2 $TEST_FILE "$TEST_FILE-format-posix.txt"
}

tp18()
{
    test_print_file_name1 $TEST_FILE "$TEST_FILE-print-file-name.txt"
}

tp19()
{
    test_print_file_name2 $TEST_FILE "$TEST_FILE-print-file-name.txt"
}

tp20()
{
    test_print_size1 $TEST_FILE "$TEST_FILE-print-size.txt"
}

tp21()
{
    test_print_size2 $TEST_FILE "$TEST_FILE-print-size.txt"
}

tp22()
{
    test_reverse_sort1 $TEST_FILE "$TEST_FILE-sort-reverse.txt"
}

tp23()
{
    test_reverse_sort2 $TEST_FILE "$TEST_FILE-sort-reverse.txt"
}

tp24()
{
    test_reverse_sort_num $TEST_FILE "$TEST_FILE-sort-reverse-num.txt"
}

tp25()
{
    test_reverse_sort_no $TEST_FILE "$TEST_FILE-sort-reverse-no.txt"
}

tp26()
{
    test_reverse_sort_size $TEST_FILE "$TEST_FILE-sort-reverse-size.txt"
}

tp27()
{
    test_size_sort $TEST_FILE "$TEST_FILE-sort-size.txt"
}

tp28()
{
    test_sysv $TEST_FILE "$TEST_FILE-format-sysv.txt"
}

tp29()
{
    test_undef1 $TEST_FILE "$TEST_FILE-undef.txt"
}

tp30()
{
    test_undef2 $TEST_FILE "$TEST_FILE-undef.txt"
}

startup()
{
    uudecode "$TEST_FILE.uu"
}

cleanup()
{
    rm -f $TEST_FILE
}

TEST_FILE="test_ko"

tet_startup="startup"
tet_cleanup="cleanup"

iclist="ic1 ic2 ic3 ic4 ic5 ic6 ic7 ic8 ic9 ic10 ic11 ic12 ic14 ic15 ic16 ic17 ic18 ic19 ic20 ic21 ic22 ic23 ic24 ic25 ic26 ic27 ic28 ic29 ic30"

ic1="tp1"
ic2="tp2"
ic3="tp3"
ic4="tp4"
ic5="tp5"
ic6="tp6"
ic7="tp7"
ic8="tp8"
ic9="tp9"
ic10="tp10"
ic11="tp11"
ic12="tp12"
ic14="tp14"
ic15="tp15"
ic16="tp16"
ic17="tp17"
ic18="tp18"
ic19="tp19"
ic20="tp20"
ic21="tp21"
ic22="tp22"
ic23="tp23"
ic24="tp24"
ic25="tp25"
ic26="tp26"
ic27="tp27"
ic28="tp28"
ic29="tp29"
ic30="tp30"

. $TET_SUITE_ROOT/ts/common/func.sh
. $TET_ROOT/lib/xpg3sh/tcm.sh
