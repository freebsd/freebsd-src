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

file(MAKE_DIRECTORY ${MAN_OUTPUT_DIRECTORY}/man3)

find_program(PANDOC pandoc
  DOC "Path to pandoc; used for building man pages."
)

function(add_man_page filename section function)
  set(input ${CMAKE_CURRENT_SOURCE_DIR}/${filename})
  set(output ${MAN_OUTPUT_DIRECTORY}/man${section}/${function}.${section})

  add_custom_command(
    OUTPUT ${output}
    COMMAND ${PANDOC} -s -f markdown -t man -o ${output} ${input}
    MAIN_DEPENDENCY ${filename}
  )
endfunction(add_man_page)

function(install_man_page section function)
  install(
    FILES ${MAN_OUTPUT_DIRECTORY}/man${section}/${function}.${section}
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man${section}
  )
endfunction(install_man_page)

function(add_man_page_alias section function alias)
  set(output ${MAN_OUTPUT_DIRECTORY}/man${section}/${alias}.${section})

  file(WRITE ${output} ".so man${section}/${function}.${section}\n")

  install_man_page(${section} ${alias})
endfunction(add_man_page_alias)

set(MAN3_FUNCTIONS
  pt_library_version
  pt_config
  pt_packet
  pt_alloc_encoder
  pt_enc_get_offset
  pt_enc_get_config
  pt_pkt_alloc_decoder
  pt_pkt_sync_forward
  pt_pkt_get_offset
  pt_qry_alloc_decoder
  pt_qry_sync_forward
  pt_qry_get_offset
  pt_qry_cond_branch
  pt_qry_event
  pt_qry_time
  pt_image_alloc
  pt_image_add_file
  pt_image_remove_by_filename
  pt_image_set_callback
  pt_insn_alloc_decoder
  pt_insn_sync_forward
  pt_insn_get_offset
  pt_insn_get_image
  pt_insn_next
  pt_iscache_alloc
  pt_iscache_add_file
  pt_iscache_read
  pt_iscache_set_limit
  pt_blk_alloc_decoder
  pt_blk_sync_forward
  pt_blk_get_offset
  pt_blk_next
)

foreach (function ${MAN3_FUNCTIONS})
  set(MAN_PAGES ${MAN_PAGES} ${MAN_OUTPUT_DIRECTORY}/man3/${function}.3)

  add_man_page(${function}.3.md 3 ${function})
  install_man_page(3 ${function})
endforeach ()

add_man_page_alias(3 pt_config pt_cpu_errata)
add_man_page_alias(3 pt_packet pt_enc_next)
add_man_page_alias(3 pt_packet pt_pkt_next)
add_man_page_alias(3 pt_alloc_encoder pt_free_encoder)
add_man_page_alias(3 pt_enc_get_offset pt_enc_sync_set)
add_man_page_alias(3 pt_enc_get_config pt_pkt_get_config)
add_man_page_alias(3 pt_enc_get_config pt_qry_get_config)
add_man_page_alias(3 pt_enc_get_config pt_insn_get_config)
add_man_page_alias(3 pt_enc_get_config pt_blk_get_config)
add_man_page_alias(3 pt_pkt_alloc_decoder pt_pkt_free_decoder)
add_man_page_alias(3 pt_pkt_sync_forward pt_pkt_sync_backward)
add_man_page_alias(3 pt_pkt_sync_forward pt_pkt_sync_set)
add_man_page_alias(3 pt_pkt_get_offset pt_pkt_get_sync_offset)
add_man_page_alias(3 pt_qry_alloc_decoder pt_qry_free_decoder)
add_man_page_alias(3 pt_qry_sync_forward pt_qry_sync_backward)
add_man_page_alias(3 pt_qry_sync_forward pt_qry_sync_set)
add_man_page_alias(3 pt_qry_get_offset pt_qry_get_sync_offset)
add_man_page_alias(3 pt_qry_cond_branch pt_qry_indirect_branch)
add_man_page_alias(3 pt_qry_time pt_qry_core_bus_ratio)
add_man_page_alias(3 pt_qry_time pt_insn_time)
add_man_page_alias(3 pt_qry_time pt_insn_core_bus_ratio)
add_man_page_alias(3 pt_qry_time pt_blk_time)
add_man_page_alias(3 pt_qry_time pt_blk_core_bus_ratio)
add_man_page_alias(3 pt_qry_event pt_insn_event)
add_man_page_alias(3 pt_qry_event pt_blk_event)
add_man_page_alias(3 pt_image_alloc pt_image_free)
add_man_page_alias(3 pt_image_alloc pt_image_name)
add_man_page_alias(3 pt_image_add_file pt_image_copy)
add_man_page_alias(3 pt_image_add_file pt_image_add_cached)
add_man_page_alias(3 pt_image_remove_by_filename pt_image_remove_by_asid)
add_man_page_alias(3 pt_insn_alloc_decoder pt_insn_free_decoder)
add_man_page_alias(3 pt_insn_sync_forward pt_insn_sync_backward)
add_man_page_alias(3 pt_insn_sync_forward pt_insn_sync_set)
add_man_page_alias(3 pt_insn_get_offset pt_insn_get_sync_offset)
add_man_page_alias(3 pt_insn_get_image pt_insn_set_image)
add_man_page_alias(3 pt_insn_get_image pt_blk_get_image)
add_man_page_alias(3 pt_insn_get_image pt_blk_set_image)
add_man_page_alias(3 pt_insn_next pt_insn)
add_man_page_alias(3 pt_iscache_alloc pt_iscache_free)
add_man_page_alias(3 pt_iscache_alloc pt_iscache_name)
add_man_page_alias(3 pt_blk_alloc_decoder pt_blk_free_decoder)
add_man_page_alias(3 pt_blk_sync_forward pt_blk_sync_backward)
add_man_page_alias(3 pt_blk_sync_forward pt_blk_sync_set)
add_man_page_alias(3 pt_blk_get_offset pt_blk_get_sync_offset)
add_man_page_alias(3 pt_blk_next pt_block)

add_custom_target(man ALL DEPENDS ${MAN_PAGES})
