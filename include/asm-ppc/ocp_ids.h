/*
 * FILE NAME: ocp_ids.h
 *
 * BRIEF MODULE DESCRIPTION:
 * OCP device ids based on the ideas from PCI
 *
 * The numbers below are almost completely arbitrary, and in fact
 * strings might work better.  -- paulus
 *
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/*
 * Vender  device
 * [xxxx]  [xxxx]
 *
 *  Keep in order, please
 */

/* Vendor IDs 0x0001 - 0xFFFF copied from pci_ids.h */

#define	OCP_VENDOR_INVALID	0x0000
#define	OCP_VENDOR_ARM		0x0004
#define OCP_VENDOR_IBM		0x1014
#define OCP_VENDOR_MOTOROLA	0x1057
#define	OCP_VENDOR_XILINX	0x10ee
#define	OCP_VENDOR_UNKNOWN	0xFFFF

/* device identification */

/* define type */
#define OCP_FUNC_INVALID	0x0000

/* system 0x0001 - 0x001F */

/* Timers 0x0020 - 0x002F */

/* Serial 0x0030 - 0x006F*/
#define OCP_FUNC_16550		0x0031
#define OCP_FUNC_IIC		0x0032
#define OCP_FUNC_USB		0x0033

/* Memory devices 0x0090 - 0x009F */
#define OCP_FUNC_MAL		0x0090
#define OCP_FUNC_DMA		0x0091

	/* Additional data */
	struct ocp_func_mal_data {
		int	num_tx_chans;	/* Number of TX channels */
		int	num_rx_chans;	/* Number of RX channels */
	};

/* Display 0x00A0 - 0x00AF */

/* Sound 0x00B0 - 0x00BF */

/* Mass Storage 0x00C0 - 0xxCF */
#define OCP_FUNC_IDE		0x00C0

/* Misc 0x00D0 - 0x00DF*/
#define OCP_FUNC_GPIO		0x00D0
#define OCP_FUNC_ZMII		0x00D1
#define OCP_FUNC_PERFMON	0x00D2	/* Performance Monitor */

/* Network 0x0200 - 0x02FF */
#define OCP_FUNC_EMAC		0x0200
#define OCP_FUNC_GFAR		0x0201	/* TSEC & FEC */

	/* Additional data
	 *
	 * Note about mdio_idx: When you have a zmii, it's usually
	 * not necessary, it covers the case of the 405EP which has
	 * the MDIO lines on EMAC0 only
	 */
	struct ocp_func_emac_data {
		int	zmii_idx;	/* ZMII device index or -1 */
		int	zmii_mux;	/* ZMII input of this EMAC */
		int	mal_idx;	/* MAL device index */
		int	mal_rx_chan;	/* MAL rx channel number */
		int	mal_tx1_chan;	/* MAL tx channel 1 number */
		int	mal_tx2_chan;	/* MAL tx channel 2 number */
		int	wol_irq;	/* WOL interrupt */
		int	mdio_idx;	/* EMAC idx of MDIO master or -1 */
	};

/* Bridge devices 0xE00 - 0xEFF */
#define OCP_FUNC_OPB		0x0E00

#define OCP_FUNC_UNKNOWN	0xFFFF
