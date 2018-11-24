# $FreeBSD$

atf_test_case multibyte
multibyte_head()
{
	atf_set "descr" "Check matching multibyte characters (PR153502)"
}
multibyte_body()
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

atf_init_test_cases()
{
	atf_add_test_case multibyte
}
