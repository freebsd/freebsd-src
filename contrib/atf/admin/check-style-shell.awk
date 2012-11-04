#
# Automated Testing Framework (atf)
#
# Copyright (c) 2007 The NetBSD Foundation, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
# CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

function warn(msg) {
    print FILENAME "[" FNR "]: " msg > "/dev/stderr"
    error = 1
}

BEGIN {
    skip = 0
    error = 0
    emacs_modeline = 0
    vim_modeline = 0
}

/NO_CHECK_STYLE_BEGIN/ {
    skip = 1
    next
}

/NO_CHECK_STYLE_END/ {
    skip = 0
    next
}

/NO_CHECK_STYLE/ {
    next
}

{
    if (skip)
        next
}

/vim: syntax=sh/ {
    vim_modeline = 1
}

/^[ \t]*#/ {
    next
}

/[$ \t]+_[a-zA-Z0-9]+=/ {
    warn("Variable should not start with an underline")
}

/[^\\]\$[^0-9!'"$?@#*{}(|\/,]+/ {
    warn("Missing braces around variable name")
}

/=(""|'')/ {
    warn("Assignment to the empty string does not need quotes");
}

/basename[ \t]+/ {
    warn("Use parameter expansion instead of basename");
}

/if[ \t]+(test|![ \t]+test)/ {
    warn("Use [ instead of test");
}

/[ \t]+(test|\[).*==/ {
    warn("test(1)'s == operator is not portable");
}

/if.*;[ \t]*fi$/ {
    warn("Avoid using a single-line if conditional");
}

END {
    if (skip)
        warn("Missing NO_CHECK_STYLE_END");
    if (! vim_modeline)
        warn("Missing mode lines");
    if (error)
        exit 1
}

# vim: syntax=awk:expandtab:shiftwidth=4:softtabstop=4
