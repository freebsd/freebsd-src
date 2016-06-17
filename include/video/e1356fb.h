/*
 *	e1356fb.h  --  Epson SED1356 Framebuffer Driver IOCTL Interface
 *
 *	Copyright 2001, 2002, 2003 MontaVista Software Inc.
 *	Author: MontaVista Software, Inc.
 *		stevel@mvista.com or source@mvista.com
 *
 *	This program is free software; you can redistribute  it and/or modify it
 *	under  the terms of  the GNU General  Public License as published by the
 *	Free Software Foundation;  either version 2 of the  License, or (at your
 *	option) any later version.
 *
 *	THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *	WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *	MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *	NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *	NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *	USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *	ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *	THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	You should have received a copy of the  GNU General Public License along
 *	with this program; if not, write  to the Free Software Foundation, Inc.,
 *	675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
 * IOCTLs to SED1356 fb driver. 0x45 is 'E' for Epson.
 */
#define FBIO_SED1356_BITBLT 0x4500

typedef struct {
	int   operation;
	int   rop;
	int   src_y;
	int   src_x;
	int   src_width;
	int   src_height;
	int   dst_y;
	int   dst_x;
	int   dst_width;
	int   dst_height;
	int   pattern_x;
	int   pattern_y;
	int   attribute;
	unsigned int bg_color;
	unsigned int fg_color;
	unsigned short* src;
	int   srcsize;
	int   srcstride;
} blt_info_t;

enum blt_attribute_t {
	BLT_ATTR_TRANSPARENT = 1
};

enum blt_operation_t {
	BLT_WRITE_ROP = 0,
	BLT_READ,
	BLT_MOVE_POS_ROP,
	BLT_MOVE_NEG_ROP,
	BLT_WRITE_TRANSP,
	BLT_MOVE_POS_TRANSP,
	BLT_PAT_FILL_ROP,
	BLT_PAT_FILL_TRANSP,
	BLT_COLOR_EXP,
	BLT_COLOR_EXP_TRANSP,
	BLT_MOVE_COLOR_EXP,
	BLT_MOVE_COLOR_EXP_TRANSP,
	BLT_SOLID_FILL
};
