#!/bin/sh
#
# $Id: func.sh 2378 2012-01-03 08:59:56Z jkoshy $

test_format_bsd1()
{
    # $1 test file
    # $2 oracle file

    run "-t d -B" $1 $2
}

test_format_bsd2()
{
    # $1 test file
    # $2 oracle file

    run "-t d --format=bsd" $1 $2
}

test_dynamic1()
{
    # $1 test file
    # $2 oracle file

    run "-t d -D" $1 $2
}

test_dynamic2()
{
    # $1 test file
    # $2 oracle file

    run "-t d --dynamic" $1 $2
}

test_external()
{
    # $1 test file
    # $2 oracle file

    run "-t d -g" $1 $2
}

test_hexa1()
{
    # $1 test file
    # $2 oracle file

    run "-x" $1 $2
}

test_hexa2()
{
    # $1 test file
    # $2 oracle file

    run "-t x" $1 $2
}

test_hexa3()
{
    # $1 test file
    # $2 oracle file

    run "--radix=x" $1 $2
}

test_no_sort1()
{
    # $1 test file
    # $2 oracle file

    run "-t d -p" $1 $2
}

test_no_sort2()
{
    # $1 test file
    # $2 oracle file

    run "-t d --no-sort" $1 $2
}

test_num_sort1()
{
    # $1 test file
    # $2 oracle file

    run "-t d -n" $1 $2
}

test_num_sort2()
{
    # $1 test file
    # $2 oracle file

    run "-t d --numeric-sort" $1 $2
}

test_octal2()
{
    # $1 test file
    # $2 oracle file

    run "-t o" $1 $2
}

test_octal3()
{
    # $1 test file
    # $2 oracle file

    run "--radix=o" $1 $2
}

test_posix1()
{
    # $1 test file
    # $2 oracle file

    run "-P" $1 $2
}

test_posix2()
{
    # $1 test file
    # $2 oracle file

    run "--format=posix" $1 $2
}

test_print_file_name1()
{
    # $1 test file
    # $2 oracle file

    run "-t d -A" $1 $2
}

test_print_file_name2()
{
    # $1 test file
    # $2 oracle file

    run "-t d --print-file-name" $1 $2
}

test_print_size1()
{
    # $1 test file
    # $2 oracle file

    run "-t d -S" $1 $2
}

test_print_size2()
{
    # $1 test file
    # $2 oracle file

    run "-t d --print-size" $1 $2
}

test_reverse_sort1()
{
    # $1 test file
    # $2 oracle file

    run "-t d -r" $1 $2
}

test_reverse_sort2()
{
    # $1 test file
    # $2 oracle file

    run "-t d --reverse-sort" $1 $2
}

test_reverse_sort_num()
{
    # $1 test file
    # $2 oracle file

    run "-t d -r -n" $1 $2
}

test_reverse_sort_no()
{
    # $1 test file
    # $2 oracle file

    run "-t d -r -p" $1 $2
}

test_reverse_sort_size()
{
    # $1 test file
    # $2 oracle file

    run "-t d -r --size-sort" $1 $2
}

test_size_sort()
{
    # $1 test file
    # $2 oracle file

    run "-t d --size-sort" $1 $2
}

test_sysv()
{
    # $1 test file
    # $2 oracle file

    run "-t d --format=sysv" $1 $2
}

test_undef1()
{
    # $1 test file
    # $2 oracle file

    run "-t d -u" $1 $2
}

test_undef2()
{
    # $1 test file
    # $2 oracle file

    run "-t d --undefined-only" $1 $2
}

test_debug_syms1()
{
    # $1 test file
    # $2 oracle file

    run "-a" $1 $2
}

test_debug_syms2()
{
    # $1 test file
    # $2 oracle file

    run "--debug-syms" $1 $2
}

run()
{
    # $1 nm option
    # $2 test file
    # $3 oracle file

    tet_infoline "OPTION $1"

    NM_PATH="$TET_SUITE_ROOT/../../nm/nm"
    TEST_OUTPUT_FILE="test.out"

    $NM_PATH $1 $2 > $TEST_OUTPUT_FILE 2> /dev/null
    NM_RETURN_CODE="$?"
    if [ $NM_RETURN_CODE -ne "0" ]; then
        tet_infoline "nm execution failed"
        tet_result FAIL

        return
    fi

    diff $TEST_OUTPUT_FILE $3 > /dev/null
    DIFF_RETURN_CODE="$?"
    if [ $DIFF_RETURN_CODE -ne "0" ]; then
        tet_infoline "diff failed"
        tet_result FAIL

        return
    fi

    tet_result PASS
}

run_without_diff()
{
    # $1 nm option
    # $2 oracle return code

    tet_infoline "OPTION $1"

    NM_PATH="$TET_SUITE_ROOT/../../nm/nm"

    $NM_PATH $1 > /dev/null 2> /dev/null
    NM_RETURN_CODE="$?"
    if [ $NM_RETURN_CODE -ne $2 ]; then
        tet_infoline "nm execution failed"
        tet_result FAIL

        return
    fi

    tet_result PASS
}
