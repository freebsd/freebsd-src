/*
  This software is available to you under a choice of one of two
  licenses.  You may choose to be licensed under the terms of the GNU
  General Public License (GPL) Version 2, available at
  <http://www.fsf.org/copyleft/gpl.html>, or the OpenIB.org BSD
  license, available in the LICENSE.TXT file accompanying this
  software.  These details are also available at
  <http://openib.org/license.html>.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
  ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

  Copyright (c) 2004 Topspin Communications.  All rights reserved.

  $Id$
*/

#ifndef _SDP_INET_H
#define _SDP_INET_H

/*
 * constants shared between user and kernel space.
 */

#ifndef SOLARIS_BUILD
#ifdef __FreeBSD__
#include <sys/socket.h>
#else
#define AF_INET_SDP 27			  /* SDP socket protocol family */
#define AF_INET6_SDP 28                   /* SDP socket protocol family */
#endif
#else
#define AF_INET_SDP 31  /* This is an invalid family on native solaris
                         * and will only work using QuickTransit */
//TODO XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXx
#define AF_INET6_SDP 32                   /* SDP socket protocol family */
#endif

#define AF_INET_STR "AF_INET_SDP"	/* SDP enabled environment variable */
#define AF_INET6_STR "AF_INET6_SDP"     /* SDP enabled environment variable */

#ifndef SDP_ZCOPY_THRESH
#define SDP_ZCOPY_THRESH 80
#endif

#ifndef SDP_LAST_BIND_ERR
#define SDP_LAST_BIND_ERR 81
#endif

#endif /* _SDP_INET_H */
