#! /bin/sh
# Copyright 2015 The Kyua Authors.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
# * Neither the name of Google Inc. nor the names of its contributors
#   may be used to endorse or promote products derived from this software
#   without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

BEGIN {
    failed = 0
}

# Skip empty lines.
/^$/ {next}

# Skip lines that do not directly reference a file.
/^[^\/]/ {next}

# Ignore known problems.  As far as I can tell, all the cases listed here are
# well-documented in the code but Doxygen fails, for some reason or another, to
# properly locate the docstrings.
/engine\/kyuafile\.cpp.*no matching class member/ {next}
/engine\/scheduler\.hpp.*Member setup\(void\).*friend/ {next}
/engine\/scheduler\.hpp.*Member wait_any\(void\)/ {next}
/utils\/optional\.ipp.*no matching file member/ {next}
/utils\/optional\.hpp.*Member make_optional\(const T &\)/ {next}
/utils\/config\/nodes\.hpp.*Member set_lua\(lutok::state &, const int\)/ {next}
/utils\/config\/nodes\.hpp.*Member push_lua\(lutok::state &\)/ {next}
/utils\/config\/nodes\.hpp.*Member set_string\(const std::string &\)/ {next}
/utils\/config\/nodes\.hpp.*Member to_string\(void\)/ {next}
/utils\/config\/nodes\.hpp.*Member is_set\(void\)/ {next}
/utils\/process\/executor\.hpp.*Member spawn\(Hook.*\)/ {next}
/utils\/process\/executor\.hpp.*Member spawn_followup\(Hook.*\)/ {next}
/utils\/process\/executor\.hpp.*Member setup\(void\).*friend/ {next}
/utils\/signals\/timer\.hpp.*Member detail::invoke_do_fired.*friend/ {next}
/utils\/stacktrace_test\.cpp.*no matching class member/ {next}

# Dump any other problems and account for the failure.
{
    failed = 1
    print
}

END {
    if (failed) {
        print "ERROR: Unexpected docstring problems encountered"
        exit 1
    } else {
        exit 0
    }
}
