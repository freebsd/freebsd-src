/*
 * linux/arch/mips/tx4927/toshiba_rbtx4927/toshiba_rbtx4927_rtc.c
 *
 * RTC routines for Dallas chip.
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
#include <asm/mc146818rtc.h>

static unsigned char ds1742_rtc_read_data(unsigned long addr)
{
	return readb(addr);
}

static void ds1742_rtc_write_data(unsigned char data, unsigned long addr)
{
	writeb(data, addr);
}

static int ds1742_rtc_bcd_mode(void)
{
	return 1;
}

struct rtc_ops ds1742_rtc_ops = {
	&ds1742_rtc_read_data,
	&ds1742_rtc_write_data,
	&ds1742_rtc_bcd_mode
};
