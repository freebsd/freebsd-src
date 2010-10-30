#source: start.s
#source: symbol2ref.s
#source: symbol2w.s
#ld: -T group.ld
#warning: ^[^\\n]*\.[obj]+: warning: function 'Foo' used$
#readelf: -s
#notarget: "sparc64-*-solaris2*" "sparcv9-*-solaris2*"
#xfail: "arc-*-*" "d30v-*-*" "dlx-*-*" "i960-*-*" "or32-*-*" "pj-*-*"

# Check that warnings are generated for the symbols in .gnu.warning
# construct and that the symbol still appears as expected.

#...
[ 	]+[0-9]+:[ 	]+[0-9a-f]+[ 	]+20[ 	]+OBJECT[	 ]+GLOBAL DEFAULT[ 	]+[1-2] Foo
#pass
