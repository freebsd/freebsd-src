#!/bin/sh
# grog -- guess options for groff command
# Like doctype in Kernighan & Pike, Unix Programming Environment, pp 306-8.

soelim=gsoelim

opts=

for arg
do
	case "$arg" in
	--)
		shift; break;;
	-)
		break;;
	-*)
		opts="$opts $arg"; shift;;
	*)
		break;;
	esac
done

egrep -h '^\.(P|[LI]P|[pnil]p|sh|Dd|Tp|Dp|De|Cx|Cl|Oo|Oc|TS|EQ|TH|SH|so|\[|R1|PH|SA)' $* \
| sed -e '/^\.so/s/^.*$/.SO_START\
&\
.SO_END/' \
| $soelim \
| egrep '^\.(P|[LI]P|[pnil]p|sh|Dd|Tp|Dp|De|Cx|Cl|Oo|Oc|TS|EQ|TH|SH|\[|R1|PH|SA|SO_START|SO_END)' \
| awk '
/^\.SO_START$/ { so = 1 }
/^\.SO_END$/ { so = 0 }
/^\.TS/ { tbl++; if (so > 0) soelim++ }
/^\.PS([ 0-9.<].*)?$/ { pic++; if (so > 0) soelim++ }
/^\.EQ/ { eqn++; if (so > 0) soelim++ }
/^\.(R1|\[)/ { refer++; if (so > 0) soelim++ }
/^\.TH/ { TH++ }
/^\.[PLI]P/ { PP++ }
/^\.P$/ { P++ }
/^\.SH/ { SH++ }
/^\.(PH|SA)/ { mm++ }
/^\.([pnil]p|sh)/ { me++ }
/^\.Dd/ { mdoc++ }
/^\.(Tp|Dp|De|Cx|Cl)/ { mdoc_old++ }
/^\.Oo/ { Oo++ }
/^\.Oc/ { Oo-- }

END {
	if (files ~ /^-/)
		files = "-- " files
	printf "groff"
	if (pic > 0 || tbl > 0 || eqn > 0 || refer > 0) {
		printf " -"
		if (soelim > 0) printf "s"
		if (refer > 0) printf "R"
		if (pic > 0) printf "p"
		if (tbl > 0) printf "t"
		if (eqn > 0) printf "e"
	}
	if (me > 0)
		printf " -me"
	else if (SH > 0 && TH > 0)
		printf " -man"
	else if (PP > 0)
		printf " -ms"
	else if (P > 0 || mm > 0)
		printf " -mm"
	else if (mdoc > 0) {
		if (mdoc_old > 0 || Oo > 0)
			printf " -mdoc.old"
		else
			printf " -mdoc"
	}
	if (opts != "")
		printf "%s", opts
	if (files != "")
		printf " %s", files
	print ""
}' "opts=$opts" "files=$*" -
