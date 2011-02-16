/******************************************************************************

  Copyright (c) 2001-2010, Intel Corporation 
  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are met:
  
   1. Redistributions of source code must retain the above copyright notice, 
      this list of conditions and the following disclaimer.
  
   2. Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in the 
      documentation and/or other materials provided with the distribution.
  
   3. Neither the name of the Intel Corporation nor the names of its 
      contributors may be used to endorse or promote products derived from 
      this software without specific prior written permission.
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/
/*$FreeBSD: src/sys/dev/e1000/e1000_nvm.h,v 1.2.2.2.2.1 2010/12/21 17:09:25 kensmith Exp $*/

#ifndef _E1000_NVM_H_
#define _E1000_NVM_H_

void e1000_init_nvm_ops_generic(struct e1000_hw *hw);
s32  e1000_null_read_nvm(struct e1000_hw *hw, u16 a, u16 b, u16 *c);
void e1000_null_nvm_generic(struct e1000_hw *hw);
s32  e1000_null_led_default(struct e1000_hw *hw, u16 *data);
s32  e1000_null_write_nvm(struct e1000_hw *hw, u16 a, u16 b, u16 *c);
s32  e1000_acquire_nvm_generic(struct e1000_hw *hw);

s32  e1000_poll_eerd_eewr_done(struct e1000_hw *hw, int ee_reg);
s32  e1000_read_mac_addr_generic(struct e1000_hw *hw);
s32  e1000_read_pba_num_generic(struct e1000_hw *hw, u32 *pba_num);
s32  e1000_read_pba_string_generic(struct e1000_hw *hw, u8 *pba_num,
                                   u32 pba_num_size);
s32  e1000_read_pba_length_generic(struct e1000_hw *hw, u32 *pba_num_size);
s32  e1000_read_nvm_spi(struct e1000_hw *hw, u16 offset, u16 words, u16 *data);
s32  e1000_read_nvm_microwire(struct e1000_hw *hw, u16 offset,
                              u16 words, u16 *data);
s32  e1000_read_nvm_eerd(struct e1000_hw *hw, u16 offset, u16 words,
                         u16 *data);
s32  e1000_valid_led_default_generic(struct e1000_hw *hw, u16 *data);
s32  e1000_validate_nvm_checksum_generic(struct e1000_hw *hw);
s32  e1000_write_nvm_eewr(struct e1000_hw *hw, u16 offset,
                          u16 words, u16 *data);
s32  e1000_write_nvm_microwire(struct e1000_hw *hw, u16 offset,
                               u16 words, u16 *data);
s32  e1000_write_nvm_spi(struct e1000_hw *hw, u16 offset, u16 words,
                         u16 *data);
s32  e1000_update_nvm_checksum_generic(struct e1000_hw *hw);
void e1000_stop_nvm(struct e1000_hw *hw);
void e1000_release_nvm_generic(struct e1000_hw *hw);

#define E1000_STM_OPCODE  0xDB00

#endif
