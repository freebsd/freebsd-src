#! __ATF_SH__
# Copyright 2012 Google Inc.
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

Cxx="__CXX__"
ExamplesDir="__EXAMPLESDIR__"
LibDir="__LIBDIR__"


make_example() {
    cp "${ExamplesDir}/Makefile" "${ExamplesDir}/${1}.cpp" .
    make CXX="${Cxx}" "${1}"

    # Ensure that the binary we just built can find liblutok.  This is
    # needed because the lutok.pc file (which the Makefile used above
    # queries) does not provide rpaths to the installed library and
    # therefore the binary may not be able to locate it.  Hardcoding the
    # rpath flags into lutok.pc is non-trivial because we simply don't
    # have any knowledge about what the correct flag to set an rpath is.
    #
    # Additionally, setting rpaths is not always the right thing to do.
    # For example, pkgsrc will automatically change lutok.pc to add the
    # missing rpath, in which case this is unnecessary.  But in the case
    # of Fedora, adding rpaths goes against the packaging guidelines.
    if [ -n "${LD_LIBRARY_PATH}" ]; then
        export LD_LIBRARY_PATH="${LibDir}:${LD_LIBRARY_PATH}"
    else
        export LD_LIBRARY_PATH="${LibDir}"
    fi
}


example_test_case() {
    local name="${1}"; shift

    atf_test_case "${name}"
    eval "${name}_head() { \
        atf_set 'require.files' '${ExamplesDir}/${name}.cpp'; \
        atf_set 'require.progs' 'make pkg-config'; \
    }"
    eval "${name}_body() { \
        make_example '${name}'; \
        ${name}_validate; \
    }"
}


example_test_case bindings
bindings_validate() {
    atf_check -s exit:0 -o inline:'120\n' ./bindings 5
    atf_check -s exit:1 -e match:'Argument.*must be an integer' ./bindings foo
    atf_check -s exit:1 -e match:'Argument.*must be positive' ./bindings -5
}


example_test_case hello
hello_validate() {
    atf_check -s exit:0 -o inline:'Hello, world!\n' ./hello
}


example_test_case interpreter
interpreter_validate() {
    cat >script.lua <<EOF
test_variable = 12345
print("From the interpreter: " .. (test_variable - 345))
EOF

    atf_check -s exit:0 -o match:"From the interpreter: 12000" \
        -x "./interpreter <script.lua"
}


example_test_case raii
raii_validate() {
cat >expout <<EOF
String in field foo: hello
String in field bar: 123
String in field baz: bye
EOF
    atf_check -s exit:0 -o file:expout ./raii
}


atf_init_test_cases() {
    atf_add_test_case bindings
    atf_add_test_case hello
    atf_add_test_case interpreter
    atf_add_test_case raii
}
