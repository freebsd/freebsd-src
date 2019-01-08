/*
 * Copyright (c) 2002-2005, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    in the file LICENSE.txt in the root directory. The license is also
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is in the file
 *    LICENSE2.txt in the root directory. The license is also available from
 *    the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a 
 *    copy of which is in the file LICENSE3.txt in the root directory. The 
 *    license is also available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/gpl-license.php.
 *
 * Licensee has the right to choose one of the above licenses.
 *
 * Redistributions of source code must retain the above copyright
 * notice and one of the license notices.
 *
 * Redistributions in binary form must reproduce both the above copyright
 * notice, one of the license notices in the documentation
 * and/or other materials provided with the distribution.
 */

#include "dapl_proto.h"
/*
 * Class Thread
 *
 * Threads subsystem initialization
 */
void DT_Thread_Init(Per_Test_Data_t * pt_ptr)
{
	DT_Mdep_LockInit(&pt_ptr->Thread_counter_lock);
	pt_ptr->Thread_counter = 0;

	/*
	 * Initialize the synchronization event in the pt_ptr so it's ready
	 * to be signalled when the time comes.  The countdown counter
	 * lets me coordinate with all the test threads so that the server
	 * thread doesn't get notified that the test endpoints are ready
	 * until they actually are.  Only transaction tests use this *
	 * functionality; if the performance test gets changed to use
	 * multiple threads on the server side then that code semantic
	 * will need to be added for final test endpoint setup
	 * notification or there will continue  to be a race condition
	 * between the main server thread and the server test threads.
	 */
	DT_Mdep_wait_object_init(&pt_ptr->synch_wait_object);
	pt_ptr->Countdown_Counter = 0;

}

/*
 * Threads subsystem destroying
 */
void DT_Thread_End(Per_Test_Data_t * pt_ptr)
{
	DT_Mdep_LockDestroy(&pt_ptr->Thread_counter_lock);

	/*
	 * destroy the wait object created by init.
	 */
	DT_Mdep_wait_object_destroy(&pt_ptr->synch_wait_object);

}

/*
 * Thread constructor
 *
 * NOTE: This routine does NOT create a thread as the name implies. The thread
 * is created in DT_Thread_Start (which is counter intuitive)
 */
Thread *DT_Thread_Create(Per_Test_Data_t * pt_ptr,
			 void (*fn) (void *),
			 void *param, unsigned int stacksize)
{
	Thread *thread_ptr;
	thread_ptr =
	    (Thread *) DT_MemListAlloc(pt_ptr, "thread.c", THREAD,
				       sizeof(Thread));
	if (thread_ptr == NULL) {
		return NULL;
	}
	thread_ptr->param = param;
	thread_ptr->function = fn;
	thread_ptr->thread_handle = 0;
	thread_ptr->stacksize = stacksize;

	DT_Mdep_Lock(&pt_ptr->Thread_counter_lock);
	pt_ptr->Thread_counter++;
	DT_Mdep_Unlock(&pt_ptr->Thread_counter_lock);

	DT_Mdep_Thread_Init_Attributes(thread_ptr);

	return thread_ptr;
}

/*
 * Thread destructor
 */
void DT_Thread_Destroy(Thread * thread_ptr, Per_Test_Data_t * pt_ptr)
{
	if (thread_ptr) {
		DT_Mdep_Lock(&pt_ptr->Thread_counter_lock);
		pt_ptr->Thread_counter--;
		DT_Mdep_Unlock(&pt_ptr->Thread_counter_lock);

		DT_Mdep_Thread_Destroy_Attributes(thread_ptr);
		DT_MemListFree(pt_ptr, thread_ptr);
	}
}

/*
 * Start thread execution NOTE: This routine DOES create a thread in addition
 * to starting it whereas DT_Thread_Create just sets up some data structures.
 * (this is counter-intuitive)
 */
bool DT_Thread_Start(Thread * thread_ptr)
{
	return DT_Mdep_Thread_Start(thread_ptr);
}
