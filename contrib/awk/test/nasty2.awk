# Based on nasty.awk, test same thing for printf
#
BEGIN {
a="aaaaa"
a=a a #10
a=a a #20
a=a a #40
a=a a #80
a=a a #160
a=a a # i.e. a is long enough

printf("a = %s, f() = %s\n", a, f())
print a
}

function f()
{
gsub(/a/, "123", a)
return "X"
}
