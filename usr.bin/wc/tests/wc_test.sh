#
# Copyright (c) 2023 Klara, Inc.
#
# SPDX-License-Identifier: BSD-2-Clause
#

#
# These tests need to run in a multibyte locale with non-localized
# error messages.
#
export LC_CTYPE=C.UTF-8
export LC_MESSAGES=C

#
# Size of wc's read buffer.
#
MAXBSIZE=65536

#
# Sample text containing multibyte characters
#
tv="Der bode en underlig gråsprængt en
på den yderste nøgne ø; –
han gjorde visst intet menneske mén
hverken på land eller sjø;
dog stundom gnistred hans øjne stygt, –
helst mod uroligt vejr, –
og da mente folk, at han var forrykt,
og da var der få, som uden frykt
kom Terje Vigen nær.
"
tvl=10
tvw=55
tvc=300
tvm=283
tvcL=42
tvmL=39

#
# Run a series of tests using the same input file.  The first argument
# is the name of the file.  The next three are the expected line,
# word, and byte counts.  The optional fifth is the expected character
# count; if not provided, it is expected to be identical to the byte
# count.
#
atf_check_wc() {
	local file="$1"
	local l="$2"
	local w="$3"
	local c="$4"
	local m="${5-$4}"

	atf_check -o match:"^ +${l} +${w} +${c}\$" wc <"${file}"
	atf_check -o match:"^ +${l}\$" wc -l <"${file}"
	atf_check -o match:"^ +${w}\$" wc -w <"${file}"
	atf_check -o match:"^ +${c}\$" wc -c <"${file}"
	atf_check -o match:"^ +${m}\$" wc -m <"${file}"
	atf_check -o match:"^ +${l} +${w} +${c} ${file}\$" wc "$file"
	atf_check -o match:"^ +${l} ${file}\$" wc -l "$file"
	atf_check -o match:"^ +${w} ${file}\$" wc -w "$file"
	atf_check -o match:"^ +${c} ${file}\$" wc -c "$file"
	atf_check -o match:"^ +${m} ${file}\$" wc -m "$file"
}

atf_test_case basic
basic_head()
{
	atf_set "descr" "Basic test case"
}
basic_body()
{
	printf "a b\n" >foo
	atf_check_wc foo 1 2 4
}

atf_test_case blank
blank_head()
{
	atf_set "descr" "Input containing only blank lines"
}
blank_body()
{
	printf "\n\n\n" >foo
	atf_check_wc foo 3 0 3
}

atf_test_case empty
empty_head()
{
	atf_set "descr" "Empty input"
}
empty_body()
{
	printf "" >foo
	atf_check_wc foo 0 0 0
}

atf_test_case invalid
invalid_head()
{
	atf_set "descr" "Invalid multibyte input"
}
invalid_body()
{
	printf "a\377b\n" >foo
	atf_check \
	    -e match:"Illegal byte sequence" \
	    -o match:"^ +4 foo$" \
	    wc -m foo
}

atf_test_case multiline
multiline_head()
{
	atf_set "descr" "Multiline, multibyte input"
}
multiline_body()
{
	printf "%s\n" "$tv" >foo
	atf_check_wc foo $tvl $tvw $tvc $tvm
	# longest line in bytes
	atf_check -o match:"^ +$tvc +$tvcL foo" wc -cL foo
	atf_check -o match:"^ +$tvc +$tvcL" wc -cL <foo
	# longest line in characters
	atf_check -o match:"^ +$tvm +$tvmL foo" wc -mL foo
	atf_check -o match:"^ +$tvm +$tvmL" wc -mL <foo
}

atf_test_case multiline_repeated
multiline_repeated_head()
{
	atf_set "descr" "Multiline input exceeding the input buffer size"
}
multiline_repeated_body()
{
	local c=0
	while [ $c -lt 1000 ] ; do
		printf "%1\$s\n%1\$s\n%1\$s\n%1\$s\n%1\$s\n" "$tv"
		c=$((c+5))
	done >foo
	atf_check_wc foo $((tvl*c)) $((tvw*c)) $((tvc*c)) $((tvm*c))
}

atf_test_case nul
nul_head()
{
	atf_set "descr" "Input containing NUL"
}
nul_body()
{
	printf "a\0b\n" >foo
	atf_check_wc foo 1 1 4
}

atf_test_case poop
poop_head()
{
	atf_set "descr" "Multibyte sequence across buffer boundary"
}
poop_body()
{
	local l=0 w=0 c=0 m=0
	# The code below produces a stream of 4-byte UTF-8 sequences
	# aligned on 5-byte boundaries, ensuring that the first full
	# read of length MAXBSIZE will end in a partial sequence —
	# unless MAXBSIZE is a multiple of 5 (not possible since it's
	# a power of 2) or one less than a multiple of 5 (e.g. 2^18 =
	# 262,144 = (52429 * 5) - 1) in which case we prepend a single
	# newline to push our sequence out of phase.
	atf_check_not_equal 0 $((MAXBSIZE % 5))
	:>foo
	if [ $((MAXBSIZE % 5)) -eq 4 ] ; then
		printf "\n"
		l=$((l + 1))
		c=$((c + 1))
		m=$((m + 1))
	fi >>foo
	while [ $c -le $MAXBSIZE ] ; do
		printf "💩.💩.💩.💩.💩.💩.💩.💩.💩.💩.💩.💩.💩.💩.💩.💩\n"
		l=$((l + 1))
		w=$((w + 1))
		c=$((c + 80)) # 80 bytes
		m=$((m + 32)) # 32 multibyte characters
	done >>foo
	atf_check_wc foo $l $w $c $m
}

atf_test_case total
total_head()
{
	atf_set "descr" "Multiple inputs"
}
total_body()
{
	printf "%s\n" "$tv" >foo
	printf "%s\n" "$tv" >bar
	atf_check \
	    -o match:"^ +$((tvl*2)) +$((tvw*2)) +$((tvc*2)) total$" \
	    wc foo bar
}

atf_test_case unterminated
unterminated_head()
{
	atf_set "descr" "Input not ending in newline"
}
unterminated_body()
{
	printf "a b" >foo
	atf_check_wc foo 0 2 3
}

atf_test_case usage
usage_head()
{
	atf_set "descr" "Trigger usage message"
}
usage_body()
{
	atf_check -s exit:1 -e match:"usage: wc" wc -\?
}

atf_test_case whitespace
whitespace_head()
{
	atf_set "descr" "Input containing only whitespace and newlines"
}
whitespace_body()
{
	printf "\n \n\t\n" >foo
	atf_check_wc foo 3 0 5
}

atf_init_test_cases()
{
	atf_add_test_case basic
	atf_add_test_case blank
	atf_add_test_case empty
	atf_add_test_case invalid
	atf_add_test_case multiline
	atf_add_test_case multiline_repeated
	atf_add_test_case nul
	atf_add_test_case poop
	atf_add_test_case total
	atf_add_test_case unterminated
	atf_add_test_case usage
	atf_add_test_case whitespace
}
