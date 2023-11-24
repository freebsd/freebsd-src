BEGIN {
    SUBSEP = 123.456;
    a["hello", "world"] = "foo";
    print a["hello" SUBSEP "world"];
}
