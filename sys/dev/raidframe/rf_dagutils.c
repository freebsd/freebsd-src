/*	$NetBSD: rf_dagutils.c,v 1.6 1999/12/09 02:26:09 oster Exp $	*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Mark Holland, William V. Courtright II, Jim Zelenka
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

/******************************************************************************
 *
 * rf_dagutils.c -- utility routines for manipulating dags
 *
 *****************************************************************************/

#include <dev/raidframe/rf_archs.h>
#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_threadstuff.h>
#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_dag.h>
#include <dev/raidframe/rf_dagutils.h>
#include <dev/raidframe/rf_dagfuncs.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_freelist.h>
#include <dev/raidframe/rf_map.h>
#include <dev/raidframe/rf_shutdown.h>

#define SNUM_DIFF(_a_,_b_) (((_a_)>(_b_))?((_a_)-(_b_)):((_b_)-(_a_)))

RF_RedFuncs_t rf_xorFuncs = {
	rf_RegularXorFunc, "Reg Xr",
rf_SimpleXorFunc, "Simple Xr"};

RF_RedFuncs_t rf_xorRecoveryFuncs = {
	rf_RecoveryXorFunc, "Recovery Xr",
rf_RecoveryXorFunc, "Recovery Xr"};

static void rf_RecurPrintDAG(RF_DagNode_t *, int, int);
static void rf_PrintDAG(RF_DagHeader_t *);
static int 
rf_ValidateBranch(RF_DagNode_t *, int *, int *,
    RF_DagNode_t **, int);
static void rf_ValidateBranchVisitedBits(RF_DagNode_t *, int, int);
static void rf_ValidateVisitedBits(RF_DagHeader_t *);

/******************************************************************************
 *
 * InitNode - initialize a dag node
 *
 * the size of the propList array is always the same as that of the
 * successors array.
 *
 *****************************************************************************/
void 
rf_InitNode(
    RF_DagNode_t * node,
    RF_NodeStatus_t initstatus,
    int commit,
    int (*doFunc) (RF_DagNode_t * node),
    int (*undoFunc) (RF_DagNode_t * node),
    int (*wakeFunc) (RF_DagNode_t * node, int status),
    int nSucc,
    int nAnte,
    int nParam,
    int nResult,
    RF_DagHeader_t * hdr,
    char *name,
    RF_AllocListElem_t * alist)
{
	void  **ptrs;
	int     nptrs;

	if (nAnte > RF_MAX_ANTECEDENTS)
		RF_PANIC();
	node->status = initstatus;
	node->commitNode = commit;
	node->doFunc = doFunc;
	node->undoFunc = undoFunc;
	node->wakeFunc = wakeFunc;
	node->numParams = nParam;
	node->numResults = nResult;
	node->numAntecedents = nAnte;
	node->numAntDone = 0;
	node->next = NULL;
	node->numSuccedents = nSucc;
	node->name = name;
	node->dagHdr = hdr;
	node->visited = 0;

	/* allocate all the pointers with one call to malloc */
	nptrs = nSucc + nAnte + nResult + nSucc;

	if (nptrs <= RF_DAG_PTRCACHESIZE) {
		/*
	         * The dag_ptrs field of the node is basically some scribble
	         * space to be used here. We could get rid of it, and always
	         * allocate the range of pointers, but that's expensive. So,
	         * we pick a "common case" size for the pointer cache. Hopefully,
	         * we'll find that:
	         * (1) Generally, nptrs doesn't exceed RF_DAG_PTRCACHESIZE by
	         *     only a little bit (least efficient case)
	         * (2) Generally, ntprs isn't a lot less than RF_DAG_PTRCACHESIZE
	         *     (wasted memory)
	         */
		ptrs = (void **) node->dag_ptrs;
	} else {
		RF_CallocAndAdd(ptrs, nptrs, sizeof(void *), (void **), alist);
	}
	node->succedents = (nSucc) ? (RF_DagNode_t **) ptrs : NULL;
	node->antecedents = (nAnte) ? (RF_DagNode_t **) (ptrs + nSucc) : NULL;
	node->results = (nResult) ? (void **) (ptrs + nSucc + nAnte) : NULL;
	node->propList = (nSucc) ? (RF_PropHeader_t **) (ptrs + nSucc + nAnte + nResult) : NULL;

	if (nParam) {
		if (nParam <= RF_DAG_PARAMCACHESIZE) {
			node->params = (RF_DagParam_t *) node->dag_params;
		} else {
			RF_CallocAndAdd(node->params, nParam, sizeof(RF_DagParam_t), (RF_DagParam_t *), alist);
		}
	} else {
		node->params = NULL;
	}
}



/******************************************************************************
 *
 * allocation and deallocation routines
 *
 *****************************************************************************/

void 
rf_FreeDAG(dag_h)
	RF_DagHeader_t *dag_h;
{
	RF_AccessStripeMapHeader_t *asmap, *t_asmap;
	RF_DagHeader_t *nextDag;
	int     i;

	while (dag_h) {
		nextDag = dag_h->next;
		for (i = 0; dag_h->memChunk[i] && i < RF_MAXCHUNKS; i++) {
			/* release mem chunks */
			rf_ReleaseMemChunk(dag_h->memChunk[i]);
			dag_h->memChunk[i] = NULL;
		}

		RF_ASSERT(i == dag_h->chunkIndex);
		if (dag_h->xtraChunkCnt > 0) {
			/* free xtraMemChunks */
			for (i = 0; dag_h->xtraMemChunk[i] && i < dag_h->xtraChunkIndex; i++) {
				rf_ReleaseMemChunk(dag_h->xtraMemChunk[i]);
				dag_h->xtraMemChunk[i] = NULL;
			}
			RF_ASSERT(i == dag_h->xtraChunkIndex);
			/* free ptrs to xtraMemChunks */
			RF_Free(dag_h->xtraMemChunk, dag_h->xtraChunkCnt * sizeof(RF_ChunkDesc_t *));
		}
		rf_FreeAllocList(dag_h->allocList);
		for (asmap = dag_h->asmList; asmap;) {
			t_asmap = asmap;
			asmap = asmap->next;
			rf_FreeAccessStripeMap(t_asmap);
		}
		rf_FreeDAGHeader(dag_h);
		dag_h = nextDag;
	}
}

RF_PropHeader_t *
rf_MakePropListEntry(
    RF_DagHeader_t * dag_h,
    int resultNum,
    int paramNum,
    RF_PropHeader_t * next,
    RF_AllocListElem_t * allocList)
{
	RF_PropHeader_t *p;

	RF_CallocAndAdd(p, 1, sizeof(RF_PropHeader_t),
	    (RF_PropHeader_t *), allocList);
	p->resultNum = resultNum;
	p->paramNum = paramNum;
	p->next = next;
	return (p);
}

static RF_FreeList_t *rf_dagh_freelist;

#define RF_MAX_FREE_DAGH 128
#define RF_DAGH_INC       16
#define RF_DAGH_INITIAL   32

static void rf_ShutdownDAGs(void *);
static void 
rf_ShutdownDAGs(ignored)
	void   *ignored;
{
	RF_FREELIST_DESTROY(rf_dagh_freelist, next, (RF_DagHeader_t *));
}

int 
rf_ConfigureDAGs(listp)
	RF_ShutdownList_t **listp;
{
	int     rc;

	RF_FREELIST_CREATE(rf_dagh_freelist, RF_MAX_FREE_DAGH,
	    RF_DAGH_INC, sizeof(RF_DagHeader_t));
	if (rf_dagh_freelist == NULL)
		return (ENOMEM);
	rc = rf_ShutdownCreate(listp, rf_ShutdownDAGs, NULL);
	if (rc) {
		RF_ERRORMSG3("Unable to add to shutdown list file %s line %d rc=%d\n",
		    __FILE__, __LINE__, rc);
		rf_ShutdownDAGs(NULL);
		return (rc);
	}
	RF_FREELIST_PRIME(rf_dagh_freelist, RF_DAGH_INITIAL, next,
	    (RF_DagHeader_t *));
	return (0);
}

RF_DagHeader_t *
rf_AllocDAGHeader()
{
	RF_DagHeader_t *dh;

	RF_FREELIST_GET(rf_dagh_freelist, dh, next, (RF_DagHeader_t *));
	if (dh) {
		bzero((char *) dh, sizeof(RF_DagHeader_t));
	}
	return (dh);
}

void 
rf_FreeDAGHeader(RF_DagHeader_t * dh)
{
	RF_FREELIST_FREE(rf_dagh_freelist, dh, next);
}
/* allocates a buffer big enough to hold the data described by pda */
void   *
rf_AllocBuffer(
    RF_Raid_t * raidPtr,
    RF_DagHeader_t * dag_h,
    RF_PhysDiskAddr_t * pda,
    RF_AllocListElem_t * allocList)
{
	char   *p;

	RF_MallocAndAdd(p, pda->numSector << raidPtr->logBytesPerSector,
	    (char *), allocList);
	return ((void *) p);
}
/******************************************************************************
 *
 * debug routines
 *
 *****************************************************************************/

char   *
rf_NodeStatusString(RF_DagNode_t * node)
{
	switch (node->status) {
		case rf_wait:return ("wait");
	case rf_fired:
		return ("fired");
	case rf_good:
		return ("good");
	case rf_bad:
		return ("bad");
	default:
		return ("?");
	}
}

void 
rf_PrintNodeInfoString(RF_DagNode_t * node)
{
	RF_PhysDiskAddr_t *pda;
	int     (*df) (RF_DagNode_t *) = node->doFunc;
	int     i, lk, unlk;
	void   *bufPtr;

	if ((df == rf_DiskReadFunc) || (df == rf_DiskWriteFunc)
	    || (df == rf_DiskReadMirrorIdleFunc)
	    || (df == rf_DiskReadMirrorPartitionFunc)) {
		pda = (RF_PhysDiskAddr_t *) node->params[0].p;
		bufPtr = (void *) node->params[1].p;
		lk = RF_EXTRACT_LOCK_FLAG(node->params[3].v);
		unlk = RF_EXTRACT_UNLOCK_FLAG(node->params[3].v);
		RF_ASSERT(!(lk && unlk));
		printf("r %d c %d offs %ld nsect %d buf 0x%lx %s\n", pda->row, pda->col,
		    (long) pda->startSector, (int) pda->numSector, (long) bufPtr,
		    (lk) ? "LOCK" : ((unlk) ? "UNLK" : " "));
		return;
	}
	if (df == rf_DiskUnlockFunc) {
		pda = (RF_PhysDiskAddr_t *) node->params[0].p;
		lk = RF_EXTRACT_LOCK_FLAG(node->params[3].v);
		unlk = RF_EXTRACT_UNLOCK_FLAG(node->params[3].v);
		RF_ASSERT(!(lk && unlk));
		printf("r %d c %d %s\n", pda->row, pda->col,
		    (lk) ? "LOCK" : ((unlk) ? "UNLK" : "nop"));
		return;
	}
	if ((df == rf_SimpleXorFunc) || (df == rf_RegularXorFunc)
	    || (df == rf_RecoveryXorFunc)) {
		printf("result buf 0x%lx\n", (long) node->results[0]);
		for (i = 0; i < node->numParams - 1; i += 2) {
			pda = (RF_PhysDiskAddr_t *) node->params[i].p;
			bufPtr = (RF_PhysDiskAddr_t *) node->params[i + 1].p;
			printf("    buf 0x%lx r%d c%d offs %ld nsect %d\n",
			    (long) bufPtr, pda->row, pda->col,
			    (long) pda->startSector, (int) pda->numSector);
		}
		return;
	}
#if RF_INCLUDE_PARITYLOGGING > 0
	if (df == rf_ParityLogOverwriteFunc || df == rf_ParityLogUpdateFunc) {
		for (i = 0; i < node->numParams - 1; i += 2) {
			pda = (RF_PhysDiskAddr_t *) node->params[i].p;
			bufPtr = (RF_PhysDiskAddr_t *) node->params[i + 1].p;
			printf(" r%d c%d offs %ld nsect %d buf 0x%lx\n",
			    pda->row, pda->col, (long) pda->startSector,
			    (int) pda->numSector, (long) bufPtr);
		}
		return;
	}
#endif				/* RF_INCLUDE_PARITYLOGGING > 0 */

	if ((df == rf_TerminateFunc) || (df == rf_NullNodeFunc)) {
		printf("\n");
		return;
	}
	printf("?\n");
}

static void 
rf_RecurPrintDAG(node, depth, unvisited)
	RF_DagNode_t *node;
	int     depth;
	int     unvisited;
{
	char   *anttype;
	int     i;

	node->visited = (unvisited) ? 0 : 1;
	printf("(%d) %d C%d %s: %s,s%d %d/%d,a%d/%d,p%d,r%d S{", depth,
	    node->nodeNum, node->commitNode, node->name, rf_NodeStatusString(node),
	    node->numSuccedents, node->numSuccFired, node->numSuccDone,
	    node->numAntecedents, node->numAntDone, node->numParams, node->numResults);
	for (i = 0; i < node->numSuccedents; i++) {
		printf("%d%s", node->succedents[i]->nodeNum,
		    ((i == node->numSuccedents - 1) ? "\0" : " "));
	}
	printf("} A{");
	for (i = 0; i < node->numAntecedents; i++) {
		switch (node->antType[i]) {
		case rf_trueData:
			anttype = "T";
			break;
		case rf_antiData:
			anttype = "A";
			break;
		case rf_outputData:
			anttype = "O";
			break;
		case rf_control:
			anttype = "C";
			break;
		default:
			anttype = "?";
			break;
		}
		printf("%d(%s)%s", node->antecedents[i]->nodeNum, anttype, (i == node->numAntecedents - 1) ? "\0" : " ");
	}
	printf("}; ");
	rf_PrintNodeInfoString(node);
	for (i = 0; i < node->numSuccedents; i++) {
		if (node->succedents[i]->visited == unvisited)
			rf_RecurPrintDAG(node->succedents[i], depth + 1, unvisited);
	}
}

static void 
rf_PrintDAG(dag_h)
	RF_DagHeader_t *dag_h;
{
	int     unvisited, i;
	char   *status;

	/* set dag status */
	switch (dag_h->status) {
	case rf_enable:
		status = "enable";
		break;
	case rf_rollForward:
		status = "rollForward";
		break;
	case rf_rollBackward:
		status = "rollBackward";
		break;
	default:
		status = "illegal!";
		break;
	}
	/* find out if visited bits are currently set or clear */
	unvisited = dag_h->succedents[0]->visited;

	printf("DAG type:  %s\n", dag_h->creator);
	printf("format is (depth) num commit type: status,nSucc nSuccFired/nSuccDone,nAnte/nAnteDone,nParam,nResult S{x} A{x(type)};  info\n");
	printf("(0) %d Hdr: %s, s%d, (commit %d/%d) S{", dag_h->nodeNum,
	    status, dag_h->numSuccedents, dag_h->numCommitNodes, dag_h->numCommits);
	for (i = 0; i < dag_h->numSuccedents; i++) {
		printf("%d%s", dag_h->succedents[i]->nodeNum,
		    ((i == dag_h->numSuccedents - 1) ? "\0" : " "));
	}
	printf("};\n");
	for (i = 0; i < dag_h->numSuccedents; i++) {
		if (dag_h->succedents[i]->visited == unvisited)
			rf_RecurPrintDAG(dag_h->succedents[i], 1, unvisited);
	}
}
/* assigns node numbers */
int 
rf_AssignNodeNums(RF_DagHeader_t * dag_h)
{
	int     unvisited, i, nnum;
	RF_DagNode_t *node;

	nnum = 0;
	unvisited = dag_h->succedents[0]->visited;

	dag_h->nodeNum = nnum++;
	for (i = 0; i < dag_h->numSuccedents; i++) {
		node = dag_h->succedents[i];
		if (node->visited == unvisited) {
			nnum = rf_RecurAssignNodeNums(dag_h->succedents[i], nnum, unvisited);
		}
	}
	return (nnum);
}

int 
rf_RecurAssignNodeNums(node, num, unvisited)
	RF_DagNode_t *node;
	int     num;
	int     unvisited;
{
	int     i;

	node->visited = (unvisited) ? 0 : 1;

	node->nodeNum = num++;
	for (i = 0; i < node->numSuccedents; i++) {
		if (node->succedents[i]->visited == unvisited) {
			num = rf_RecurAssignNodeNums(node->succedents[i], num, unvisited);
		}
	}
	return (num);
}
/* set the header pointers in each node to "newptr" */
void 
rf_ResetDAGHeaderPointers(dag_h, newptr)
	RF_DagHeader_t *dag_h;
	RF_DagHeader_t *newptr;
{
	int     i;
	for (i = 0; i < dag_h->numSuccedents; i++)
		if (dag_h->succedents[i]->dagHdr != newptr)
			rf_RecurResetDAGHeaderPointers(dag_h->succedents[i], newptr);
}

void 
rf_RecurResetDAGHeaderPointers(node, newptr)
	RF_DagNode_t *node;
	RF_DagHeader_t *newptr;
{
	int     i;
	node->dagHdr = newptr;
	for (i = 0; i < node->numSuccedents; i++)
		if (node->succedents[i]->dagHdr != newptr)
			rf_RecurResetDAGHeaderPointers(node->succedents[i], newptr);
}


void 
rf_PrintDAGList(RF_DagHeader_t * dag_h)
{
	int     i = 0;

	for (; dag_h; dag_h = dag_h->next) {
		rf_AssignNodeNums(dag_h);
		printf("\n\nDAG %d IN LIST:\n", i++);
		rf_PrintDAG(dag_h);
	}
}

static int 
rf_ValidateBranch(node, scount, acount, nodes, unvisited)
	RF_DagNode_t *node;
	int    *scount;
	int    *acount;
	RF_DagNode_t **nodes;
	int     unvisited;
{
	int     i, retcode = 0;

	/* construct an array of node pointers indexed by node num */
	node->visited = (unvisited) ? 0 : 1;
	nodes[node->nodeNum] = node;

	if (node->next != NULL) {
		printf("INVALID DAG: next pointer in node is not NULL\n");
		retcode = 1;
	}
	if (node->status != rf_wait) {
		printf("INVALID DAG: Node status is not wait\n");
		retcode = 1;
	}
	if (node->numAntDone != 0) {
		printf("INVALID DAG: numAntDone is not zero\n");
		retcode = 1;
	}
	if (node->doFunc == rf_TerminateFunc) {
		if (node->numSuccedents != 0) {
			printf("INVALID DAG: Terminator node has succedents\n");
			retcode = 1;
		}
	} else {
		if (node->numSuccedents == 0) {
			printf("INVALID DAG: Non-terminator node has no succedents\n");
			retcode = 1;
		}
	}
	for (i = 0; i < node->numSuccedents; i++) {
		if (!node->succedents[i]) {
			printf("INVALID DAG: succedent %d of node %s is NULL\n", i, node->name);
			retcode = 1;
		}
		scount[node->succedents[i]->nodeNum]++;
	}
	for (i = 0; i < node->numAntecedents; i++) {
		if (!node->antecedents[i]) {
			printf("INVALID DAG: antecedent %d of node %s is NULL\n", i, node->name);
			retcode = 1;
		}
		acount[node->antecedents[i]->nodeNum]++;
	}
	for (i = 0; i < node->numSuccedents; i++) {
		if (node->succedents[i]->visited == unvisited) {
			if (rf_ValidateBranch(node->succedents[i], scount,
				acount, nodes, unvisited)) {
				retcode = 1;
			}
		}
	}
	return (retcode);
}

static void 
rf_ValidateBranchVisitedBits(node, unvisited, rl)
	RF_DagNode_t *node;
	int     unvisited;
	int     rl;
{
	int     i;

	RF_ASSERT(node->visited == unvisited);
	for (i = 0; i < node->numSuccedents; i++) {
		if (node->succedents[i] == NULL) {
			printf("node=%lx node->succedents[%d] is NULL\n", (long) node, i);
			RF_ASSERT(0);
		}
		rf_ValidateBranchVisitedBits(node->succedents[i], unvisited, rl + 1);
	}
}
/* NOTE:  never call this on a big dag, because it is exponential
 * in execution time
 */
static void 
rf_ValidateVisitedBits(dag)
	RF_DagHeader_t *dag;
{
	int     i, unvisited;

	unvisited = dag->succedents[0]->visited;

	for (i = 0; i < dag->numSuccedents; i++) {
		if (dag->succedents[i] == NULL) {
			printf("dag=%lx dag->succedents[%d] is NULL\n", (long) dag, i);
			RF_ASSERT(0);
		}
		rf_ValidateBranchVisitedBits(dag->succedents[i], unvisited, 0);
	}
}
/* validate a DAG.  _at entry_ verify that:
 *   -- numNodesCompleted is zero
 *   -- node queue is null
 *   -- dag status is rf_enable
 *   -- next pointer is null on every node
 *   -- all nodes have status wait
 *   -- numAntDone is zero in all nodes
 *   -- terminator node has zero successors
 *   -- no other node besides terminator has zero successors
 *   -- no successor or antecedent pointer in a node is NULL
 *   -- number of times that each node appears as a successor of another node
 *      is equal to the antecedent count on that node
 *   -- number of times that each node appears as an antecedent of another node
 *      is equal to the succedent count on that node
 *   -- what else?
 */
int 
rf_ValidateDAG(dag_h)
	RF_DagHeader_t *dag_h;
{
	int     i, nodecount;
	int    *scount, *acount;/* per-node successor and antecedent counts */
	RF_DagNode_t **nodes;	/* array of ptrs to nodes in dag */
	int     retcode = 0;
	int     unvisited;
	int     commitNodeCount = 0;

	if (rf_validateVisitedDebug)
		rf_ValidateVisitedBits(dag_h);

	if (dag_h->numNodesCompleted != 0) {
		printf("INVALID DAG: num nodes completed is %d, should be 0\n", dag_h->numNodesCompleted);
		retcode = 1;
		goto validate_dag_bad;
	}
	if (dag_h->status != rf_enable) {
		printf("INVALID DAG: not enabled\n");
		retcode = 1;
		goto validate_dag_bad;
	}
	if (dag_h->numCommits != 0) {
		printf("INVALID DAG: numCommits != 0 (%d)\n", dag_h->numCommits);
		retcode = 1;
		goto validate_dag_bad;
	}
	if (dag_h->numSuccedents != 1) {
		/* currently, all dags must have only one succedent */
		printf("INVALID DAG: numSuccedents !1 (%d)\n", dag_h->numSuccedents);
		retcode = 1;
		goto validate_dag_bad;
	}
	nodecount = rf_AssignNodeNums(dag_h);

	unvisited = dag_h->succedents[0]->visited;

	RF_Calloc(scount, nodecount, sizeof(int), (int *));
	RF_Calloc(acount, nodecount, sizeof(int), (int *));
	RF_Calloc(nodes, nodecount, sizeof(RF_DagNode_t *), (RF_DagNode_t **));
	for (i = 0; i < dag_h->numSuccedents; i++) {
		if ((dag_h->succedents[i]->visited == unvisited)
		    && rf_ValidateBranch(dag_h->succedents[i], scount,
			acount, nodes, unvisited)) {
			retcode = 1;
		}
	}
	/* start at 1 to skip the header node */
	for (i = 1; i < nodecount; i++) {
		if (nodes[i]->commitNode)
			commitNodeCount++;
		if (nodes[i]->doFunc == NULL) {
			printf("INVALID DAG: node %s has an undefined doFunc\n", nodes[i]->name);
			retcode = 1;
			goto validate_dag_out;
		}
		if (nodes[i]->undoFunc == NULL) {
			printf("INVALID DAG: node %s has an undefined doFunc\n", nodes[i]->name);
			retcode = 1;
			goto validate_dag_out;
		}
		if (nodes[i]->numAntecedents != scount[nodes[i]->nodeNum]) {
			printf("INVALID DAG: node %s has %d antecedents but appears as a succedent %d times\n",
			    nodes[i]->name, nodes[i]->numAntecedents, scount[nodes[i]->nodeNum]);
			retcode = 1;
			goto validate_dag_out;
		}
		if (nodes[i]->numSuccedents != acount[nodes[i]->nodeNum]) {
			printf("INVALID DAG: node %s has %d succedents but appears as an antecedent %d times\n",
			    nodes[i]->name, nodes[i]->numSuccedents, acount[nodes[i]->nodeNum]);
			retcode = 1;
			goto validate_dag_out;
		}
	}

	if (dag_h->numCommitNodes != commitNodeCount) {
		printf("INVALID DAG: incorrect commit node count.  hdr->numCommitNodes (%d) found (%d) commit nodes in graph\n",
		    dag_h->numCommitNodes, commitNodeCount);
		retcode = 1;
		goto validate_dag_out;
	}
validate_dag_out:
	RF_Free(scount, nodecount * sizeof(int));
	RF_Free(acount, nodecount * sizeof(int));
	RF_Free(nodes, nodecount * sizeof(RF_DagNode_t *));
	if (retcode)
		rf_PrintDAGList(dag_h);

	if (rf_validateVisitedDebug)
		rf_ValidateVisitedBits(dag_h);

	return (retcode);

validate_dag_bad:
	rf_PrintDAGList(dag_h);
	return (retcode);
}


/******************************************************************************
 *
 * misc construction routines
 *
 *****************************************************************************/

void 
rf_redirect_asm(
    RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap)
{
	int     ds = (raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE) ? 1 : 0;
	int     row = asmap->physInfo->row;
	int     fcol = raidPtr->reconControl[row]->fcol;
	int     srow = raidPtr->reconControl[row]->spareRow;
	int     scol = raidPtr->reconControl[row]->spareCol;
	RF_PhysDiskAddr_t *pda;

	RF_ASSERT(raidPtr->status[row] == rf_rs_reconstructing);
	for (pda = asmap->physInfo; pda; pda = pda->next) {
		if (pda->col == fcol) {
			if (rf_dagDebug) {
				if (!rf_CheckRUReconstructed(raidPtr->reconControl[row]->reconMap,
					pda->startSector)) {
					RF_PANIC();
				}
			}
			/* printf("Remapped data for large write\n"); */
			if (ds) {
				raidPtr->Layout.map->MapSector(raidPtr, pda->raidAddress,
				    &pda->row, &pda->col, &pda->startSector, RF_REMAP);
			} else {
				pda->row = srow;
				pda->col = scol;
			}
		}
	}
	for (pda = asmap->parityInfo; pda; pda = pda->next) {
		if (pda->col == fcol) {
			if (rf_dagDebug) {
				if (!rf_CheckRUReconstructed(raidPtr->reconControl[row]->reconMap, pda->startSector)) {
					RF_PANIC();
				}
			}
		}
		if (ds) {
			(raidPtr->Layout.map->MapParity) (raidPtr, pda->raidAddress, &pda->row, &pda->col, &pda->startSector, RF_REMAP);
		} else {
			pda->row = srow;
			pda->col = scol;
		}
	}
}


/* this routine allocates read buffers and generates stripe maps for the
 * regions of the array from the start of the stripe to the start of the
 * access, and from the end of the access to the end of the stripe.  It also
 * computes and returns the number of DAG nodes needed to read all this data.
 * Note that this routine does the wrong thing if the access is fully
 * contained within one stripe unit, so we RF_ASSERT against this case at the
 * start.
 */
void 
rf_MapUnaccessedPortionOfStripe(
    RF_Raid_t * raidPtr,
    RF_RaidLayout_t * layoutPtr,/* in: layout information */
    RF_AccessStripeMap_t * asmap,	/* in: access stripe map */
    RF_DagHeader_t * dag_h,	/* in: header of the dag to create */
    RF_AccessStripeMapHeader_t ** new_asm_h,	/* in: ptr to array of 2
						 * headers, to be filled in */
    int *nRodNodes,		/* out: num nodes to be generated to read
				 * unaccessed data */
    char **sosBuffer,		/* out: pointers to newly allocated buffer */
    char **eosBuffer,
    RF_AllocListElem_t * allocList)
{
	RF_RaidAddr_t sosRaidAddress, eosRaidAddress;
	RF_SectorNum_t sosNumSector, eosNumSector;

	RF_ASSERT(asmap->numStripeUnitsAccessed > (layoutPtr->numDataCol / 2));
	/* generate an access map for the region of the array from start of
	 * stripe to start of access */
	new_asm_h[0] = new_asm_h[1] = NULL;
	*nRodNodes = 0;
	if (!rf_RaidAddressStripeAligned(layoutPtr, asmap->raidAddress)) {
		sosRaidAddress = rf_RaidAddressOfPrevStripeBoundary(layoutPtr, asmap->raidAddress);
		sosNumSector = asmap->raidAddress - sosRaidAddress;
		RF_MallocAndAdd(*sosBuffer, rf_RaidAddressToByte(raidPtr, sosNumSector), (char *), allocList);
		new_asm_h[0] = rf_MapAccess(raidPtr, sosRaidAddress, sosNumSector, *sosBuffer, RF_DONT_REMAP);
		new_asm_h[0]->next = dag_h->asmList;
		dag_h->asmList = new_asm_h[0];
		*nRodNodes += new_asm_h[0]->stripeMap->numStripeUnitsAccessed;

		RF_ASSERT(new_asm_h[0]->stripeMap->next == NULL);
		/* we're totally within one stripe here */
		if (asmap->flags & RF_ASM_REDIR_LARGE_WRITE)
			rf_redirect_asm(raidPtr, new_asm_h[0]->stripeMap);
	}
	/* generate an access map for the region of the array from end of
	 * access to end of stripe */
	if (!rf_RaidAddressStripeAligned(layoutPtr, asmap->endRaidAddress)) {
		eosRaidAddress = asmap->endRaidAddress;
		eosNumSector = rf_RaidAddressOfNextStripeBoundary(layoutPtr, eosRaidAddress) - eosRaidAddress;
		RF_MallocAndAdd(*eosBuffer, rf_RaidAddressToByte(raidPtr, eosNumSector), (char *), allocList);
		new_asm_h[1] = rf_MapAccess(raidPtr, eosRaidAddress, eosNumSector, *eosBuffer, RF_DONT_REMAP);
		new_asm_h[1]->next = dag_h->asmList;
		dag_h->asmList = new_asm_h[1];
		*nRodNodes += new_asm_h[1]->stripeMap->numStripeUnitsAccessed;

		RF_ASSERT(new_asm_h[1]->stripeMap->next == NULL);
		/* we're totally within one stripe here */
		if (asmap->flags & RF_ASM_REDIR_LARGE_WRITE)
			rf_redirect_asm(raidPtr, new_asm_h[1]->stripeMap);
	}
}



/* returns non-zero if the indicated ranges of stripe unit offsets overlap */
int 
rf_PDAOverlap(
    RF_RaidLayout_t * layoutPtr,
    RF_PhysDiskAddr_t * src,
    RF_PhysDiskAddr_t * dest)
{
	RF_SectorNum_t soffs = rf_StripeUnitOffset(layoutPtr, src->startSector);
	RF_SectorNum_t doffs = rf_StripeUnitOffset(layoutPtr, dest->startSector);
	/* use -1 to be sure we stay within SU */
	RF_SectorNum_t send = rf_StripeUnitOffset(layoutPtr, src->startSector + src->numSector - 1);
	RF_SectorNum_t dend = rf_StripeUnitOffset(layoutPtr, dest->startSector + dest->numSector - 1);
	return ((RF_MAX(soffs, doffs) <= RF_MIN(send, dend)) ? 1 : 0);
}


/* GenerateFailedAccessASMs
 *
 * this routine figures out what portion of the stripe needs to be read
 * to effect the degraded read or write operation.  It's primary function
 * is to identify everything required to recover the data, and then
 * eliminate anything that is already being accessed by the user.
 *
 * The main result is two new ASMs, one for the region from the start of the
 * stripe to the start of the access, and one for the region from the end of
 * the access to the end of the stripe.  These ASMs describe everything that
 * needs to be read to effect the degraded access.  Other results are:
 *    nXorBufs -- the total number of buffers that need to be XORed together to
 *                recover the lost data,
 *    rpBufPtr -- ptr to a newly-allocated buffer to hold the parity.  If NULL
 *                at entry, not allocated.
 *    overlappingPDAs --
 *                describes which of the non-failed PDAs in the user access
 *                overlap data that needs to be read to effect recovery.
 *                overlappingPDAs[i]==1 if and only if, neglecting the failed
 *                PDA, the ith pda in the input asm overlaps data that needs
 *                to be read for recovery.
 */
 /* in: asm - ASM for the actual access, one stripe only */
 /* in: faildPDA - which component of the access has failed */
 /* in: dag_h - header of the DAG we're going to create */
 /* out: new_asm_h - the two new ASMs */
 /* out: nXorBufs - the total number of xor bufs required */
 /* out: rpBufPtr - a buffer for the parity read */
void 
rf_GenerateFailedAccessASMs(
    RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap,
    RF_PhysDiskAddr_t * failedPDA,
    RF_DagHeader_t * dag_h,
    RF_AccessStripeMapHeader_t ** new_asm_h,
    int *nXorBufs,
    char **rpBufPtr,
    char *overlappingPDAs,
    RF_AllocListElem_t * allocList)
{
	RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);

	/* s=start, e=end, s=stripe, a=access, f=failed, su=stripe unit */
	RF_RaidAddr_t sosAddr, sosEndAddr, eosStartAddr, eosAddr;

	RF_SectorCount_t numSect[2], numParitySect;
	RF_PhysDiskAddr_t *pda;
	char   *rdBuf, *bufP;
	int     foundit, i;

	bufP = NULL;
	foundit = 0;
	/* first compute the following raid addresses: start of stripe,
	 * (sosAddr) MIN(start of access, start of failed SU),   (sosEndAddr)
	 * MAX(end of access, end of failed SU),       (eosStartAddr) end of
	 * stripe (i.e. start of next stripe)   (eosAddr) */
	sosAddr = rf_RaidAddressOfPrevStripeBoundary(layoutPtr, asmap->raidAddress);
	sosEndAddr = RF_MIN(asmap->raidAddress, rf_RaidAddressOfPrevStripeUnitBoundary(layoutPtr, failedPDA->raidAddress));
	eosStartAddr = RF_MAX(asmap->endRaidAddress, rf_RaidAddressOfNextStripeUnitBoundary(layoutPtr, failedPDA->raidAddress));
	eosAddr = rf_RaidAddressOfNextStripeBoundary(layoutPtr, asmap->raidAddress);

	/* now generate access stripe maps for each of the above regions of
	 * the stripe.  Use a dummy (NULL) buf ptr for now */

	new_asm_h[0] = (sosAddr != sosEndAddr) ? rf_MapAccess(raidPtr, sosAddr, sosEndAddr - sosAddr, NULL, RF_DONT_REMAP) : NULL;
	new_asm_h[1] = (eosStartAddr != eosAddr) ? rf_MapAccess(raidPtr, eosStartAddr, eosAddr - eosStartAddr, NULL, RF_DONT_REMAP) : NULL;

	/* walk through the PDAs and range-restrict each SU to the region of
	 * the SU touched on the failed PDA.  also compute total data buffer
	 * space requirements in this step.  Ignore the parity for now. */

	numSect[0] = numSect[1] = 0;
	if (new_asm_h[0]) {
		new_asm_h[0]->next = dag_h->asmList;
		dag_h->asmList = new_asm_h[0];
		for (pda = new_asm_h[0]->stripeMap->physInfo; pda; pda = pda->next) {
			rf_RangeRestrictPDA(raidPtr, failedPDA, pda, RF_RESTRICT_NOBUFFER, 0);
			numSect[0] += pda->numSector;
		}
	}
	if (new_asm_h[1]) {
		new_asm_h[1]->next = dag_h->asmList;
		dag_h->asmList = new_asm_h[1];
		for (pda = new_asm_h[1]->stripeMap->physInfo; pda; pda = pda->next) {
			rf_RangeRestrictPDA(raidPtr, failedPDA, pda, RF_RESTRICT_NOBUFFER, 0);
			numSect[1] += pda->numSector;
		}
	}
	numParitySect = failedPDA->numSector;

	/* allocate buffer space for the data & parity we have to read to
	 * recover from the failure */

	if (numSect[0] + numSect[1] + ((rpBufPtr) ? numParitySect : 0)) {	/* don't allocate parity
										 * buf if not needed */
		RF_MallocAndAdd(rdBuf, rf_RaidAddressToByte(raidPtr, numSect[0] + numSect[1] + numParitySect), (char *), allocList);
		bufP = rdBuf;
		if (rf_degDagDebug)
			printf("Newly allocated buffer (%d bytes) is 0x%lx\n",
			    (int) rf_RaidAddressToByte(raidPtr, numSect[0] + numSect[1] + numParitySect), (unsigned long) bufP);
	}
	/* now walk through the pdas one last time and assign buffer pointers
	 * (ugh!).  Again, ignore the parity.  also, count nodes to find out
	 * how many bufs need to be xored together */
	(*nXorBufs) = 1;	/* in read case, 1 is for parity.  In write
				 * case, 1 is for failed data */
	if (new_asm_h[0]) {
		for (pda = new_asm_h[0]->stripeMap->physInfo; pda; pda = pda->next) {
			pda->bufPtr = bufP;
			bufP += rf_RaidAddressToByte(raidPtr, pda->numSector);
		}
		*nXorBufs += new_asm_h[0]->stripeMap->numStripeUnitsAccessed;
	}
	if (new_asm_h[1]) {
		for (pda = new_asm_h[1]->stripeMap->physInfo; pda; pda = pda->next) {
			pda->bufPtr = bufP;
			bufP += rf_RaidAddressToByte(raidPtr, pda->numSector);
		}
		(*nXorBufs) += new_asm_h[1]->stripeMap->numStripeUnitsAccessed;
	}
	if (rpBufPtr)
		*rpBufPtr = bufP;	/* the rest of the buffer is for
					 * parity */

	/* the last step is to figure out how many more distinct buffers need
	 * to get xor'd to produce the missing unit.  there's one for each
	 * user-data read node that overlaps the portion of the failed unit
	 * being accessed */

	for (foundit = i = 0, pda = asmap->physInfo; pda; i++, pda = pda->next) {
		if (pda == failedPDA) {
			i--;
			foundit = 1;
			continue;
		}
		if (rf_PDAOverlap(layoutPtr, pda, failedPDA)) {
			overlappingPDAs[i] = 1;
			(*nXorBufs)++;
		}
	}
	if (!foundit) {
		RF_ERRORMSG("GenerateFailedAccessASMs: did not find failedPDA in asm list\n");
		RF_ASSERT(0);
	}
	if (rf_degDagDebug) {
		if (new_asm_h[0]) {
			printf("First asm:\n");
			rf_PrintFullAccessStripeMap(new_asm_h[0], 1);
		}
		if (new_asm_h[1]) {
			printf("Second asm:\n");
			rf_PrintFullAccessStripeMap(new_asm_h[1], 1);
		}
	}
}


/* adjusts the offset and number of sectors in the destination pda so that
 * it covers at most the region of the SU covered by the source PDA.  This
 * is exclusively a restriction:  the number of sectors indicated by the
 * target PDA can only shrink.
 *
 * For example:  s = sectors within SU indicated by source PDA
 *               d = sectors within SU indicated by dest PDA
 *               r = results, stored in dest PDA
 *
 * |--------------- one stripe unit ---------------------|
 * |           sssssssssssssssssssssssssssssssss         |
 * |    ddddddddddddddddddddddddddddddddddddddddddddd    |
 * |           rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr         |
 *
 * Another example:
 *
 * |--------------- one stripe unit ---------------------|
 * |           sssssssssssssssssssssssssssssssss         |
 * |    ddddddddddddddddddddddd                          |
 * |           rrrrrrrrrrrrrrrr                          |
 *
 */
void 
rf_RangeRestrictPDA(
    RF_Raid_t * raidPtr,
    RF_PhysDiskAddr_t * src,
    RF_PhysDiskAddr_t * dest,
    int dobuffer,
    int doraidaddr)
{
	RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
	RF_SectorNum_t soffs = rf_StripeUnitOffset(layoutPtr, src->startSector);
	RF_SectorNum_t doffs = rf_StripeUnitOffset(layoutPtr, dest->startSector);
	RF_SectorNum_t send = rf_StripeUnitOffset(layoutPtr, src->startSector + src->numSector - 1);	/* use -1 to be sure we
													 * stay within SU */
	RF_SectorNum_t dend = rf_StripeUnitOffset(layoutPtr, dest->startSector + dest->numSector - 1);
	RF_SectorNum_t subAddr = rf_RaidAddressOfPrevStripeUnitBoundary(layoutPtr, dest->startSector);	/* stripe unit boundary */

	dest->startSector = subAddr + RF_MAX(soffs, doffs);
	dest->numSector = subAddr + RF_MIN(send, dend) + 1 - dest->startSector;

	if (dobuffer)
		dest->bufPtr += (soffs > doffs) ? rf_RaidAddressToByte(raidPtr, soffs - doffs) : 0;
	if (doraidaddr) {
		dest->raidAddress = rf_RaidAddressOfPrevStripeUnitBoundary(layoutPtr, dest->raidAddress) +
		    rf_StripeUnitOffset(layoutPtr, dest->startSector);
	}
}
/*
 * Want the highest of these primes to be the largest one
 * less than the max expected number of columns (won't hurt
 * to be too small or too large, but won't be optimal, either)
 * --jimz
 */
#define NLOWPRIMES 8
static int lowprimes[NLOWPRIMES] = {2, 3, 5, 7, 11, 13, 17, 19};
/*****************************************************************************
 * compute the workload shift factor.  (chained declustering)
 *
 * return nonzero if access should shift to secondary, otherwise,
 * access is to primary
 *****************************************************************************/
int 
rf_compute_workload_shift(
    RF_Raid_t * raidPtr,
    RF_PhysDiskAddr_t * pda)
{
	/*
         * variables:
         *  d   = column of disk containing primary
         *  f   = column of failed disk
         *  n   = number of disks in array
         *  sd  = "shift distance" (number of columns that d is to the right of f)
         *  row = row of array the access is in
         *  v   = numerator of redirection ratio
         *  k   = denominator of redirection ratio
         */
	RF_RowCol_t d, f, sd, row, n;
	int     k, v, ret, i;

	row = pda->row;
	n = raidPtr->numCol;

	/* assign column of primary copy to d */
	d = pda->col;

	/* assign column of dead disk to f */
	for (f = 0; ((!RF_DEAD_DISK(raidPtr->Disks[row][f].status)) && (f < n)); f++);

	RF_ASSERT(f < n);
	RF_ASSERT(f != d);

	sd = (f > d) ? (n + d - f) : (d - f);
	RF_ASSERT(sd < n);

	/*
         * v of every k accesses should be redirected
         *
         * v/k := (n-1-sd)/(n-1)
         */
	v = (n - 1 - sd);
	k = (n - 1);

#if 1
	/*
         * XXX
         * Is this worth it?
         *
         * Now reduce the fraction, by repeatedly factoring
         * out primes (just like they teach in elementary school!)
         */
	for (i = 0; i < NLOWPRIMES; i++) {
		if (lowprimes[i] > v)
			break;
		while (((v % lowprimes[i]) == 0) && ((k % lowprimes[i]) == 0)) {
			v /= lowprimes[i];
			k /= lowprimes[i];
		}
	}
#endif

	raidPtr->hist_diskreq[row][d]++;
	if (raidPtr->hist_diskreq[row][d] > v) {
		ret = 0;	/* do not redirect */
	} else {
		ret = 1;	/* redirect */
	}

#if 0
	printf("d=%d f=%d sd=%d v=%d k=%d ret=%d h=%d\n", d, f, sd, v, k, ret,
	    raidPtr->hist_diskreq[row][d]);
#endif

	if (raidPtr->hist_diskreq[row][d] >= k) {
		/* reset counter */
		raidPtr->hist_diskreq[row][d] = 0;
	}
	return (ret);
}
/*
 * Disk selection routines
 */

/*
 * Selects the disk with the shortest queue from a mirror pair.
 * Both the disk I/Os queued in RAIDframe as well as those at the physical
 * disk are counted as members of the "queue"
 */
void 
rf_SelectMirrorDiskIdle(RF_DagNode_t * node)
{
	RF_Raid_t *raidPtr = (RF_Raid_t *) node->dagHdr->raidPtr;
	RF_RowCol_t rowData, colData, rowMirror, colMirror;
	int     dataQueueLength, mirrorQueueLength, usemirror;
	RF_PhysDiskAddr_t *data_pda = (RF_PhysDiskAddr_t *) node->params[0].p;
	RF_PhysDiskAddr_t *mirror_pda = (RF_PhysDiskAddr_t *) node->params[4].p;
	RF_PhysDiskAddr_t *tmp_pda;
	RF_RaidDisk_t **disks = raidPtr->Disks;
	RF_DiskQueue_t **dqs = raidPtr->Queues, *dataQueue, *mirrorQueue;

	/* return the [row col] of the disk with the shortest queue */
	rowData = data_pda->row;
	colData = data_pda->col;
	rowMirror = mirror_pda->row;
	colMirror = mirror_pda->col;
	dataQueue = &(dqs[rowData][colData]);
	mirrorQueue = &(dqs[rowMirror][colMirror]);

#ifdef RF_LOCK_QUEUES_TO_READ_LEN
	RF_LOCK_QUEUE_MUTEX(dataQueue, "SelectMirrorDiskIdle");
#endif				/* RF_LOCK_QUEUES_TO_READ_LEN */
	dataQueueLength = dataQueue->queueLength + dataQueue->numOutstanding;
#ifdef RF_LOCK_QUEUES_TO_READ_LEN
	RF_UNLOCK_QUEUE_MUTEX(dataQueue, "SelectMirrorDiskIdle");
	RF_LOCK_QUEUE_MUTEX(mirrorQueue, "SelectMirrorDiskIdle");
#endif				/* RF_LOCK_QUEUES_TO_READ_LEN */
	mirrorQueueLength = mirrorQueue->queueLength + mirrorQueue->numOutstanding;
#ifdef RF_LOCK_QUEUES_TO_READ_LEN
	RF_UNLOCK_QUEUE_MUTEX(mirrorQueue, "SelectMirrorDiskIdle");
#endif				/* RF_LOCK_QUEUES_TO_READ_LEN */

	usemirror = 0;
	if (RF_DEAD_DISK(disks[rowMirror][colMirror].status)) {
		usemirror = 0;
	} else
		if (RF_DEAD_DISK(disks[rowData][colData].status)) {
			usemirror = 1;
		} else
			if (raidPtr->parity_good == RF_RAID_DIRTY) {
				/* Trust only the main disk */
				usemirror = 0;
			} else
				if (dataQueueLength < mirrorQueueLength) {
					usemirror = 0;
				} else
					if (mirrorQueueLength < dataQueueLength) {
						usemirror = 1;
					} else {
						/* queues are equal length. attempt
						 * cleverness. */
						if (SNUM_DIFF(dataQueue->last_deq_sector, data_pda->startSector)
						    <= SNUM_DIFF(mirrorQueue->last_deq_sector, mirror_pda->startSector)) {
							usemirror = 0;
						} else {
							usemirror = 1;
						}
					}

	if (usemirror) {
		/* use mirror (parity) disk, swap params 0 & 4 */
		tmp_pda = data_pda;
		node->params[0].p = mirror_pda;
		node->params[4].p = tmp_pda;
	} else {
		/* use data disk, leave param 0 unchanged */
	}
	/* printf("dataQueueLength %d, mirrorQueueLength
	 * %d\n",dataQueueLength, mirrorQueueLength); */
}
/*
 * Do simple partitioning. This assumes that
 * the data and parity disks are laid out identically.
 */
void 
rf_SelectMirrorDiskPartition(RF_DagNode_t * node)
{
	RF_Raid_t *raidPtr = (RF_Raid_t *) node->dagHdr->raidPtr;
	RF_RowCol_t rowData, colData, rowMirror, colMirror;
	RF_PhysDiskAddr_t *data_pda = (RF_PhysDiskAddr_t *) node->params[0].p;
	RF_PhysDiskAddr_t *mirror_pda = (RF_PhysDiskAddr_t *) node->params[4].p;
	RF_PhysDiskAddr_t *tmp_pda;
	RF_RaidDisk_t **disks = raidPtr->Disks;
	RF_DiskQueue_t **dqs = raidPtr->Queues, *dataQueue, *mirrorQueue;
	int     usemirror;

	/* return the [row col] of the disk with the shortest queue */
	rowData = data_pda->row;
	colData = data_pda->col;
	rowMirror = mirror_pda->row;
	colMirror = mirror_pda->col;
	dataQueue = &(dqs[rowData][colData]);
	mirrorQueue = &(dqs[rowMirror][colMirror]);

	usemirror = 0;
	if (RF_DEAD_DISK(disks[rowMirror][colMirror].status)) {
		usemirror = 0;
	} else
		if (RF_DEAD_DISK(disks[rowData][colData].status)) {
			usemirror = 1;
		} else 
			if (raidPtr->parity_good == RF_RAID_DIRTY) {
				/* Trust only the main disk */
				usemirror = 0;
			} else
				if (data_pda->startSector < 
				    (disks[rowData][colData].numBlocks / 2)) {
					usemirror = 0;
				} else {
					usemirror = 1;
				}

	if (usemirror) {
		/* use mirror (parity) disk, swap params 0 & 4 */
		tmp_pda = data_pda;
		node->params[0].p = mirror_pda;
		node->params[4].p = tmp_pda;
	} else {
		/* use data disk, leave param 0 unchanged */
	}
}
