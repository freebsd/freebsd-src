/*
 *  linux/drivers/message/fusion/isense.c
 *      Little linux driver / shim that interfaces with the Fusion MPT
 *      Linux base driver to provide english readable strings in SCSI
 *      Error Report logging output.  This module implements SCSI-3
 *      Opcode lookup and a sorted table of SCSI-3 ASC/ASCQ strings.
 *
 *  Copyright (c) 1991-2002 Steven J. Ralston
 *  Written By: Steven J. Ralston
 *  (yes I wrote some of the orig. code back in 1991!)
 *  (mailto:sjralston1@netscape.net)
 *  (mailto:mpt_linux_developer@lsil.com)
 *
 *  $Id: isense.c,v 1.34 2003/03/18 22:49:48 pdelaney Exp $
 */
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; version 2 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    NO WARRANTY
    THE PROGRAM IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OR
    CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED INCLUDING, WITHOUT
    LIMITATION, ANY WARRANTIES OR CONDITIONS OF TITLE, NON-INFRINGEMENT,
    MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Each Recipient is
    solely responsible for determining the appropriateness of using and
    distributing the Program and assumes all risks associated with its
    exercise of rights under this Agreement, including but not limited to
    the risks and costs of program errors, damage to or loss of data,
    programs or equipment, and unavailability or interruption of operations.

    DISCLAIMER OF LIABILITY
    NEITHER RECIPIENT NOR ANY CONTRIBUTORS SHALL HAVE ANY LIABILITY FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING WITHOUT LIMITATION LOST PROFITS), HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
    TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
    USE OR DISTRIBUTION OF THE PROGRAM OR THE EXERCISE OF ANY RIGHTS GRANTED
    HEREUNDER, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGES

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <asm/io.h>
#if defined (__sparc__)
#include <linux/timer.h>
#endif

/* Hmmm, avoid undefined spinlock_t on lk-2.2.14-5.0 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0)
#include <asm/spinlock.h>
#endif

#define MODULEAUTHOR "Steven J. Ralston"
#define COPYRIGHT "Copyright (c) 2001-2002 " MODULEAUTHOR
#include "mptbase.h"

#include "isense.h"

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
/*
 *  Private data...
 */

/*
 *  YIKES!  I don't usually #include C source files, but..
 *  The following #include's pulls in our needed ASCQ_Table[] array,
 *  ASCQ_TableSz integer, and ScsiOpcodeString[] array!
 */
#include "ascq_tbl.c"
#include "scsiops.c"

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
#define my_NAME		"SCSI-3 Opcodes & ASC/ASCQ Strings"
#define my_VERSION	MPT_LINUX_VERSION_COMMON
#define MYNAM		"isense"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,62)
EXPORT_NO_SYMBOLS;
#endif
MODULE_AUTHOR(MODULEAUTHOR);
MODULE_DESCRIPTION(my_NAME);
MODULE_LICENSE("GPL");

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
int __init isense_init(void)
{
	show_mptmod_ver(my_NAME, my_VERSION);

	/*
	 *  Install our handler
	 */
	if (mpt_register_ascqops_strings(&ASCQ_Table[0], ASCQ_TableSize, ScsiOpcodeString) != 1)
	{
		printk(KERN_ERR MYNAM ": ERROR: Can't register with Fusion MPT base driver!\n");
		return -EBUSY;
	}
	printk(KERN_INFO MYNAM ": Registered SCSI-3 Opcodes & ASC/ASCQ Strings\n");
	return 0;
}


/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/
static void isense_exit(void)
{
#ifdef MODULE
	mpt_deregister_ascqops_strings();
#endif
	printk(KERN_INFO MYNAM ": Deregistered SCSI-3 Opcodes & ASC/ASCQ Strings\n");
}

module_init(isense_init);
module_exit(isense_exit);

/*=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=*/

