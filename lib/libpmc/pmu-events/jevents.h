/* $FreeBSD$ */
#ifndef JEVENTS_H
#define JEVENTS_H 1

int json_events(const char *fn,
		int (*func)(void *data, char *name, const char *event, char *desc,
				char *long_desc,
				char *pmu,
				char *unit, char *perpkg, char *metric_expr,
				char *metric_name, char *metric_group),
		void *data);
char *get_cpu_str(void);

#endif
