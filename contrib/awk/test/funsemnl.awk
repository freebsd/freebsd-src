# make sure that ; + \n at end after function works
function foo() { print "foo" } ;
BEGIN { foo() }
