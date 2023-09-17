function foo() {
    a = "";
    for (i = 0; i < 10000; i++) {
        a = a "c";
    }
    return a;
}

BEGIN {
    FS = foo();
    $0="foo";
    print $1;
}
