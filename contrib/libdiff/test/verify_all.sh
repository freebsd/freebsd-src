#!/bin/sh

diff_prog="../diff/obj/diff"
if [ ! -x $diff_prog ]; then
	diff_prog="../diff/diff"
fi

# At present, test015 only passes with GNU patch.
# Larry's patch has a bug with empty files in combination with -R...
if command -v gpatch >/dev/null 2>&1; then
	patch_prog="gpatch"
else
	patch_prog="patch"
fi

diff_type=unidiff

rm -f errors

verify_diff_script() {
	orig_left="$1"
	orig_right="$2"
	the_diff="$3"
	expected_diff="$4"
	diff_opts="$5"

	if echo -- $diff_opts | grep -q -- 'w'; then
		ignore_whitespace="true"
	else
		ignore_whitespace=""
	fi

	if echo -- $diff_opts | grep -q -- 'e'; then
		is_edscript="true"
	else
		is_edscript=""
	fi
	
	verify_left="verify.$orig_left"
	verify_right="verify.$orig_right"

        if [ -e "$expected_diff" ]; then
		echo cmp "$got_diff" "$expected_diff"
                if ! cmp "$got_diff" "$expected_diff" ; then
                        echo "FAIL: $got_diff != $expected_diff" | tee -a errors
                        return 1
                fi
	fi
        if [ -z "$ignore_whitespace" -a -z "$is_edscript" -a "x$diff_type" = "xunidiff" ]; then
                cp "$orig_left" "$verify_right"
                $patch_prog --quiet -u "$verify_right" "$the_diff"
                if ! cmp "$orig_right" "$verify_right" ; then
                        echo "FAIL: $orig_right != $verify_right" | tee -a errors
                        return 1
                fi

                cp "$orig_right" "$verify_left"
                $patch_prog --quiet -u -R "$verify_left" "$the_diff"
                if ! cmp "$orig_left" "$verify_left" ; then
                        echo "FAIL: $orig_left != $verify_left" | tee -a errors
                        return 1
                fi
        elif [ -z "$ignore_whitespace" -a -z "$is_edscript" ]; then
                tail -n +3 "$the_diff" | grep -v "^+" | sed 's/^.//' > "$verify_left"
                tail -n +3 "$the_diff" | grep -v "^-" | sed 's/^.//' > "$verify_right"

                if ! cmp "$orig_left" "$verify_left" ; then
                        echo "FAIL: $orig_left != $verify_left" | tee -a errors
                        return 1
                fi
                if ! cmp "$orig_right" "$verify_right" ; then
                        echo "FAIL: $orig_right != $verify_right" | tee -a errors
                        return 1
                fi
        fi
        echo "OK: $diff_prog $orig_left $orig_right"
        return 0
}

for left in test*.left* ; do
        right="$(echo "$left" | sed 's/\.left/\.right/')"
        diff_opts="$(echo "$left" | sed 's/test[0-9]*\.left\([-a-zA-Z0-9]*\).txt/\1/')"
        expected_diff="$(echo "$left" | sed 's/test\([-0-9a-zA-Z]*\)\..*/expect\1.diff/')"
        got_diff="verify.$expected_diff"

        "$diff_prog" $diff_opts "$left" "$right" > "$got_diff"

	verify_diff_script "$left" "$right" "$got_diff" "$expected_diff" "$diff_opts"
done

# XXX required to keep GNU make completely silent during 'make regress'
if make -h 2>/dev/null |  grep -q no-print-directory; then
	make_opts="--no-print-directory"
fi
for ctest in *_test.c ; do
	prog="$(echo "$ctest" | sed 's/.c//')"
	expect_output="expect.${prog}"
	prog_output="verify.$expect_output"
	make $make_opts -s -C "$prog" regress > "$prog_output"
	if ! cmp "$prog_output" "$expect_output" ; then
		echo "FAIL: $prog_output != $expect_output" | tee -a errors
	else
		echo "OK: $prog"
	fi
done

echo
if [ -f errors ]; then
	echo "Tests failed:"
	cat errors
	exit 1
else
	echo "All tests OK"
	echo
fi
