/*	$NetBSD: rf_states.c,v 1.15 2000/10/20 02:24:45 oster Exp $	*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland, William V. Courtright II, Robby Findler
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/errno.h>

#include <dev/raidframe/rf_archs.h>
#include <dev/raidframe/rf_threadstuff.h>
#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_dag.h>
#include <dev/raidframe/rf_desc.h>
#include <dev/raidframe/rf_aselect.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_states.h>
#include <dev/raidframe/rf_dagutils.h>
#include <dev/raidframe/rf_driver.h>
#include <dev/raidframe/rf_engine.h>
#include <dev/raidframe/rf_map.h>
#include <dev/raidframe/rf_etimer.h>
#include <dev/raidframe/rf_kintf.h>

/* prototypes for some of the available states.

   States must:

     - not block.

     - either schedule rf_ContinueRaidAccess as a callback and return
       RF_TRUE, or complete all of their work and return RF_FALSE.

     - increment desc->state when they have finished their work.
*/

static char *
StateName(RF_AccessState_t state)
{
	switch (state) {
		case rf_QuiesceState:return "QuiesceState";
	case rf_MapState:
		return "MapState";
	case rf_LockState:
		return "LockState";
	case rf_CreateDAGState:
		return "CreateDAGState";
	case rf_ExecuteDAGState:
		return "ExecuteDAGState";
	case rf_ProcessDAGState:
		return "ProcessDAGState";
	case rf_CleanupState:
		return "CleanupState";
	case rf_LastState:
		return "LastState";
	case rf_IncrAccessesCountState:
		return "IncrAccessesCountState";
	case rf_DecrAccessesCountState:
		return "DecrAccessesCountState";
	default:
		return "!!! UnnamedState !!!";
	}
}

void 
rf_ContinueRaidAccess(RF_RaidAccessDesc_t * desc)
{
	int     suspended = RF_FALSE;
	int     current_state_index = desc->state;
	RF_AccessState_t current_state = desc->states[current_state_index];
	int     unit = desc->raidPtr->raidid;

	do {

		current_state_index = desc->state;
		current_state = desc->states[current_state_index];

		switch (current_state) {

		case rf_QuiesceState:
			suspended = rf_State_Quiesce(desc);
			break;
		case rf_IncrAccessesCountState:
			suspended = rf_State_IncrAccessCount(desc);
			break;
		case rf_MapState:
			suspended = rf_State_Map(desc);
			break;
		case rf_LockState:
			suspended = rf_State_Lock(desc);
			break;
		case rf_CreateDAGState:
			suspended = rf_State_CreateDAG(desc);
			break;
		case rf_ExecuteDAGState:
			suspended = rf_State_ExecuteDAG(desc);
			break;
		case rf_ProcessDAGState:
			suspended = rf_State_ProcessDAG(desc);
			break;
		case rf_CleanupState:
			suspended = rf_State_Cleanup(desc);
			break;
		case rf_DecrAccessesCountState:
			suspended = rf_State_DecrAccessCount(desc);
			break;
		case rf_LastState:
			suspended = rf_State_LastState(desc);
			break;
		}

		/* after this point, we cannot dereference desc since desc may
		 * have been freed. desc is only freed in LastState, so if we
		 * renter this function or loop back up, desc should be valid. */

		if (rf_printStatesDebug) {
			printf("raid%d: State: %-24s StateIndex: %3i desc: 0x%ld %s\n",
			       unit, StateName(current_state), 
			       current_state_index, (long) desc,
			       suspended ? "callback scheduled" : "looping");
		}
	} while (!suspended && current_state != rf_LastState);

	return;
}


void 
rf_ContinueDagAccess(RF_DagList_t * dagList)
{
	RF_AccTraceEntry_t *tracerec = &(dagList->desc->tracerec);
	RF_RaidAccessDesc_t *desc;
	RF_DagHeader_t *dag_h;
	RF_Etimer_t timer;
	int     i;

	desc = dagList->desc;

	timer = tracerec->timer;
	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	tracerec->specific.user.exec_us = RF_ETIMER_VAL_US(timer);
	RF_ETIMER_START(tracerec->timer);

	/* skip to dag which just finished */
	dag_h = dagList->dags;
	for (i = 0; i < dagList->numDagsDone; i++) {
		dag_h = dag_h->next;
	}

	/* check to see if retry is required */
	if (dag_h->status == rf_rollBackward) {
		/* when a dag fails, mark desc status as bad and allow all
		 * other dags in the desc to execute to completion.  then,
		 * free all dags and start over */
		desc->status = 1;	/* bad status */
		{
			printf("raid%d: DAG failure: %c addr 0x%lx (%ld) nblk 0x%x (%d) buf 0x%lx\n",
			       desc->raidPtr->raidid, desc->type, 
			       (long) desc->raidAddress,
			       (long) desc->raidAddress, (int) desc->numBlocks,
			       (int) desc->numBlocks, 
			       (unsigned long) (desc->bufPtr));
		}
	}
	dagList->numDagsDone++;
	rf_ContinueRaidAccess(desc);
}

int 
rf_State_LastState(RF_RaidAccessDesc_t * desc)
{
	void    (*callbackFunc) (RF_CBParam_t) = desc->callbackFunc;
	RF_CBParam_t callbackArg;

	callbackArg.p = desc->callbackArg;
	
	/*
	 * If this is not an async request, wake up the caller
	 */
	if (desc->async_flag == 0)
		wakeup(desc->bp);

	/*
	 * That's all the IO for this one... unbusy the 'disk'.
	 */

	rf_disk_unbusy(desc);

	/* 
	 * Wakeup any requests waiting to go.
	 */
	
	RF_LOCK_MUTEX(((RF_Raid_t *) desc->raidPtr)->mutex);
	((RF_Raid_t *) desc->raidPtr)->openings++;
	RF_UNLOCK_MUTEX(((RF_Raid_t *) desc->raidPtr)->mutex);

	/* wake up any pending IO */
	raidstart(((RF_Raid_t *) desc->raidPtr));
		
	/* printf("Calling biodone on 0x%x\n",desc->bp); */
	biodone(desc->bp);	/* access came through ioctl */

	if (callbackFunc)
		callbackFunc(callbackArg);
	rf_FreeRaidAccDesc(desc);

	return RF_FALSE;
}

int 
rf_State_IncrAccessCount(RF_RaidAccessDesc_t * desc)
{
	RF_Raid_t *raidPtr;

	raidPtr = desc->raidPtr;
	/* Bummer. We have to do this to be 100% safe w.r.t. the increment
	 * below */
	RF_LOCK_MUTEX(raidPtr->access_suspend_mutex);
	raidPtr->accs_in_flight++;	/* used to detect quiescence */
	RF_UNLOCK_MUTEX(raidPtr->access_suspend_mutex);

	desc->state++;
	return RF_FALSE;
}

int 
rf_State_DecrAccessCount(RF_RaidAccessDesc_t * desc)
{
	RF_Raid_t *raidPtr;

	raidPtr = desc->raidPtr;

	RF_LOCK_MUTEX(raidPtr->access_suspend_mutex);
	raidPtr->accs_in_flight--;
	if (raidPtr->accesses_suspended && raidPtr->accs_in_flight == 0) {
		rf_SignalQuiescenceLock(raidPtr, raidPtr->reconDesc);
	}
	rf_UpdateUserStats(raidPtr, RF_ETIMER_VAL_US(desc->timer), desc->numBlocks);
	RF_UNLOCK_MUTEX(raidPtr->access_suspend_mutex);

	desc->state++;
	return RF_FALSE;
}

int 
rf_State_Quiesce(RF_RaidAccessDesc_t * desc)
{
	RF_AccTraceEntry_t *tracerec = &desc->tracerec;
	RF_Etimer_t timer;
	int     suspended = RF_FALSE;
	RF_Raid_t *raidPtr;

	raidPtr = desc->raidPtr;

	RF_ETIMER_START(timer);
	RF_ETIMER_START(desc->timer);

	RF_LOCK_MUTEX(raidPtr->access_suspend_mutex);
	if (raidPtr->accesses_suspended) {
		RF_CallbackDesc_t *cb;
		cb = rf_AllocCallbackDesc();
		/* XXX the following cast is quite bogus...
		 * rf_ContinueRaidAccess takes a (RF_RaidAccessDesc_t *) as an
		 * argument..  GO */
		cb->callbackFunc = (void (*) (RF_CBParam_t)) rf_ContinueRaidAccess;
		cb->callbackArg.p = (void *) desc;
		cb->next = raidPtr->quiesce_wait_list;
		raidPtr->quiesce_wait_list = cb;
		suspended = RF_TRUE;
	}
	RF_UNLOCK_MUTEX(raidPtr->access_suspend_mutex);

	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	tracerec->specific.user.suspend_ovhd_us += RF_ETIMER_VAL_US(timer);

	if (suspended && rf_quiesceDebug)
		printf("Stalling access due to quiescence lock\n");

	desc->state++;
	return suspended;
}

int 
rf_State_Map(RF_RaidAccessDesc_t * desc)
{
	RF_Raid_t *raidPtr = desc->raidPtr;
	RF_AccTraceEntry_t *tracerec = &desc->tracerec;
	RF_Etimer_t timer;

	RF_ETIMER_START(timer);

	if (!(desc->asmap = rf_MapAccess(raidPtr, desc->raidAddress, desc->numBlocks,
		    desc->bufPtr, RF_DONT_REMAP)))
		RF_PANIC();

	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	tracerec->specific.user.map_us = RF_ETIMER_VAL_US(timer);

	desc->state++;
	return RF_FALSE;
}

int 
rf_State_Lock(RF_RaidAccessDesc_t * desc)
{
	RF_AccTraceEntry_t *tracerec = &desc->tracerec;
	RF_Raid_t *raidPtr = desc->raidPtr;
	RF_AccessStripeMapHeader_t *asmh = desc->asmap;
	RF_AccessStripeMap_t *asm_p;
	RF_Etimer_t timer;
	int     suspended = RF_FALSE;

	RF_ETIMER_START(timer);
	if (!(raidPtr->Layout.map->flags & RF_NO_STRIPE_LOCKS)) {
		RF_StripeNum_t lastStripeID = -1;

		/* acquire each lock that we don't already hold */
		for (asm_p = asmh->stripeMap; asm_p; asm_p = asm_p->next) {
			RF_ASSERT(RF_IO_IS_R_OR_W(desc->type));
			if (!rf_suppressLocksAndLargeWrites &&
			    asm_p->parityInfo &&
			    !(desc->flags & RF_DAG_SUPPRESS_LOCKS) &&
			    !(asm_p->flags & RF_ASM_FLAGS_LOCK_TRIED)) {
				asm_p->flags |= RF_ASM_FLAGS_LOCK_TRIED;
				RF_ASSERT(asm_p->stripeID > lastStripeID);

				/* locks must be acquired hierarchically */

				lastStripeID = asm_p->stripeID;
				/* XXX the cast to (void (*)(RF_CBParam_t))
				 * below is bogus!  GO */
				RF_INIT_LOCK_REQ_DESC(asm_p->lockReqDesc,
				    desc->type,
				    (void (*) (RF_Buf_t)) rf_ContinueRaidAccess,
				    desc, asm_p,
				    raidPtr->Layout.dataSectorsPerStripe);
				if (rf_AcquireStripeLock(raidPtr->lockTable,
				    asm_p->stripeID, &asm_p->lockReqDesc)) {
					suspended = RF_TRUE;
					break;
				}
			}
			if (desc->type == RF_IO_TYPE_WRITE &&
			    raidPtr->status[asm_p->physInfo->row] == rf_rs_reconstructing) {
				if (!(asm_p->flags & RF_ASM_FLAGS_FORCE_TRIED)) {
					int     val;

					asm_p->flags |= RF_ASM_FLAGS_FORCE_TRIED;
					/* XXX the cast below is quite
					 * bogus!!! XXX  GO */
					val = rf_ForceOrBlockRecon(raidPtr, asm_p,
					    (void (*) (RF_Raid_t *, void *)) rf_ContinueRaidAccess, desc);
					if (val == 0) {
						asm_p->flags |= RF_ASM_FLAGS_RECON_BLOCKED;
					} else {
						suspended = RF_TRUE;
						break;
					}
				} else {
					if (rf_pssDebug) {
						printf("raid%d: skipping force/block because already done, psid %ld\n",
						       desc->raidPtr->raidid, 
						       (long) asm_p->stripeID);
					}
				}
			} else {
				if (rf_pssDebug) {
					printf("raid%d: skipping force/block because not write or not under recon, psid %ld\n",
					       desc->raidPtr->raidid, 
					       (long) asm_p->stripeID);
				}
			}
		}

		RF_ETIMER_STOP(timer);
		RF_ETIMER_EVAL(timer);
		tracerec->specific.user.lock_us += RF_ETIMER_VAL_US(timer);

		if (suspended)
			return (RF_TRUE);
	}
	desc->state++;
	return (RF_FALSE);
}
/*
 * the following three states create, execute, and post-process dags
 * the error recovery unit is a single dag.
 * by default, SelectAlgorithm creates an array of dags, one per parity stripe
 * in some tricky cases, multiple dags per stripe are created
 *   - dags within a parity stripe are executed sequentially (arbitrary order)
 *   - dags for distinct parity stripes are executed concurrently
 *
 * repeat until all dags complete successfully -or- dag selection fails
 *
 * while !done
 *   create dag(s) (SelectAlgorithm)
 *   if dag
 *     execute dag (DispatchDAG)
 *     if dag successful
 *       done (SUCCESS)
 *     else
 *       !done (RETRY - start over with new dags)
 *   else
 *     done (FAIL)
 */
int 
rf_State_CreateDAG(RF_RaidAccessDesc_t * desc)
{
	RF_AccTraceEntry_t *tracerec = &desc->tracerec;
	RF_Etimer_t timer;
	RF_DagHeader_t *dag_h;
	int     i, selectStatus;

	/* generate a dag for the access, and fire it off.  When the dag
	 * completes, we'll get re-invoked in the next state. */
	RF_ETIMER_START(timer);
	/* SelectAlgorithm returns one or more dags */
	selectStatus = rf_SelectAlgorithm(desc, desc->flags | RF_DAG_SUPPRESS_LOCKS);
	if (rf_printDAGsDebug)
		for (i = 0; i < desc->numStripes; i++)
			rf_PrintDAGList(desc->dagArray[i].dags);
	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	/* update time to create all dags */
	tracerec->specific.user.dag_create_us = RF_ETIMER_VAL_US(timer);

	desc->status = 0;	/* good status */

	if (selectStatus) {
		/* failed to create a dag */
		/* this happens when there are too many faults or incomplete
		 * dag libraries */
		printf("[Failed to create a DAG]\n");
		RF_PANIC();
	} else {
		/* bind dags to desc */
		for (i = 0; i < desc->numStripes; i++) {
			dag_h = desc->dagArray[i].dags;
			while (dag_h) {
				dag_h->bp = (RF_Buf_t) desc->bp;
				dag_h->tracerec = tracerec;
				dag_h = dag_h->next;
			}
		}
		desc->flags |= RF_DAG_DISPATCH_RETURNED;
		desc->state++;	/* next state should be rf_State_ExecuteDAG */
	}
	return RF_FALSE;
}



/* the access has an array of dagLists, one dagList per parity stripe.
 * fire the first dag in each parity stripe (dagList).
 * dags within a stripe (dagList) must be executed sequentially
 *  - this preserves atomic parity update
 * dags for independents parity groups (stripes) are fired concurrently */

int 
rf_State_ExecuteDAG(RF_RaidAccessDesc_t * desc)
{
	int     i;
	RF_DagHeader_t *dag_h;
	RF_DagList_t *dagArray = desc->dagArray;

	/* next state is always rf_State_ProcessDAG important to do this
	 * before firing the first dag (it may finish before we leave this
	 * routine) */
	desc->state++;

	/* sweep dag array, a stripe at a time, firing the first dag in each
	 * stripe */
	for (i = 0; i < desc->numStripes; i++) {
		RF_ASSERT(dagArray[i].numDags > 0);
		RF_ASSERT(dagArray[i].numDagsDone == 0);
		RF_ASSERT(dagArray[i].numDagsFired == 0);
		RF_ETIMER_START(dagArray[i].tracerec.timer);
		/* fire first dag in this stripe */
		dag_h = dagArray[i].dags;
		RF_ASSERT(dag_h);
		dagArray[i].numDagsFired++;
		/* XXX Yet another case where we pass in a conflicting
		 * function pointer :-(  XXX  GO */
		rf_DispatchDAG(dag_h, (void (*) (void *)) rf_ContinueDagAccess, &dagArray[i]);
	}

	/* the DAG will always call the callback, even if there was no
	 * blocking, so we are always suspended in this state */
	return RF_TRUE;
}



/* rf_State_ProcessDAG is entered when a dag completes.
 * first, check to all dags in the access have completed
 * if not, fire as many dags as possible */

int 
rf_State_ProcessDAG(RF_RaidAccessDesc_t * desc)
{
	RF_AccessStripeMapHeader_t *asmh = desc->asmap;
	RF_Raid_t *raidPtr = desc->raidPtr;
	RF_DagHeader_t *dag_h;
	int     i, j, done = RF_TRUE;
	RF_DagList_t *dagArray = desc->dagArray;
	RF_Etimer_t timer;

	/* check to see if this is the last dag */
	for (i = 0; i < desc->numStripes; i++)
		if (dagArray[i].numDags != dagArray[i].numDagsDone)
			done = RF_FALSE;

	if (done) {
		if (desc->status) {
			/* a dag failed, retry */
			RF_ETIMER_START(timer);
			/* free all dags */
			for (i = 0; i < desc->numStripes; i++) {
				rf_FreeDAG(desc->dagArray[i].dags);
			}
			rf_MarkFailuresInASMList(raidPtr, asmh);
			/* back up to rf_State_CreateDAG */
			desc->state = desc->state - 2;
			return RF_FALSE;
		} else {
			/* move on to rf_State_Cleanup */
			desc->state++;
		}
		return RF_FALSE;
	} else {
		/* more dags to execute */
		/* see if any are ready to be fired.  if so, fire them */
		/* don't fire the initial dag in a list, it's fired in
		 * rf_State_ExecuteDAG */
		for (i = 0; i < desc->numStripes; i++) {
			if ((dagArray[i].numDagsDone < dagArray[i].numDags)
			    && (dagArray[i].numDagsDone == dagArray[i].numDagsFired)
			    && (dagArray[i].numDagsFired > 0)) {
				RF_ETIMER_START(dagArray[i].tracerec.timer);
				/* fire next dag in this stripe */
				/* first, skip to next dag awaiting execution */
				dag_h = dagArray[i].dags;
				for (j = 0; j < dagArray[i].numDagsDone; j++)
					dag_h = dag_h->next;
				dagArray[i].numDagsFired++;
				/* XXX and again we pass a different function
				 * pointer.. GO */
				rf_DispatchDAG(dag_h, (void (*) (void *)) rf_ContinueDagAccess,
				    &dagArray[i]);
			}
		}
		return RF_TRUE;
	}
}
/* only make it this far if all dags complete successfully */
int 
rf_State_Cleanup(RF_RaidAccessDesc_t * desc)
{
	RF_AccTraceEntry_t *tracerec = &desc->tracerec;
	RF_AccessStripeMapHeader_t *asmh = desc->asmap;
	RF_Raid_t *raidPtr = desc->raidPtr;
	RF_AccessStripeMap_t *asm_p;
	RF_DagHeader_t *dag_h;
	RF_Etimer_t timer;
	int i;

	desc->state++;

	timer = tracerec->timer;
	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	tracerec->specific.user.dag_retry_us = RF_ETIMER_VAL_US(timer);

	/* the RAID I/O is complete.  Clean up. */
	tracerec->specific.user.dag_retry_us = 0;

	RF_ETIMER_START(timer);
	if (desc->flags & RF_DAG_RETURN_DAG) {
		/* copy dags into paramDAG */
		*(desc->paramDAG) = desc->dagArray[0].dags;
		dag_h = *(desc->paramDAG);
		for (i = 1; i < desc->numStripes; i++) {
			/* concatenate dags from remaining stripes */
			RF_ASSERT(dag_h);
			while (dag_h->next)
				dag_h = dag_h->next;
			dag_h->next = desc->dagArray[i].dags;
		}
	} else {
		/* free all dags */
		for (i = 0; i < desc->numStripes; i++) {
			rf_FreeDAG(desc->dagArray[i].dags);
		}
	}

	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	tracerec->specific.user.cleanup_us = RF_ETIMER_VAL_US(timer);

	RF_ETIMER_START(timer);
	if (!(raidPtr->Layout.map->flags & RF_NO_STRIPE_LOCKS)) {
		for (asm_p = asmh->stripeMap; asm_p; asm_p = asm_p->next) {
			if (!rf_suppressLocksAndLargeWrites &&
			    asm_p->parityInfo &&
			    !(desc->flags & RF_DAG_SUPPRESS_LOCKS)) {
				RF_ASSERT_VALID_LOCKREQ(&asm_p->lockReqDesc);
				rf_ReleaseStripeLock(raidPtr->lockTable, 
						     asm_p->stripeID,
						     &asm_p->lockReqDesc);
			}
			if (asm_p->flags & RF_ASM_FLAGS_RECON_BLOCKED) {
				rf_UnblockRecon(raidPtr, asm_p);
			}
		}
	}
	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	tracerec->specific.user.lock_us += RF_ETIMER_VAL_US(timer);

	RF_ETIMER_START(timer);
	if (desc->flags & RF_DAG_RETURN_ASM)
		*(desc->paramASM) = asmh;
	else
		rf_FreeAccessStripeMap(asmh);
	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	tracerec->specific.user.cleanup_us += RF_ETIMER_VAL_US(timer);

	RF_ETIMER_STOP(desc->timer);
	RF_ETIMER_EVAL(desc->timer);

	timer = desc->tracerec.tot_timer;
	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	desc->tracerec.total_us = RF_ETIMER_VAL_US(timer);

	rf_LogTraceRec(raidPtr, tracerec);

	desc->flags |= RF_DAG_ACCESS_COMPLETE;

	return RF_FALSE;
}
