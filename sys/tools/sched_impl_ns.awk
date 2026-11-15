#!/usr/bin/awk -f

# Copyright (c) 2026 The FreeBSD Foundation
#
# SPDX-License-Identifier: BSD-2-Clause
#
# This software was developed by Olivier Certner <olce.freebsd@certner.fr>
# at Kumacom SARL under sponsorship from the FreeBSD Foundation.

/^__END__$/ {
    keep = 0;
}

keep != 0 && $1 == "__CHECK_IMPLEMENTATION__" {
    # Check that SCHED_IMPLEMENTATION is defined
    print "#if !defined(SCHED_IMPLEMENTATION)";
    print "#error SCHED_IMPLEMENTATION not defined"
    print "#endif"
    # Check that it is consistent with that used to generate the header
    printf("#define _SCHED_IMPL_CHECK_FAILED_%s\n", $2)
    printf("/* Check that SCHED_IMPLEMENTATION is '%s'. */\n", $2);
    print "__CONCAT(_SCHED_IMPL_CHECK_FAILED_, SCHED_IMPLEMENTATION)"
    printf("#undef _SCHED_IMPL_CHECK_FAILED_%s\n", $2)
    no_print = 1;
}

keep != 0 && no_print == 0 {
    gsub("__NL__", "\n");
    print;
}

{
    no_print = 0;
}

/^__START__$/ {
    keep = 1;
}
