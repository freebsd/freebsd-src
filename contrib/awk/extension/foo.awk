BEGIN {
    extension("./dl.so","dlload")
    zaxxon("hi there", "this is", "a test", "of argument passing")
    zaxxon(1)
    zaxxon(1,2)
    z = zaxxon(1,2,3,4)
    z = zaxxon(1,zaxxon(zaxxon("foo")),3,4)
    print z
}
