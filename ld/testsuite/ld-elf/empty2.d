#source: empty2.s
#ld:
#readelf: -s
#xfail: "d30v-*-*" "dlx-*-*" "hppa*-*-*" "i960-*-*" "or32-*-*" "pj-*-*"

#...
[ 	]+[0-9]+:[ 	]+0+[ 	]+0[ 	]+FILE[	 ]+LOCAL[ 	]+DEFAULT[ 	]+ABS empty2.s
#...
[ 	]+[0-9]+:[ 	]+0*12345678[ 	]+0[ 	]+NOTYPE[	 ]+LOCAL[ 	]+DEFAULT[ 	]+ABS constant
#...
[ 	]+[0-9]+:[ 	]+[0-9a-f]+[ 	]+[0-9]+[ 	]+FUNC[	 ]+GLOBAL[ 	]+DEFAULT[ 	]+[1-9] _start
#pass
