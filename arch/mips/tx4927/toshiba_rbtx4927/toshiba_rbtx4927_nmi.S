/*
 * linux/arch/mips/tx4927/toshiba_rbtx4927/toshiba_rbtx4927_nmi.S
 *
 * NMI handler for Toshiba RBTX4927 board
 *
 * Copyright (C) 2003 TimeSys Corp.
 *                    S. James Hill (James.Hill@timesys.com)
 *                                  (sjhill@realitydiluted.com)
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 *  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 *  OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 *  TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 *  USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <asm/regdef.h>
#include <asm/stackframe.h>

		.align	5
		.set noat
		NESTED(tx4927_nmi_handler, PT_SIZE, sp)
		SAVE_ALL
		CLI
		.set at
		mfc0	k0, CP0_STATUS
		lui	k1, 0x50		/* Clear BEV and NMI */
		nor	k1, zero, k1
		and	k0, k1
		mtc0	k0, CP0_STATUS
		move	a0, sp
		jal	toshiba_rbtx4927_nmi
		END(tx4927_nmi_handler)
