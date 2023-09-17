function foo(c, n) {
    s = "";
    for (i = 0; i < n; i++) {
        s = s c;
    }
    return s;
}

BEGIN {
    str1 = foo("a", 4500);
    str2 = foo("b", 9000);

    a[(SUBSEP = str1), (SUBSEP = str2), "c"] = 1;

    for (k in a) {
        print length(k);
    }

    print (((SUBSEP = str1), (SUBSEP = str2), "c") in a);
    print (((SUBSEP = str1) SUBSEP (SUBSEP = str2) SUBSEP "c") in a);
    delete a[(SUBSEP = str1), (SUBSEP = str2), "c"];
    print (((SUBSEP = str1), (SUBSEP = str2), "c") in a);
    print (((SUBSEP = str1) SUBSEP (SUBSEP = str2) SUBSEP "c") in a);
}
