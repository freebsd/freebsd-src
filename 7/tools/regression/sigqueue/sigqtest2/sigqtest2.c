/* $FreeBSD$ */
#include <signal.h>
#include <stdio.h>
#include <err.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

int stop_received;
int exit_received;
int cont_received;

void job_handler(int sig, siginfo_t *si, void *ctx)
{
	int status;
	int ret;

	if (si->si_code == CLD_STOPPED) {
		stop_received = 1;
		kill(si->si_pid, SIGCONT);
	} else if (si->si_code == CLD_EXITED) {
		ret = waitpid(si->si_pid, &status, 0);
		if (ret == -1)
			errx(1, "waitpid");
		if (!WIFEXITED(status))
			errx(1, "!WIFEXITED(status)");
		exit_received = 1;
	} else if (si->si_code == CLD_CONTINUED) {
		cont_received = 1;
	}
}

void job_control_test()
{
	struct sigaction sa;
	pid_t pid;
	int count = 10;

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = job_handler;
	sigaction(SIGCHLD, &sa, NULL);
	stop_received = 0;
	cont_received = 0;
	exit_received = 0;
	pid = fork();
	if (pid == 0) {
		kill(getpid(), SIGSTOP);
		exit(1);
	}

	while (!(cont_received && stop_received && exit_received)) {
		sleep(1);
		if (--count == 0)
			break;
	}
	if (!(cont_received && stop_received && exit_received))
		errx(1, "job signals lost");

	printf("job control test OK.\n");
}

void rtsig_handler(int sig, siginfo_t *si, void *ctx)
{
}

int main()
{
	struct sigaction sa;
	sigset_t set;
	union sigval val;

	/* test job control with empty signal queue */
	job_control_test();

	/* now full fill signal queue in kernel */
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = rtsig_handler;
	sigaction(SIGRTMIN, &sa, NULL);
	sigemptyset(&set);
	sigaddset(&set, SIGRTMIN);
	sigprocmask(SIG_BLOCK, &set, NULL);
	val.sival_int = 1;
	while (sigqueue(getpid(), SIGRTMIN, val))
		;

	/* signal queue is fully filled, test the job control again. */
	job_control_test();
	return (0);
}
