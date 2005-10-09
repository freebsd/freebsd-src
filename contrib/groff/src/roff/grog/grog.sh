#!/bin/sh
# grog -- guess options for groff command
# Like doctype in Kernighan & Pike, Unix Programming Environment, pp 306-8.

soelim=@g@soelim

opts=
sp="([	 ]|$)"

for arg
do
	case "$arg" in
	--)
		shift; break;;
	-)
		break;;
	-C)
		sp=; opts="$opts -C"; shift; break;;
	-v | --version)
		echo "GNU grog (groff) version @VERSION@"
		exit 0;;
	--help)
		echo "usage: grog [ option ...] [files...]"
		exit 0;;
	-*)
		opts="$opts $arg"; shift;;
	*)
		break;;
	esac
done

egrep -h "^\.(\[|\])|((P|PS|[PLI]P|[pnil]p|sh|Dd|Tp|Dp|De|Cx|Cl|Oo|.* Oo|Oc|.* Oc|TS|EQ|TH|SH|so|\[|R1|GS|G1|PH|SA)$sp)" $* \
| sed -e '/^\.so/s/^.*$/.SO_START\
&\
.SO_END/' \
| $soelim \
| egrep '^\.(P|PS|[PLI]P|[pnil]p|sh|Dd|Tp|Dp|De|Cx|Cl|Oo|.* Oo|Oc|.* Oc|TS|EQ|TH|SH|\[|\]|R1|GS|G1|PH|SA|SO_START|SO_END)' \
| awk '
/^\.SO_START$/ { so = 1 }
/^\.SO_END$/ { so = 0 }
/^\.TS/ { tbl++; if (so > 0) soelim++ }
/^\.PS([ 0-9.<].*)?$/ { pic++; if (so > 0) soelim++ }
/^\.EQ/ { eqn++; if (so > 0) soelim++ }
/^\.R1/ { refer++; if (so > 0) soelim++ }
/^\.\[/ {refer_start++; if (so > 0) soelim++ }
/^\.\]/ {refer_end++; if (so > 0) soelim++ }
/^\.GS/ { grn++; if (so > 0) soelim++ }
/^\.G1/ { grap++; pic++; if (so > 0) soelim++ }
/^\.TH/ { TH++ }
/^\.[PLI]P/ { PP++ }
/^\.P$/ { P++ }
/^\.SH/ { SH++ }
/^\.(PH|SA)/ { mm++ }
/^\.([pnil]p|sh)/ { me++ }
/^\.Dd/ { mdoc++ }
/^\.(Tp|Dp|De|Cx|Cl)/ { mdoc_old++ }
/^\.(O[oc]|.* O[oc]( |$))/ {
	sub(/\\\".*/, "")
	gsub(/\"[^\"]*\"/, "")
	sub(/\".*/, "")
	sub(/^\.Oo/, " Oo ")
	sub(/^\.Oc/, " Oc ")
	sub(/ Oo$/, " Oo ")
	sub(/ Oc$/, " Oc ")
	while (/ Oo /) {
		sub(/ Oo /, " ")
		Oo++
	}
	while (/ Oc /) {
		sub(/ Oc /, " ")
		Oo--
	}
}
/^\.(PRINTSTYLE|START)/ { mom++ }

END {
	if (files ~ /^-/)
		files = "-- " files
	printf "groff"
	refer = refer || (refer_start && refer_end)
	if (pic > 0 || tbl > 0 || grn > 0 || grap > 0 || eqn > 0 || refer > 0) {
		printf " -"
		if (soelim > 0) printf "s"
		if (refer > 0) printf "R"
		if (grn > 0) printf "g"
		if (grap > 0) printf "G"
		if (pic > 0) printf "p"
		if (tbl > 0) printf "t"
		if (eqn > 0) printf "e"
	}
	if (me > 0)
		printf " -me"
	else if (SH > 0 && TH > 0)
		printf " -man"
	else if (mom > 0)
		printf " -mom"
	else if (PP > 0)
		printf " -ms"
	else if (P > 0 || mm > 0)
		printf " -mm"
	else if (mdoc > 0) {
		if (mdoc_old > 0 || Oo > 0)
			printf " -mdoc-old"
		else
			printf " -mdoc"
	}
	if (opts != "")
		printf "%s", opts
	if (files != "")
		printf " %s", files
	print ""
}' "opts=$opts" "files=$*" -
