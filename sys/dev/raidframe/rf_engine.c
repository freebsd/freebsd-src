/*	$NetBSD: rf_engine.c,v 1.10 2000/08/20 16:51:03 thorpej Exp $	*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: William V. Courtright II, Mark Holland, Rachad Youssef
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

/****************************************************************************
 *                                                                          *
 * engine.c -- code for DAG execution engine                                *
 *                                                                          *
 * Modified to work as follows (holland):                                   *
 *   A user-thread calls into DispatchDAG, which fires off the nodes that   *
 *   are direct successors to the header node.  DispatchDAG then returns,   *
 *   and the rest of the I/O continues asynchronously.  As each node        *
 *   completes, the node execution function calls FinishNode().  FinishNode *
 *   scans the list of successors to the node and increments the antecedent *
 *   counts.  Each node that becomes enabled is placed on a central node    *
 *   queue.  A dedicated dag-execution thread grabs nodes off of this       *
 *   queue and fires them.                                                  *
 *                                                                          *
 *   NULL nodes are never fired.                                            *
 *                                                                          *
 *   Terminator nodes are never fired, but rather cause the callback        *
 *   associated with the DAG to be invoked.                                 *
 *                                                                          *
 *   If a node fails, the dag either rolls forward to the completion or     *
 *   rolls back, undoing previously-completed nodes and fails atomically.   *
 *   The direction of recovery is determined by the location of the failed  *
 *   node in the graph.  If the failure occured before the commit node in   *
 *   the graph, backward recovery is used.  Otherwise, forward recovery is  *
 *   used.                                                                  *
 *                                                                          *
 ****************************************************************************/

#include <dev/raidframe/rf_threadstuff.h>

#include <sys/errno.h>

#include <dev/raidframe/rf_dag.h>
#include <dev/raidframe/rf_engine.h>
#include <dev/raidframe/rf_etimer.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_dagutils.h>
#include <dev/raidframe/rf_shutdown.h>
#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_kintf.h>

static void DAGExecutionThread(RF_ThreadArg_t arg);

#define DO_INIT(_l_,_r_) { \
  int _rc; \
  _rc = rf_create_managed_mutex(_l_,&(_r_)->node_queue_mutex); \
  if (_rc) { \
    return(_rc); \
  } \
  _rc = rf_create_managed_cond(_l_,&(_r_)->node_queue_cond); \
  if (_rc) { \
    return(_rc); \
  } \
}

/* synchronization primitives for this file.  DO_WAIT should be enclosed in a while loop. */

/*
 * XXX Is this spl-ing really necessary?
 */
#define DO_LOCK(_r_) \
do { \
	ks = splbio(); \
	RF_LOCK_MUTEX((_r_)->node_queue_mutex); \
} while (0)

#define DO_UNLOCK(_r_) \
do { \
	RF_UNLOCK_MUTEX((_r_)->node_queue_mutex); \
	splx(ks); \
} while (0)

#define	DO_WAIT(_r_) \
	RF_WAIT_COND((_r_)->node_queue, (_r_)->node_queue_mutex)

#define	DO_SIGNAL(_r_) \
	RF_BROADCAST_COND((_r_)->node_queue)	/* XXX RF_SIGNAL_COND? */

static void rf_ShutdownEngine(void *);

static void 
rf_ShutdownEngine(arg)
	void   *arg;
{
	RF_Raid_t *raidPtr;

	raidPtr = (RF_Raid_t *) arg;
	raidPtr->shutdown_engine = 1;
	DO_SIGNAL(raidPtr);
}

int 
rf_ConfigureEngine(
    RF_ShutdownList_t ** listp,
    RF_Raid_t * raidPtr,
    RF_Config_t * cfgPtr)
{
	int     rc;

	DO_INIT(listp, raidPtr);

	raidPtr->node_queue = NULL;
	raidPtr->dags_in_flight = 0;

	rc = rf_init_managed_threadgroup(listp, &raidPtr->engine_tg);
	if (rc)
		return (rc);

	/* we create the execution thread only once per system boot. no need
	 * to check return code b/c the kernel panics if it can't create the
	 * thread. */
	if (rf_engineDebug) {
		printf("raid%d: Creating engine thread\n", raidPtr->raidid);
	}
	if (RF_CREATE_THREAD(raidPtr->engine_thread, DAGExecutionThread, raidPtr,"raid")) {
		RF_ERRORMSG("RAIDFRAME: Unable to create engine thread\n");
		return (ENOMEM);
	}
	if (rf_engineDebug) {
		printf("raid%d: Created engine thread\n", raidPtr->raidid);
	}
	RF_THREADGROUP_STARTED(&raidPtr->engine_tg);
	/* XXX something is missing here... */
#ifdef debug
	printf("Skipping the WAIT_START!!\n");
#endif
#if 1
	printf("Waiting for DAG engine to start\n");
	RF_THREADGROUP_WAIT_START(&raidPtr->engine_tg);
#endif
	/* engine thread is now running and waiting for work */
	if (rf_engineDebug) {
		printf("raid%d: Engine thread running and waiting for events\n", raidPtr->raidid);
	}
	rc = rf_ShutdownCreate(listp, rf_ShutdownEngine, raidPtr);
	if (rc) {
		RF_ERRORMSG3("Unable to add to shutdown list file %s line %d rc=%d\n", __FILE__,
		    __LINE__, rc);
		rf_ShutdownEngine(NULL);
	}
	return (rc);
}

static int 
BranchDone(RF_DagNode_t * node)
{
	int     i;

	/* return true if forward execution is completed for a node and it's
	 * succedents */
	switch (node->status) {
	case rf_wait:
		/* should never be called in this state */
		RF_PANIC();
		break;
	case rf_fired:
		/* node is currently executing, so we're not done */
		return (RF_FALSE);
	case rf_good:
		for (i = 0; i < node->numSuccedents; i++)	/* for each succedent */
			if (!BranchDone(node->succedents[i]))	/* recursively check
								 * branch */
				return RF_FALSE;
		return RF_TRUE;	/* node and all succedent branches aren't in
				 * fired state */
		break;
	case rf_bad:
		/* succedents can't fire */
		return (RF_TRUE);
	case rf_recover:
		/* should never be called in this state */
		RF_PANIC();
		break;
	case rf_undone:
	case rf_panic:
		/* XXX need to fix this case */
		/* for now, assume that we're done */
		return (RF_TRUE);
		break;
	default:
		/* illegal node status */
		RF_PANIC();
		break;
	}
}

static int 
NodeReady(RF_DagNode_t * node)
{
	int     ready;

	switch (node->dagHdr->status) {
	case rf_enable:
	case rf_rollForward:
		if ((node->status == rf_wait) && (node->numAntecedents == node->numAntDone))
			ready = RF_TRUE;
		else
			ready = RF_FALSE;
		break;
	case rf_rollBackward:
		RF_ASSERT(node->numSuccDone <= node->numSuccedents);
		RF_ASSERT(node->numSuccFired <= node->numSuccedents);
		RF_ASSERT(node->numSuccFired <= node->numSuccDone);
		if ((node->status == rf_good) && (node->numSuccDone == node->numSuccedents))
			ready = RF_TRUE;
		else
			ready = RF_FALSE;
		break;
	default:
		printf("Execution engine found illegal DAG status in NodeReady\n");
		RF_PANIC();
		break;
	}

	return (ready);
}



/* user context and dag-exec-thread context:
 * Fire a node.  The node's status field determines which function, do or undo,
 * to be fired.
 * This routine assumes that the node's status field has alread been set to
 * "fired" or "recover" to indicate the direction of execution.
 */
static void 
FireNode(RF_DagNode_t * node)
{
	switch (node->status) {
	case rf_fired:
		/* fire the do function of a node */
		if (rf_engineDebug) {
			printf("raid%d: Firing node 0x%lx (%s)\n", 
			       node->dagHdr->raidPtr->raidid, 
			       (unsigned long) node, node->name);
		}
		if (node->flags & RF_DAGNODE_FLAG_YIELD) {
#if defined(__NetBSD__) || defined(__FreeBSD__) && defined(_KERNEL)
			/* thread_block(); */
			/* printf("Need to block the thread here...\n");  */
			/* XXX thread_block is actually mentioned in
			 * /usr/include/vm/vm_extern.h */
#else
			thread_block();
#endif
		}
		(*(node->doFunc)) (node);
		break;
	case rf_recover:
		/* fire the undo function of a node */
		if (rf_engineDebug) {
			printf("raid%d: Firing (undo) node 0x%lx (%s)\n", 
			       node->dagHdr->raidPtr->raidid,
			       (unsigned long) node, node->name);
		}
		if (node->flags & RF_DAGNODE_FLAG_YIELD)
#if defined(__NetBSD__) || defined(__FreeBSD__) && defined(_KERNEL)
			/* thread_block(); */
			/* printf("Need to block the thread here...\n"); */
			/* XXX thread_block is actually mentioned in
			 * /usr/include/vm/vm_extern.h */
#else
			thread_block();
#endif
		(*(node->undoFunc)) (node);
		break;
	default:
		RF_PANIC();
		break;
	}
}



/* user context:
 * Attempt to fire each node in a linear array.
 * The entire list is fired atomically.
 */
static void 
FireNodeArray(
    int numNodes,
    RF_DagNode_t ** nodeList)
{
	RF_DagStatus_t dstat;
	RF_DagNode_t *node;
	int     i, j;

	/* first, mark all nodes which are ready to be fired */
	for (i = 0; i < numNodes; i++) {
		node = nodeList[i];
		dstat = node->dagHdr->status;
		RF_ASSERT((node->status == rf_wait) || (node->status == rf_good));
		if (NodeReady(node)) {
			if ((dstat == rf_enable) || (dstat == rf_rollForward)) {
				RF_ASSERT(node->status == rf_wait);
				if (node->commitNode)
					node->dagHdr->numCommits++;
				node->status = rf_fired;
				for (j = 0; j < node->numAntecedents; j++)
					node->antecedents[j]->numSuccFired++;
			} else {
				RF_ASSERT(dstat == rf_rollBackward);
				RF_ASSERT(node->status == rf_good);
				RF_ASSERT(node->commitNode == RF_FALSE);	/* only one commit node
										 * per graph */
				node->status = rf_recover;
			}
		}
	}
	/* now, fire the nodes */
	for (i = 0; i < numNodes; i++) {
		if ((nodeList[i]->status == rf_fired) || (nodeList[i]->status == rf_recover))
			FireNode(nodeList[i]);
	}
}


/* user context:
 * Attempt to fire each node in a linked list.
 * The entire list is fired atomically.
 */
static void 
FireNodeList(RF_DagNode_t * nodeList)
{
	RF_DagNode_t *node, *next;
	RF_DagStatus_t dstat;
	int     j;

	if (nodeList) {
		/* first, mark all nodes which are ready to be fired */
		for (node = nodeList; node; node = next) {
			next = node->next;
			dstat = node->dagHdr->status;
			RF_ASSERT((node->status == rf_wait) || (node->status == rf_good));
			if (NodeReady(node)) {
				if ((dstat == rf_enable) || (dstat == rf_rollForward)) {
					RF_ASSERT(node->status == rf_wait);
					if (node->commitNode)
						node->dagHdr->numCommits++;
					node->status = rf_fired;
					for (j = 0; j < node->numAntecedents; j++)
						node->antecedents[j]->numSuccFired++;
				} else {
					RF_ASSERT(dstat == rf_rollBackward);
					RF_ASSERT(node->status == rf_good);
					RF_ASSERT(node->commitNode == RF_FALSE);	/* only one commit node
											 * per graph */
					node->status = rf_recover;
				}
			}
		}
		/* now, fire the nodes */
		for (node = nodeList; node; node = next) {
			next = node->next;
			if ((node->status == rf_fired) || (node->status == rf_recover))
				FireNode(node);
		}
	}
}
/* interrupt context:
 * for each succedent
 *    propagate required results from node to succedent
 *    increment succedent's numAntDone
 *    place newly-enable nodes on node queue for firing
 *
 * To save context switches, we don't place NIL nodes on the node queue,
 * but rather just process them as if they had fired.  Note that NIL nodes
 * that are the direct successors of the header will actually get fired by
 * DispatchDAG, which is fine because no context switches are involved.
 *
 * Important:  when running at user level, this can be called by any
 * disk thread, and so the increment and check of the antecedent count
 * must be locked.  I used the node queue mutex and locked down the
 * entire function, but this is certainly overkill.
 */
static void 
PropagateResults(
    RF_DagNode_t * node,
    int context)
{
	RF_DagNode_t *s, *a;
	RF_Raid_t *raidPtr;
	int     i, ks;
	RF_DagNode_t *finishlist = NULL;	/* a list of NIL nodes to be
						 * finished */
	RF_DagNode_t *skiplist = NULL;	/* list of nodes with failed truedata
					 * antecedents */
	RF_DagNode_t *firelist = NULL;	/* a list of nodes to be fired */
	RF_DagNode_t *q = NULL, *qh = NULL, *next;
	int     j, skipNode;

	raidPtr = node->dagHdr->raidPtr;

	DO_LOCK(raidPtr);

	/* debug - validate fire counts */
	for (i = 0; i < node->numAntecedents; i++) {
		a = *(node->antecedents + i);
		RF_ASSERT(a->numSuccFired >= a->numSuccDone);
		RF_ASSERT(a->numSuccFired <= a->numSuccedents);
		a->numSuccDone++;
	}

	switch (node->dagHdr->status) {
	case rf_enable:
	case rf_rollForward:
		for (i = 0; i < node->numSuccedents; i++) {
			s = *(node->succedents + i);
			RF_ASSERT(s->status == rf_wait);
			(s->numAntDone)++;
			if (s->numAntDone == s->numAntecedents) {
				/* look for NIL nodes */
				if (s->doFunc == rf_NullNodeFunc) {
					/* don't fire NIL nodes, just process
					 * them */
					s->next = finishlist;
					finishlist = s;
				} else {
					/* look to see if the node is to be
					 * skipped */
					skipNode = RF_FALSE;
					for (j = 0; j < s->numAntecedents; j++)
						if ((s->antType[j] == rf_trueData) && (s->antecedents[j]->status == rf_bad))
							skipNode = RF_TRUE;
					if (skipNode) {
						/* this node has one or more
						 * failed true data
						 * dependencies, so skip it */
						s->next = skiplist;
						skiplist = s;
					} else
						/* add s to list of nodes (q)
						 * to execute */
						if (context != RF_INTR_CONTEXT) {
							/* we only have to
							 * enqueue if we're at
							 * intr context */
							s->next = firelist;	/* put node on a list to
										 * be fired after we
										 * unlock */
							firelist = s;
						} else {	/* enqueue the node for
								 * the dag exec thread
								 * to fire */
							RF_ASSERT(NodeReady(s));
							if (q) {
								q->next = s;
								q = s;
							} else {
								qh = q = s;
								qh->next = NULL;
							}
						}
				}
			}
		}

		if (q) {
			/* xfer our local list of nodes to the node queue */
			q->next = raidPtr->node_queue;
			raidPtr->node_queue = qh;
			DO_SIGNAL(raidPtr);
		}
		DO_UNLOCK(raidPtr);

		for (; skiplist; skiplist = next) {
			next = skiplist->next;
			skiplist->status = rf_skipped;
			for (i = 0; i < skiplist->numAntecedents; i++) {
				skiplist->antecedents[i]->numSuccFired++;
			}
			if (skiplist->commitNode) {
				skiplist->dagHdr->numCommits++;
			}
			rf_FinishNode(skiplist, context);
		}
		for (; finishlist; finishlist = next) {
			/* NIL nodes: no need to fire them */
			next = finishlist->next;
			finishlist->status = rf_good;
			for (i = 0; i < finishlist->numAntecedents; i++) {
				finishlist->antecedents[i]->numSuccFired++;
			}
			if (finishlist->commitNode)
				finishlist->dagHdr->numCommits++;
			/*
		         * Okay, here we're calling rf_FinishNode() on nodes that
		         * have the null function as their work proc. Such a node
		         * could be the terminal node in a DAG. If so, it will
		         * cause the DAG to complete, which will in turn free
		         * memory used by the DAG, which includes the node in
		         * question. Thus, we must avoid referencing the node
		         * at all after calling rf_FinishNode() on it.
		         */
			rf_FinishNode(finishlist, context);	/* recursive call */
		}
		/* fire all nodes in firelist */
		FireNodeList(firelist);
		break;

	case rf_rollBackward:
		for (i = 0; i < node->numAntecedents; i++) {
			a = *(node->antecedents + i);
			RF_ASSERT(a->status == rf_good);
			RF_ASSERT(a->numSuccDone <= a->numSuccedents);
			RF_ASSERT(a->numSuccDone <= a->numSuccFired);

			if (a->numSuccDone == a->numSuccFired) {
				if (a->undoFunc == rf_NullNodeFunc) {
					/* don't fire NIL nodes, just process
					 * them */
					a->next = finishlist;
					finishlist = a;
				} else {
					if (context != RF_INTR_CONTEXT) {
						/* we only have to enqueue if
						 * we're at intr context */
						a->next = firelist;	/* put node on a list to
									 * be fired after we
									 * unlock */
						firelist = a;
					} else {	/* enqueue the node for
							 * the dag exec thread
							 * to fire */
						RF_ASSERT(NodeReady(a));
						if (q) {
							q->next = a;
							q = a;
						} else {
							qh = q = a;
							qh->next = NULL;
						}
					}
				}
			}
		}
		if (q) {
			/* xfer our local list of nodes to the node queue */
			q->next = raidPtr->node_queue;
			raidPtr->node_queue = qh;
			DO_SIGNAL(raidPtr);
		}
		DO_UNLOCK(raidPtr);
		for (; finishlist; finishlist = next) {	/* NIL nodes: no need to
							 * fire them */
			next = finishlist->next;
			finishlist->status = rf_good;
			/*
		         * Okay, here we're calling rf_FinishNode() on nodes that
		         * have the null function as their work proc. Such a node
		         * could be the first node in a DAG. If so, it will
		         * cause the DAG to complete, which will in turn free
		         * memory used by the DAG, which includes the node in
		         * question. Thus, we must avoid referencing the node
		         * at all after calling rf_FinishNode() on it.
		         */
			rf_FinishNode(finishlist, context);	/* recursive call */
		}
		/* fire all nodes in firelist */
		FireNodeList(firelist);

		break;
	default:
		printf("Engine found illegal DAG status in PropagateResults()\n");
		RF_PANIC();
		break;
	}
}



/*
 * Process a fired node which has completed
 */
static void 
ProcessNode(
    RF_DagNode_t * node,
    int context)
{
	RF_Raid_t *raidPtr;

	raidPtr = node->dagHdr->raidPtr;

	switch (node->status) {
	case rf_good:
		/* normal case, don't need to do anything */
		break;
	case rf_bad:
		if ((node->dagHdr->numCommits > 0) || (node->dagHdr->numCommitNodes == 0)) {
			node->dagHdr->status = rf_rollForward;	/* crossed commit
								 * barrier */
			if (rf_engineDebug || 1) {
				printf("raid%d: node (%s) returned fail, rolling forward\n", raidPtr->raidid, node->name);
			}
		} else {
			node->dagHdr->status = rf_rollBackward;	/* never reached commit
								 * barrier */
			if (rf_engineDebug || 1) {
				printf("raid%d: node (%s) returned fail, rolling backward\n", raidPtr->raidid, node->name);
			}
		}
		break;
	case rf_undone:
		/* normal rollBackward case, don't need to do anything */
		break;
	case rf_panic:
		/* an undo node failed!!! */
		printf("UNDO of a node failed!!!/n");
		break;
	default:
		printf("node finished execution with an illegal status!!!\n");
		RF_PANIC();
		break;
	}

	/* enqueue node's succedents (antecedents if rollBackward) for
	 * execution */
	PropagateResults(node, context);
}



/* user context or dag-exec-thread context:
 * This is the first step in post-processing a newly-completed node.
 * This routine is called by each node execution function to mark the node
 * as complete and fire off any successors that have been enabled.
 */
int 
rf_FinishNode(
    RF_DagNode_t * node,
    int context)
{
	/* as far as I can tell, retcode is not used -wvcii */
	int     retcode = RF_FALSE;
	node->dagHdr->numNodesCompleted++;
	ProcessNode(node, context);

	return (retcode);
}


/* user context:
 * submit dag for execution, return non-zero if we have to wait for completion.
 * if and only if we return non-zero, we'll cause cbFunc to get invoked with
 * cbArg when the DAG has completed.
 *
 * for now we always return 1.  If the DAG does not cause any I/O, then the callback
 * may get invoked before DispatchDAG returns.  There's code in state 5 of ContinueRaidAccess
 * to handle this.
 *
 * All we do here is fire the direct successors of the header node.  The
 * DAG execution thread does the rest of the dag processing.
 */
int 
rf_DispatchDAG(
    RF_DagHeader_t * dag,
    void (*cbFunc) (void *),
    void *cbArg)
{
	RF_Raid_t *raidPtr;

	raidPtr = dag->raidPtr;
	if (dag->tracerec) {
		RF_ETIMER_START(dag->tracerec->timer);
	}
	if (rf_engineDebug || rf_validateDAGDebug) {
		if (rf_ValidateDAG(dag))
			RF_PANIC();
	}
	if (rf_engineDebug) {
		printf("raid%d: Entering DispatchDAG\n", raidPtr->raidid);
	}
	raidPtr->dags_in_flight++;	/* debug only:  blow off proper
					 * locking */
	dag->cbFunc = cbFunc;
	dag->cbArg = cbArg;
	dag->numNodesCompleted = 0;
	dag->status = rf_enable;
	FireNodeArray(dag->numSuccedents, dag->succedents);
	return (1);
}
/* dedicated kernel thread:
 * the thread that handles all DAG node firing.
 * To minimize locking and unlocking, we grab a copy of the entire node queue and then set the
 * node queue to NULL before doing any firing of nodes.  This way we only have to release the
 * lock once.  Of course, it's probably rare that there's more than one node in the queue at
 * any one time, but it sometimes happens.
 *
 * In the kernel, this thread runs at spl0 and is not swappable.  I copied these
 * characteristics from the aio_completion_thread.
 */

static void 
DAGExecutionThread(RF_ThreadArg_t arg)
{
	RF_DagNode_t *nd, *local_nq, *term_nq, *fire_nq;
	RF_Raid_t *raidPtr;
	int     ks;

	raidPtr = (RF_Raid_t *) arg;

	if (rf_engineDebug) {
		printf("raid%d: Engine thread is running\n", raidPtr->raidid);
	}

	mtx_lock(&Giant);

	RF_THREADGROUP_RUNNING(&raidPtr->engine_tg);

	DO_LOCK(raidPtr);
	while (!raidPtr->shutdown_engine) {

		while (raidPtr->node_queue != NULL) {
			local_nq = raidPtr->node_queue;
			fire_nq = NULL;
			term_nq = NULL;
			raidPtr->node_queue = NULL;
			DO_UNLOCK(raidPtr);

			/* first, strip out the terminal nodes */
			while (local_nq) {
				nd = local_nq;
				local_nq = local_nq->next;
				switch (nd->dagHdr->status) {
				case rf_enable:
				case rf_rollForward:
					if (nd->numSuccedents == 0) {
						/* end of the dag, add to
						 * callback list */
						nd->next = term_nq;
						term_nq = nd;
					} else {
						/* not the end, add to the
						 * fire queue */
						nd->next = fire_nq;
						fire_nq = nd;
					}
					break;
				case rf_rollBackward:
					if (nd->numAntecedents == 0) {
						/* end of the dag, add to the
						 * callback list */
						nd->next = term_nq;
						term_nq = nd;
					} else {
						/* not the end, add to the
						 * fire queue */
						nd->next = fire_nq;
						fire_nq = nd;
					}
					break;
				default:
					RF_PANIC();
					break;
				}
			}

			/* execute callback of dags which have reached the
			 * terminal node */
			while (term_nq) {
				nd = term_nq;
				term_nq = term_nq->next;
				nd->next = NULL;
				(nd->dagHdr->cbFunc) (nd->dagHdr->cbArg);
				raidPtr->dags_in_flight--;	/* debug only */
			}

			/* fire remaining nodes */
			FireNodeList(fire_nq);

			DO_LOCK(raidPtr);
		}
		while (!raidPtr->shutdown_engine && raidPtr->node_queue == NULL)
			DO_WAIT(raidPtr);
	}
	DO_UNLOCK(raidPtr);

	RF_THREADGROUP_DONE(&raidPtr->engine_tg);

	RF_THREAD_EXIT(0);
}
