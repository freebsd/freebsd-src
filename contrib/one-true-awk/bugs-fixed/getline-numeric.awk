{
    print $0, ($0 <= 50 ? "<=" : ">"), 50
    getline dd < ARGV[1]
    print dd, (dd <= 50 ? "<=" : ">"), 50
    if (dd == $0) print "same"
}
