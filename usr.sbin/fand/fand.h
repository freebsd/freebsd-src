#ifndef FAND_H
#define FAND_H

#define MAX_CSTEPS 10

typedef struct cstep {
	float        temp; /* °C or K depending on offset */
	unsigned int duty; /* nanoseconds */
} cstep_t;

typedef struct cprofile {
	unsigned int  lo_duty; /* nanoseconds */
	size_t        step_count;
	cstep_t       steps[MAX_CSTEPS];
} cprofile_t;

extern cprofile_t cprofile;

int parse_fand_config(const char *path);

#endif /* FAND_H */
