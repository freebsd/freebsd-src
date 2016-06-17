#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/sched.h>
#include <asm/io.h>

int main()
{
	printf("#ifndef __ASM_OFFSETS_H__\n#define __ASM_OFFSETS_H__\n");

	printf("#define TASK_STATE %ld\n",
	       (long)offsetof(struct task_struct, state));
	printf("#define TASK_FLAGS %ld\n",
	       (long)offsetof(struct task_struct, flags));
	printf("#define TASK_SIGPENDING %ld\n",
	       (long)offsetof(struct task_struct, sigpending));
	printf("#define TASK_ADDR_LIMIT %ld\n",
	       (long)offsetof(struct task_struct, addr_limit));
	printf("#define TASK_EXEC_DOMAIN %ld\n",
	       (long)offsetof(struct task_struct, exec_domain));
	printf("#define TASK_NEED_RESCHED %ld\n",
	       (long)offsetof(struct task_struct, need_resched));
	printf("#define TASK_SIZE %ld\n", sizeof(struct task_struct));
	printf("#define STACK_SIZE %ld\n", sizeof(union task_union));

	printf("#define HAE_CACHE %ld\n",
	       (long)offsetof(struct alpha_machine_vector, hae_cache));
	printf("#define HAE_REG %ld\n",
	       (long)offsetof(struct alpha_machine_vector, hae_register));

	printf("#endif /* __ASM_OFFSETS_H__ */\n");
	return 0;
}
