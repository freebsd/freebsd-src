# noassign.awk --- library file to avoid the need for a
# special option that disables command-line assignments
#
# Arnold Robbins, arnold@gnu.org, Public Domain
# October 1999

function disable_assigns(argc, argv,    i)
{
    for (i = 1; i < argc; i++)
        if (argv[i] ~ /^[A-Za-z_][A-Za-z_0-9]*=.*/)
            argv[i] = ("./" argv[i])
}

BEGIN {
    if (No_command_assign)
        disable_assigns(ARGC, ARGV)
}
