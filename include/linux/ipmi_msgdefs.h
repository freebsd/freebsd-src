/*
 * ipmi_smi.h
 *
 * MontaVista IPMI system management interface
 *
 * Author: MontaVista Software, Inc.
 *         Corey Minyard <minyard@mvista.com>
 *         source@mvista.com
 *
 * Copyright 2002 MontaVista Software Inc.
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the
 *  Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
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

#ifndef __LINUX_IPMI_MSGDEFS_H
#define __LINUX_IPMI_MSGDEFS_H

/* Various definitions for IPMI messages used by almost everything in
   the IPMI stack. */

#define IPMI_NETFN_APP_REQUEST	0x06
#define IPMI_NETFN_APP_RESPONSE	0x07

#define IPMI_BMC_SLAVE_ADDR	0x20

#define IPMI_GET_DEVICE_ID_CMD		0x01

#define IPMI_CLEAR_MSG_FLAGS_CMD	0x30
#define IPMI_GET_MSG_FLAGS_CMD		0x31
#define IPMI_SEND_MSG_CMD		0x34
#define IPMI_GET_MSG_CMD		0x33

#define IPMI_SET_BMC_GLOBAL_ENABLES_CMD	0x2e
#define IPMI_GET_BMC_GLOBAL_ENABLES_CMD	0x2f
#define IPMI_READ_EVENT_MSG_BUFFER_CMD	0x35

#define IPMI_MAX_MSG_LENGTH	80

#endif /* __LINUX_IPMI_MSGDEFS_H */
