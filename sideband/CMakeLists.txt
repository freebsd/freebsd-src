# Copyright (c) 2017-2018, Intel Corporation
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#  * Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#  * Neither the name of Intel Corporation nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

include_directories(
  internal/include
)

set(LIBSB_FILES
  src/pt_sb_session.c
  src/pt_sb_context.c
  src/pt_sb_file.c
  src/pt_sb_pevent.c
)

if (CMAKE_HOST_WIN32)
  add_definitions(
    # export libipt-sb symbols
    #
    /Dpt_sb_export=__declspec\(dllexport\)
  )
endif (CMAKE_HOST_WIN32)

add_library(libipt-sb SHARED ${LIBSB_FILES})

# put the version into the header
#
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/include/libipt-sb.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/include/libipt-sb.h
)

set_target_properties(libipt-sb PROPERTIES
  PREFIX ""
  PUBLIC_HEADER ${CMAKE_CURRENT_BINARY_DIR}/include/libipt-sb.h
  VERSION   ${PT_VERSION}
  SOVERSION ${PT_VERSION_MAJOR}
)

target_link_libraries(libipt-sb libipt)
if (PEVENT)
  target_link_libraries(libipt-sb pevent)
endif (PEVENT)
