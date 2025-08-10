#include <limits.h>
#include<paths.h>
#include<fcntl.h>
#include<sys/cdefs.h>
#include<unistd.h>
#include<kvm.h>
#include<sys/param.h>
#include<stdlib.h>
#include<stdbool.h>
#include<assert.h>
#include "/usr/src/lib/libkvm/kvm_private.h"
#include <sys/sysctl.h>
#include <sys/user.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "librapl.h"
/*static double 
  proc_calc_pctcpu(struct kinfo_proc *pp)
  {
  const struct kinfo_proc *oldp;

  if (previous_interval != 0) {
  oldp = get_old_proc(pp);
  if (oldp != NULL)
  return ((double)(pp->ki_runtime - oldp->ki_runtime)
  / previous_interval);
  else if (pp->ki_start.tv_sec > previous_wall_time.tv_sec ||
  (pp->ki_start.tv_sec == previous_wall_time.tv_sec &&
  pp->ki_start.tv_usec >= previous_wall_time.tv_usec))
  return ((double)pp->ki_runtime / previous_interval);
  }
  return (pctdouble(pp->ki_pctcpu));
  }*/
typedef struct {
	bool is_Full;
	int tid;
	uint64_t oldruntime;
}thread_info_t;



thread_info_t *find_thread(thread_info_t *thread_info, int num_threads, int tid){
		for (int i = 0; i < num_threads; i++){
			if(thread_info[i].tid == tid){
				return &thread_info[i];
			}
		}
		
		return NULL;
	}

thread_info_t *add_thread(thread_info_t *thread_info, int num_threads, int tid){
		for (int i = 0; i < num_threads; i++){
			if (thread_info[i].is_Full == false){
				thread_info[i].tid = tid;
				thread_info[i].oldruntime = 0;
				thread_info[i].is_Full = true;
				return &thread_info[i];
			}
		}
		printf("The array is full");
		return NULL;
	}

int main(){
	kvm_t *kd;
	struct kinfo_proc *procs;
	struct timespec uptime;
	int num_procs= -1;
	char errbuf[_POSIX2_LINE_MAX];
	struct timeval timeout = {1, 0};
	int tid;
	thread_info_t *t_info;
	
	int num_threads = 19299;
	thread_info_t *thread_info_list = calloc(num_threads, sizeof(thread_info_t));

	kd = kvm_openfiles(NULL, _PATH_DEVNULL, NULL, O_RDONLY, errbuf);
	for (int j = 0; j<50; j++){
		struct kinfo_proc *oldprocs;
		procs = kvm_getprocs(kd, KERN_PROC_ALL, 0, &num_procs);
		
		for (int i = 0; i < num_procs; i++){
			
			//if (strcmp(procs[i].ki_comm, "openssl") == 0){

				
				
			
				tid = procs[i].ki_tid;
				t_info = find_thread(thread_info_list, num_threads, tid);
				if (t_info == NULL){
					
					t_info = add_thread(thread_info_list, num_threads, tid);
					assert(t_info != NULL);					
				}
				

			
				uint64_t newruntime = procs[i].ki_runtime;
				uint64_t oldruntime = t_info->oldruntime;
				
				t_info->oldruntime = newruntime;
				//printf("Difference between newruntime and oldruntime: %lu\t%lu\t%lu\n", (newruntime - oldruntime), newruntime, oldruntime);
					
				clock_gettime(CLOCK_UPTIME, &uptime);
				float perunit = (((float)(newruntime - oldruntime))/(1000000 * 1));
				
				if (perunit > 1){
					perunit = 1;
				}
				if (perunit > 0.05){
					printf(" %d\t%d\t%s\t%lu\t%f\n", tid, procs[i].ki_pid, procs[i].ki_comm, procs[i].ki_runtime, perunit);
				}
				//printf("Perunit: %f\n", perunit);	
				/*printf("Uptime: %ld\n", uptime.tv_sec);*/

			//}


		} 
#if 0
		for (int i = 0; i < num_threads; i++){
			printf("Index: %d, Is Full: %d, tid: %d, runtime: %lu \n", i, thread_info_list[i].is_Full, thread_info_list[i].tid, thread_info_list[i].oldruntime);
		}
		printf("\n");
#endif

		printf("---------\n");
		select(0, NULL, NULL, NULL, &timeout);}
}

