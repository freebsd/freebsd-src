// Copyright 2010 The Kyua Authors.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>


static int
print_args(int argc, char* argv[])
{
    for (int i = 0; i < argc; i++)
        std::cout << "argv[" << i << "] = " << argv[i] << "\n";
    std::cout << "argv[" << argc << "] = NULL";
    return EXIT_SUCCESS;
}


static int
return_code(int argc, char* argv[])
{
    if (argc != 3)
        std::abort();

    std::istringstream iss(argv[2]);
    int code;
    iss >> code;
    return code;
}


int
main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "Must provide a helper name\n";
        std::exit(EXIT_FAILURE);
    }

    if (std::strcmp(argv[1], "print-args") == 0) {
        return print_args(argc, argv);
    } else if (std::strcmp(argv[1], "return-code") == 0) {
        return return_code(argc, argv);
    } else {
        std::cerr << "Unknown helper\n";
        return EXIT_FAILURE;
    }
}
