/*
 * $FreeBSD$
 */
#define _KERNEL

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <sys/kse.h>
#include <sys/queue.h>

/*#define DEBUG*/
/*************************************************************
 * These should probably be in a .h file
 **************************************************************/
typedef void thread_fn(void *arg);

struct user_thread {
	struct thread_mailbox mbox;
	char		*stack;
	int		stack_size;
	TAILQ_ENTRY(user_thread) runq_next;
};

struct per_kse {
	struct kse_mailbox mbox;
	struct user_thread *curthread;
};
/*************************************************************
 * Debug stuff
 **************************************************************/
#ifdef	DEBUG

jmp_buf jb3;
#define DUMPREGS(desc) do {_setjmp(jb3); printjb(jb3, desc); } while (0)

char *regname[] = {"%eip","%ebx","%esp","%ebp",
			"%esi","%edi","fpcr","MSK0",
			"MSK1","MSK2","MSK3"};


static 
printjb(struct _jmp_buf *jb, char *desc)
{
	
	int i;
	printf("jb (%s) is at 0x%x\n", desc, jb);
	for( i = 0; i< _JBLEN-4; i++) {
		printf("jb[%d] (%s) = 0x%x\n", i, regname[i], jb[0]._jb[i]);
	}
}
#else
#define DUMPREGS(desc) do {} while (0)
#endif

/*************************************************************
 * Globals
 **************************************************************/
struct per_kse first_kse; /* for NOW cheat and make it global */
TAILQ_HEAD(, user_thread) runqueue = TAILQ_HEAD_INITIALIZER(runqueue);
/*************************************************************
 * Implementation parameters
 **************************************************************/
#define T_STACKSIZE	(16*4096)	/* thread stacksize */
#define K_STACKSIZE	(1*4096)	/* KSE (UTS) stacksize */

/*************************************************************
 * UTS funcions.
 * Simple round_robin for now.
 **************************************************************/
static void
runq_insert(struct user_thread *thread)
{
	TAILQ_INSERT_TAIL(&runqueue, thread, runq_next);
}

static struct user_thread *
select_thread(void)
{
	struct user_thread *thread;

	if ((thread = TAILQ_FIRST(&runqueue))) {
		TAILQ_REMOVE(&runqueue, thread, runq_next);
	}
	return (thread);
}

/*************************************************************
 * The UTS upcall entrypoint
 * Called once on startup (and left by longjump)
 * and there-after, returned to by the upcall many times.
 **************************************************************/
void
UTS(struct kse_mailbox *ke_mbox)
{
	struct user_thread *thread;
	struct thread_mailbox *completed;
	struct per_kse *ksedata;
	int done = 0;	

	/**********************************/
	/* UTS upcall starts running here. */
	/**********************************/
	/**********************************/

	ksedata = ke_mbox->kmbx_UTS_handle;
	/* If there are returned syscall threads, put them on the run queue */
	if ((completed = ke_mbox->kmbx_completed_threads)) {
		ke_mbox->kmbx_completed_threads = NULL;
		while (completed) {
			thread = completed->UTS_handle;
			completed = completed->next_completed;
			runq_insert(thread);
		}
	}

	/* find highest priority thread and load it */
	if ((thread = select_thread())) {
		ksedata->curthread = thread;
		ke_mbox->kmbx_current_thread = &thread->mbox;

		/* loads context similar to longjmp() */
		loadthread(&thread->mbox.ctx.tfrm.tf_tf);
		/* NOTREACHED */
	}
	kse_yield(); 		/* in the kernel it does a thread_exit() */
	/* NOTREACHED */
} 

/*************************************************************
 * Startup mechanism functions
 **************************************************************/
static int
kickkse(struct per_kse *ksedata, int newgroup)
{
	char * newstack;
	jmp_buf jb1;
	jmp_buf jb2;
	struct kse_mailbox *mboxaddr;
	struct per_kse *user_UTS_info;
	int err;

	newstack = malloc(K_STACKSIZE);
	mboxaddr = &ksedata->mbox;
	mboxaddr->kmbx_stackbase = newstack;
	mboxaddr->kmbx_stacksize = K_STACKSIZE;
	mboxaddr->kmbx_upcall = &UTS;
	mboxaddr->kmbx_UTS_handle = ksedata;
	err = kse_new(mboxaddr, newgroup);
	return(err);
}


static int
startkse(struct per_kse *ksedata)
{
	return (kickkse(ksedata, 0));
}

static int
startksegrp(struct per_kse *ksedata)
{
	return(kickkse(ksedata, 1));
}

void badreturn()
{
	printf("thread returned when shouldn't\n");
	exit(1);
}

__inline__ void
pushontostack(struct user_thread *tcb, int value)
{
	int *SP;

	SP = (int *)(tcb->mbox.ctx.tfrm.tf_tf.tf_isp);
	*--SP = value;
	tcb->mbox.ctx.tfrm.tf_tf.tf_isp = (int)SP;
}

struct user_thread *
makethread(thread_fn *fn, int arg1, void *arg2)
{
	struct user_thread *tcb;

	/* We could combine these mallocs */
	tcb = malloc(sizeof *tcb);
	bzero(tcb, sizeof(*tcb));
	tcb->mbox.UTS_handle = tcb; /* back pointer */

	/* malloc the thread's stack */
	/* We COULD mmap it with STACK characteristics */
	/* Then we could add a guard page. */
	tcb->stack_size = T_STACKSIZE; /* set the size we want */
	tcb->stack = malloc(tcb->stack_size);

	/* Make sure there are good defaults */
	savethread(&tcb->mbox.ctx.tfrm.tf_tf);

	/* set the PC to the fn */
	tcb->mbox.ctx.tfrm.tf_tf.tf_eip = (int) fn;

	/* Set the stack and push on the args and a dummy return address */
	tcb->mbox.ctx.tfrm.tf_tf.tf_ebp =
	tcb->mbox.ctx.tfrm.tf_tf.tf_isp =
	tcb->mbox.ctx.tfrm.tf_tf.tf_esp =
	    (int)(&tcb->stack[tcb->stack_size - 16]);
	pushontostack(tcb, (int)arg2);
	pushontostack(tcb, (int)arg1);
	pushontostack(tcb, (int)&badreturn); /* safety return address */
	return (tcb);
}

/*************************************************************
 * code for three separate threads. (so we can see if it works)
 *************************************************************/
static void 
thread1_code(void *arg)
{
	for(;;) {
		sleep (1);
		write(1,".",1);
	}
}

static void 
thread2_code(void *arg)
{
	for(;;) {
		sleep (3);
		write(1,"+",1);
	}
}

static void 
thread3_code(void *arg)
{
	for(;;) {
		sleep (5);
		write(1,"=",1);
	}
}



int main()
{

	/* set up global structures */
	TAILQ_INIT(&runqueue);

	/* define two threads to run, they are runnable but not yet running */
	runq_insert( makethread(&thread1_code, 0, NULL));
	runq_insert( makethread(&thread2_code, 0, NULL));

	/* and one which we will run ourself */
	first_kse.curthread = makethread(&thread3_code, 0, NULL);

	/* start two KSEs in different KSEGRPs */
	if (startkse(&first_kse)) {
		perror("failed to start KSE");
		exit(1);
	}

	/* startksegrp(&second_kse); */ /* we can't do 2 KSEs yet */
	/* One will be sufficient */

	/* we are a thread, start the ball rolling */
	/* let the kernel know we are it */
	first_kse.mbox.kmbx_current_thread = &first_kse.curthread->mbox;
	thread3_code(NULL);
	return 0;
}

