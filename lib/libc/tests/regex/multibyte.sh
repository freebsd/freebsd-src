atf_test_case bmpat
bmpat_head()
{
	atf_set "descr" "Check matching multibyte characters (PR153502)"
}
bmpat_body()
{
	export LC_CTYPE="C.UTF-8"

	printf 'é' | atf_check -o "inline:é" \
	    sed -ne '/^.$/p'
	printf 'éé' | atf_check -o "inline:éé" \
	    sed -ne '/^..$/p'
	printf 'aéa' | atf_check -o "inline:aéa" \
	    sed -ne '/a.a/p'
	printf 'aéa'| atf_check -o "inline:aéa" \
	    sed -ne '/a.*a/p'
	printf 'aaéaa' | atf_check -o "inline:aaéaa" \
	    sed -ne '/aa.aa/p'
	printf 'aéaéa' | atf_check -o "inline:aéaéa" \
	    sed -ne '/a.a.a/p'
	printf 'éa' | atf_check -o "inline:éa" \
	    sed -ne '/.a/p'
	printf 'aéaa' | atf_check -o "inline:aéaa" \
	    sed -ne '/a.aa/p'
	printf 'éaé' | atf_check -o "inline:éaé" \
	    sed -ne '/.a./p'
}

atf_test_case icase
icase_head()
{
	atf_set "descr" "Check case-insensitive matching for characters 128-255"
}
icase_body()
{
	export LC_CTYPE="C.UTF-8"

	a=$(printf '\302\265\n')	# U+00B5
	b=$(printf '\316\234\n')	# U+039C
	c=$(printf '\316\274\n')	# U+03BC

	echo $b | atf_check -o "inline:$b\n" sed -ne "/$a/Ip"
	echo $c | atf_check -o "inline:$c\n" sed -ne "/$a/Ip"
}

atf_test_case mbset cleanup
mbset_head()
{
	atf_set "descr" "Check multibyte sets matching"
}
mbset_body()
{
	export LC_CTYPE="C.UTF-8"

	# This involved an erroneously implemented optimization which reduces
	# single-element sets to an exact match with a single codepoint.
	# Match sets record small-codepoint characters in a bitmap and
	# large-codepoint characters in an array; the optimization would falsely
	# trigger if either the bitmap or the array was a singleton, ignoring
	# the members of the other side of the set.
	#
	# To exercise this, we construct sets which have one member of one side
	# and one or more of the other, and verify that all members can be
	# found.
	printf "a" > mbset; atf_check -o not-empty sed -ne '/[aà]/p' mbset
	printf "à" > mbset; atf_check -o not-empty sed -ne '/[aà]/p' mbset
	printf "a" > mbset; atf_check -o not-empty sed -ne '/[aàá]/p' mbset
	printf "à" > mbset; atf_check -o not-empty sed -ne '/[aàá]/p' mbset
	printf "á" > mbset; atf_check -o not-empty sed -ne '/[aàá]/p' mbset
	printf "à" > mbset; atf_check -o not-empty sed -ne '/[abà]/p' mbset
	printf "a" > mbset; atf_check -o not-empty sed -ne '/[abà]/p' mbset
	printf "b" > mbset; atf_check -o not-empty sed -ne '/[abà]/p' mbset
	printf "a" > mbset; atf_check -o not-empty sed -Ene '/[aà]/p' mbset
	printf "à" > mbset; atf_check -o not-empty sed -Ene '/[aà]/p' mbset
	printf "a" > mbset; atf_check -o not-empty sed -Ene '/[aàá]/p' mbset
	printf "à" > mbset; atf_check -o not-empty sed -Ene '/[aàá]/p' mbset
	printf "á" > mbset; atf_check -o not-empty sed -Ene '/[aàá]/p' mbset
	printf "à" > mbset; atf_check -o not-empty sed -Ene '/[abà]/p' mbset
	printf "a" > mbset; atf_check -o not-empty sed -Ene '/[abà]/p' mbset
	printf "b" > mbset; atf_check -o not-empty sed -Ene '/[abà]/p' mbset
}
mbset_cleanup()
{
	rm -f mbset
}

atf_init_test_cases()
{
	atf_add_test_case bmpat
	atf_add_test_case icase
	atf_add_test_case mbset
}
