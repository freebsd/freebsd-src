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

function warn(msg) {
    print FILENAME "[" FNR "]: " msg > "/dev/stderr"
    error = 1
}

BEGIN {
    skip = 0
    error = 0
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

/#ifdef/ {
    warn("Undesired usage of #ifdef; use #if defined()")
}

/#ifndef/ {
    warn("Undesired usage of #ifndef; use #if !defined()")
}

/assert[ \t]*\(/ {
    warn("Use the macros in sanity.hpp instead of assert");
}

/include.*assert/ {
    warn("Do not include assert.h nor cassert");
}

/std::endl/ {
    warn("Use \\n instead of std::endl");
}

/\/\*/ {
    warn("Do not use C-style comments");
}

END {
    if (skip)
        warn("Missing NO_CHECK_STYLE_END");
    if (error)
        exit 1
}

# vim: syntax=awk:expandtab:shiftwidth=4:softtabstop=4
