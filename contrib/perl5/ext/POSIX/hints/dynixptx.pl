# Need to add an extra '-lc' to the end to work around a DYNIX/ptx bug
# PR#227670 - linker error on fpgetround()

$self->{LIBS} = ['-ldb -lm -lc'];
