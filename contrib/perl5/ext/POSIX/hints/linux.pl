# libc6, aka glibc2, seems to need STRUCT_TM_HASZONE defined.
# Thanks to Bart Schuller <schuller@Lunatech.com>
# See Message-ID: <19971009002636.50729@tanglefoot>
#  XXX A Configure test is needed.
$self->{CCFLAGS} = $Config{ccflags} . ' -DSTRUCT_TM_HASZONE -DHINT_SC_EXIST' ;
