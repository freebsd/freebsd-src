#! /bin/sh

. ./tests.sh

export QUIET_TEST=1

export VALGRIND=
VGCODE=126

tot_tests=0
tot_pass=0
tot_fail=0
tot_config=0
tot_vg=0
tot_strange=0

base_run_test() {
    tot_tests=$((tot_tests + 1))
    if VALGRIND="$VALGRIND" "$@"; then
	tot_pass=$((tot_pass + 1))
    else
	ret="$?"
	if [ "$ret" == "1" ]; then
	    tot_config=$((tot_config + 1))
	elif [ "$ret" == "2" ]; then
	    tot_fail=$((tot_fail + 1))
	elif [ "$ret" == "$VGCODE" ]; then
	    tot_vg=$((tot_vg + 1))
	else
	    tot_strange=$((tot_strange + 1))
	fi
    fi
}

run_test () {
    echo -n "$@:	"
    if [ -n "$VALGRIND" -a -f $1.supp ]; then
	VGSUPP="--suppressions=$1.supp"
    fi
    base_run_test $VALGRIND $VGSUPP "./$@"
}

run_sh_test () {
    echo -n "$@:	"
    base_run_test sh "$@"
}

wrap_test () {
    (
	if verbose_run "$@"; then
	    PASS
	else
	    ret="$?"
	    if [ "$ret" -gt 127 ]; then
		signame=$(kill -l $((ret - 128)))
		FAIL "Killed by SIG$signame"
	    else
		FAIL "Returned error code $ret"
	    fi
	fi
    )
}

run_wrap_test () {
    echo -n "$@:	"
    base_run_test wrap_test "$@"
}

run_dtc_test () {
    echo -n "dtc $@:	"
    base_run_test wrap_test $VALGRIND $DTC "$@"
}

CONVERT=../convert-dtsv0

run_convert_test () {
    echo -n "convert-dtsv0 $@:	"
    base_run_test wrap_test $VALGRIND $CONVERT "$@"
}

tree1_tests () {
    TREE=$1

    # Read-only tests
    run_test get_mem_rsv $TREE
    run_test root_node $TREE
    run_test find_property $TREE
    run_test subnode_offset $TREE
    run_test path_offset $TREE
    run_test get_name $TREE
    run_test getprop $TREE
    run_test get_phandle $TREE
    run_test get_path $TREE
    run_test supernode_atdepth_offset $TREE
    run_test parent_offset $TREE
    run_test node_offset_by_prop_value $TREE
    run_test node_offset_by_phandle $TREE
    run_test node_check_compatible $TREE
    run_test node_offset_by_compatible $TREE
    run_test notfound $TREE

    # Write-in-place tests
    run_test setprop_inplace $TREE
    run_test nop_property $TREE
    run_test nop_node $TREE
}

tree1_tests_rw () {
    TREE=$1

    # Read-write tests
    run_test set_name $TREE
    run_test setprop $TREE
    run_test del_property $TREE
    run_test del_node $TREE
}

check_tests () {
    tree="$1"
    shift
    run_sh_test dtc-checkfails.sh "$@" -- -I dts -O dtb $tree
    run_dtc_test -I dts -O dtb -o $tree.test.dtb -f $tree
    run_sh_test dtc-checkfails.sh "$@" -- -I dtb -O dtb $tree.test.dtb
}

ALL_LAYOUTS="mts mst tms tsm smt stm"

libfdt_tests () {
    tree1_tests test_tree1.dtb

    # Sequential write tests
    run_test sw_tree1
    tree1_tests sw_tree1.test.dtb
    tree1_tests unfinished_tree1.test.dtb
    run_test dtbs_equal_ordered test_tree1.dtb sw_tree1.test.dtb

    # fdt_move tests
    for tree in test_tree1.dtb sw_tree1.test.dtb unfinished_tree1.test.dtb; do
	rm -f moved.$tree shunted.$tree deshunted.$tree
	run_test move_and_save $tree
	run_test dtbs_equal_ordered $tree moved.$tree
	run_test dtbs_equal_ordered $tree shunted.$tree
	run_test dtbs_equal_ordered $tree deshunted.$tree
    done

    # v16 and alternate layout tests
    for tree in test_tree1.dtb; do
	for version in 17 16; do
	    for layout in $ALL_LAYOUTS; do
		run_test mangle-layout $tree $version $layout
		tree1_tests v$version.$layout.$tree
		run_test dtbs_equal_ordered $tree v$version.$layout.$tree
	    done
	done
    done

    # Read-write tests
    for basetree in test_tree1.dtb; do
	for version in 17 16; do
	    for layout in $ALL_LAYOUTS; do
		tree=v$version.$layout.$basetree
		rm -f opened.$tree repacked.$tree
		run_test open_pack $tree
		tree1_tests opened.$tree
		tree1_tests repacked.$tree

		tree1_tests_rw $tree
		tree1_tests_rw opened.$tree
		tree1_tests_rw repacked.$tree
	    done
	done
    done
    run_test rw_tree1
    tree1_tests rw_tree1.test.dtb
    tree1_tests_rw rw_tree1.test.dtb

    for basetree in test_tree1.dtb sw_tree1.test.dtb rw_tree1.test.dtb; do
	run_test nopulate $basetree
	run_test dtbs_equal_ordered $basetree noppy.$basetree
	tree1_tests noppy.$basetree
	tree1_tests_rw noppy.$basetree
    done

    # Tests for behaviour on various sorts of corrupted trees
    run_test truncated_property

    # Specific bug tests
    run_test add_subnode_with_nops
}

dtc_tests () {
    run_dtc_test -I dts -O dtb -o dtc_tree1.test.dtb test_tree1.dts
    tree1_tests dtc_tree1.test.dtb
    tree1_tests_rw dtc_tree1.test.dtb
    run_test dtbs_equal_ordered dtc_tree1.test.dtb test_tree1.dtb

    run_dtc_test -I dts -O dtb -o dtc_tree1_dts0.test.dtb test_tree1_dts0.dts
    tree1_tests dtc_tree1_dts0.test.dtb
    tree1_tests_rw dtc_tree1_dts0.test.dtb

    run_dtc_test -I dts -O dtb -o dtc_escapes.test.dtb escapes.dts
    run_test string_escapes dtc_escapes.test.dtb

    run_dtc_test -I dts -O dtb -o dtc_references.test.dtb references.dts
    run_test references dtc_references.test.dtb

    run_dtc_test -I dts -O dtb -o dtc_references_dts0.test.dtb references_dts0.dts
    run_test references dtc_references_dts0.test.dtb

    run_dtc_test -I dts -O dtb -o dtc_path-references.test.dtb path-references.dts
    run_test path-references dtc_path-references.test.dtb

    run_dtc_test -I dts -O dtb -o dtc_comments.test.dtb comments.dts
    run_dtc_test -I dts -O dtb -o dtc_comments-cmp.test.dtb comments-cmp.dts
    run_test dtbs_equal_ordered dtc_comments.test.dtb dtc_comments-cmp.test.dtb

    # Check /include/ directive
    run_dtc_test -I dts -O dtb -o includes.test.dtb include0.dts
    run_test dtbs_equal_ordered includes.test.dtb test_tree1.dtb

    # Check /incbin/ directive
    run_dtc_test -I dts -O dtb -o incbin.test.dtb incbin.dts
    run_test incbin incbin.test.dtb

    # Check boot_cpuid_phys handling
    run_dtc_test -I dts -O dtb -b 17 -o boot_cpuid.test.dtb empty.dts
    run_test boot-cpuid boot_cpuid.test.dtb 17
    run_dtc_test -I dtb -O dtb -b 17 -o boot_cpuid_test_tree1.test.dtb test_tree1.dtb
    run_test boot-cpuid boot_cpuid_test_tree1.test.dtb 17
    run_dtc_test -I dtb -O dtb -o boot_cpuid_preserved_test_tree1.test.dtb boot_cpuid_test_tree1.test.dtb
    run_test dtbs_equal_ordered boot_cpuid_preserved_test_tree1.test.dtb boot_cpuid_test_tree1.test.dtb

    # Check -Odts mode preserve all dtb information
    for tree in test_tree1.dtb dtc_tree1.test.dtb dtc_escapes.test.dtb ; do
	run_dtc_test -I dtb -O dts -o odts_$tree.test.dts $tree
	run_dtc_test -I dts -O dtb -o odts_$tree.test.dtb odts_$tree.test.dts
	run_test dtbs_equal_ordered $tree odts_$tree.test.dtb
    done

    # Check version conversions
    for tree in test_tree1.dtb ; do
	 for aver in 1 2 3 16 17; do
	     atree="ov${aver}_$tree.test.dtb"
	     run_dtc_test -I dtb -O dtb -V$aver -o $atree $tree
	     for bver in 16 17; do
		 btree="ov${bver}_$atree"
		 run_dtc_test -I dtb -O dtb -V$bver -o $btree $atree
		 run_test dtbs_equal_ordered $btree $tree
	     done
	 done
    done

    # Check some checks
    check_tests dup-nodename.dts duplicate_node_names
    check_tests dup-propname.dts duplicate_property_names
    check_tests dup-phandle.dts explicit_phandles
    check_tests zero-phandle.dts explicit_phandles
    check_tests minusone-phandle.dts explicit_phandles
    run_sh_test dtc-checkfails.sh phandle_references -- -I dts -O dtb nonexist-node-ref.dts
    run_sh_test dtc-checkfails.sh phandle_references -- -I dts -O dtb nonexist-label-ref.dts
    check_tests bad-name-property.dts name_properties

    check_tests bad-ncells.dts address_cells_is_cell size_cells_is_cell interrupt_cells_is_cell
    check_tests bad-string-props.dts device_type_is_string model_is_string status_is_string
    check_tests bad-reg-ranges.dts reg_format ranges_format
    check_tests bad-empty-ranges.dts ranges_format
    check_tests reg-ranges-root.dts reg_format ranges_format
    check_tests default-addr-size.dts avoid_default_addr_size
    check_tests obsolete-chosen-interrupt-controller.dts obsolete_chosen_interrupt_controller
    run_sh_test dtc-checkfails.sh node_name_chars -- -I dtb -O dtb bad_node_char.dtb
    run_sh_test dtc-checkfails.sh node_name_format -- -I dtb -O dtb bad_node_format.dtb
    run_sh_test dtc-checkfails.sh prop_name_chars -- -I dtb -O dtb bad_prop_char.dtb

    # Check for proper behaviour reading from stdin
    run_dtc_test -I dts -O dtb -o stdin_dtc_tree1.test.dtb - < test_tree1.dts
    run_wrap_test cmp stdin_dtc_tree1.test.dtb dtc_tree1.test.dtb
    run_dtc_test -I dtb -O dts -o stdin_odts_test_tree1.dtb.test.dts - < test_tree1.dtb
    run_wrap_test cmp stdin_odts_test_tree1.dtb.test.dts odts_test_tree1.dtb.test.dts

    # Check for graceful failure in some error conditions
    run_sh_test dtc-fatal.sh -I dts -O dtb nosuchfile.dts
    run_sh_test dtc-fatal.sh -I dtb -O dtb nosuchfile.dtb
    run_sh_test dtc-fatal.sh -I fs -O dtb nosuchfile
}

convert_tests () {
    V0_DTS="test_tree1_dts0.dts references_dts0.dts empty.dts escapes.dts \
	test01.dts label01.dts"
    for dts in $V0_DTS; do
	run_dtc_test -I dts -O dtb -o cvtraw_$dts.test.dtb $dts
	run_dtc_test -I dts -O dts -o cvtdtc_$dts.test.dts $dts
	run_dtc_test -I dts -O dtb -o cvtdtc_$dts.test.dtb cvtdtc_$dts.test.dts
	run_convert_test $dts
	run_dtc_test -I dts -O dtb -o cvtcvt_$dts.test.dtb ${dts}v1

	run_wrap_test cmp cvtraw_$dts.test.dtb cvtdtc_$dts.test.dtb
	run_wrap_test cmp cvtraw_$dts.test.dtb cvtcvt_$dts.test.dtb
    done
}

while getopts "vt:m" ARG ; do
    case $ARG in
	"v")
	    unset QUIET_TEST
	    ;;
	"t")
	    TESTSETS=$OPTARG
	    ;;
	"m")
	    VALGRIND="valgrind --tool=memcheck -q --error-exitcode=$VGCODE"
	    ;;
    esac
done

if [ -z "$TESTSETS" ]; then
    TESTSETS="libfdt dtc convert"
fi

# Make sure we don't have stale blobs lying around
rm -f *.test.dtb *.test.dts

for set in $TESTSETS; do
    case $set in
	"libfdt")
	    libfdt_tests
	    ;;
	"dtc")
	    dtc_tests
	    ;;
	"convert")
	    convert_tests
	    ;;
    esac
done

echo -e "********** TEST SUMMARY"
echo -e "*     Total testcases:	$tot_tests"
echo -e "*                PASS:	$tot_pass"
echo -e "*                FAIL:	$tot_fail"
echo -e "*   Bad configuration:	$tot_config"
if [ -n "$VALGRIND" ]; then
    echo -e "*    valgrind errors:	$tot_vg"
fi
echo -e "* Strange test result:	$tot_strange"
echo -e "**********"

