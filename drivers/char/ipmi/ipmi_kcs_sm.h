/*
 * ipmi_kcs_sm.h
 *
 * State machine for handling IPMI KCS interfaces.
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

struct kcs_data;

void init_kcs_data(struct kcs_data *kcs,
		   unsigned int    port,
		   unsigned char   *addr);

/* Start a new transaction in the state machine.  This will return -2
   if the state machine is not idle, -1 if the size is invalid (to
   large or too small), or 0 if the transaction is successfully
   completed. */
int start_kcs_transaction(struct kcs_data *kcs, char *data, unsigned int size);

/* Return the results after the transaction.  This will return -1 if
   the buffer is too small, zero if no transaction is present, or the
   actual length of the result data. */
int kcs_get_result(struct kcs_data *kcs, unsigned char *data, int length);

enum kcs_result
{
	KCS_CALL_WITHOUT_DELAY, /* Call the driver again immediately */
	KCS_CALL_WITH_DELAY,	/* Delay some before calling again. */
	KCS_TRANSACTION_COMPLETE, /* A transaction is finished. */
	KCS_SM_IDLE,		/* The SM is in idle state. */
	KCS_SM_HOSED,		/* The hardware violated the state machine. */
	KCS_ATTN		/* The hardware is asserting attn and the
				   state machine is idle. */
};

/* Call this periodically (for a polled interface) or upon receiving
   an interrupt (for a interrupt-driven interface).  If interrupt
   driven, you should probably poll this periodically when not in idle
   state.  This should be called with the time that passed since the
   last call, if it is significant.  Time is in microseconds. */
enum kcs_result kcs_event(struct kcs_data *kcs, long time);

/* Return the size of the KCS structure in bytes. */
int kcs_size(void);
