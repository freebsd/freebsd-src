BEGIN {
    RS = ""; FS = "\n"
}

{
    print "Name is: ", $1
    print "Address is: ", $2
    print "City and State are: ", $3
    print ""
}
