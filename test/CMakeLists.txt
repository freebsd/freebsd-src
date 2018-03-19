# Copyright (c) 2015-2018, Intel Corporation
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

function(add_ptt_test name)
  set(pttc   $<TARGET_FILE:pttc>)
  set(ptdump $<TARGET_FILE:ptdump>)
  set(ptxed  $<TARGET_FILE:ptxed>)
  set(script ${BASH} ${CMAKE_SOURCE_DIR}/script/test.bash)
  set(test   ${CMAKE_CURRENT_SOURCE_DIR}/src/${name})

  add_test(
    NAME insn-${name}
    COMMAND ${script} -f -g ${pttc} -d ${ptdump} -x ${ptxed} -X --insn-decoder ${test}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/ptt-insn
  )

  add_test(
    NAME block-${name}
    COMMAND ${script} -f -g ${pttc} -d ${ptdump} -x ${ptxed} -X --block-decoder ${test}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/ptt-block
  )
endfunction(add_ptt_test)

file(GLOB TESTS
  LIST_DIRECTORIES false
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/src/
  src/*.ptt
)

file(MAKE_DIRECTORY
  ${CMAKE_CURRENT_BINARY_DIR}/ptt-insn
  ${CMAKE_CURRENT_BINARY_DIR}/ptt-block
)
foreach (test ${TESTS})
  add_ptt_test(${test})
endforeach ()

if (SIDEBAND AND PEVENT)
  add_subdirectory(pevent)
endif (SIDEBAND AND PEVENT)
