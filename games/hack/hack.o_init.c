/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* hack.o_init.c - version 1.0.3 */
/* $FreeBSD$ */

#include <string.h>
#include	"config.h"		/* for typedefs */
#include	"def.objects.h"
#include	"hack.onames.h"		/* for LAST_GEM */
extern char *index();

int
letindex(let) char let; {
int i = 0;
char ch;
	while((ch = obj_symbols[i++]) != 0)
		if(ch == let) return(i);
	return(0);
}

init_objects(){
int i, j, first, last, sum, end;
char let, *tmp;
	/* init base; if probs given check that they add up to 100,
	   otherwise compute probs; shuffle descriptions */
	end = SIZE(objects);
	first = 0;
	while( first < end ) {
		let = objects[first].oc_olet;
		last = first+1;
		while(last < end && objects[last].oc_olet == let
				&& objects[last].oc_name != NULL)
			last++;
		i = letindex(let);
		if((!i && let != ILLOBJ_SYM) || bases[i] != 0)
			error("initialization error");
		bases[i] = first;

		if(let == GEM_SYM)
			setgemprobs();
	check:
		sum = 0;
		for(j = first; j < last; j++) sum += objects[j].oc_prob;
		if(sum == 0) {
			for(j = first; j < last; j++)
			    objects[j].oc_prob = (100+j-first)/(last-first);
			goto check;
		}
		if(sum != 100)
			error("init-prob error for %c", let);

		if(objects[first].oc_descr != NULL && let != TOOL_SYM){
			/* shuffle, also some additional descriptions */
			while(last < end && objects[last].oc_olet == let)
				last++;
			j = last;
			while(--j > first) {
				i = first + rn2(j+1-first);
				tmp = objects[j].oc_descr;
				objects[j].oc_descr = objects[i].oc_descr;
				objects[i].oc_descr = tmp;
			}
		}
		first = last;
	}
}

probtype(let) char let; {
int i = bases[letindex(let)];
int prob = rn2(100);
	while((prob -= objects[i].oc_prob) >= 0) i++;
	if(objects[i].oc_olet != let || !objects[i].oc_name)
		panic("probtype(%c) error, i=%d", let, i);
	return(i);
}

setgemprobs()
{
	int j,first;
	extern xchar dlevel;

	first = bases[letindex(GEM_SYM)];

	for(j = 0; j < 9-dlevel/3; j++)
		objects[first+j].oc_prob = 0;
	first += j;
	if(first >= LAST_GEM || first >= SIZE(objects) ||
	    objects[first].oc_olet != GEM_SYM ||
	    objects[first].oc_name == NULL)
		printf("Not enough gems? - first=%d j=%d LAST_GEM=%d\n",
			first, j, LAST_GEM);
	for(j = first; j < LAST_GEM; j++)
		objects[j].oc_prob = (20+j-first)/(LAST_GEM-first);
}

oinit()			/* level dependent initialization */
{
	setgemprobs();
}

extern long *alloc();

savenames(fd) int fd; {
int i;
unsigned len;
	bwrite(fd, (char *) bases, sizeof bases);
	bwrite(fd, (char *) objects, sizeof objects);
	/* as long as we use only one version of Hack/Quest we
	   need not save oc_name and oc_descr, but we must save
	   oc_uname for all objects */
	for(i=0; i < SIZE(objects); i++) {
		if(objects[i].oc_uname) {
			len = strlen(objects[i].oc_uname)+1;
			bwrite(fd, (char *) &len, sizeof len);
			bwrite(fd, objects[i].oc_uname, len);
		}
	}
}

restnames(fd) int fd; {
int i;
unsigned len;
	mread(fd, (char *) bases, sizeof bases);
	mread(fd, (char *) objects, sizeof objects);
	for(i=0; i < SIZE(objects); i++) if(objects[i].oc_uname) {
		mread(fd, (char *) &len, sizeof len);
		objects[i].oc_uname = (char *) alloc(len);
		mread(fd, objects[i].oc_uname, len);
	}
}

dodiscovered()				/* free after Robert Viduya */
{
    extern char *typename();
    int i, end;
    int	ct = 0;

    cornline(0, "Discoveries");

    end = SIZE(objects);
    for (i = 0; i < end; i++) {
	if (interesting_to_discover (i)) {
	    ct++;
	    cornline(1, typename(i));
	}
    }
    if (ct == 0) {
	pline ("You haven't discovered anything yet...");
	cornline(3, (char *) 0);
    } else
	cornline(2, (char *) 0);

    return(0);
}

interesting_to_discover(i)
int i;
{
    return(
	objects[i].oc_uname != NULL ||
	 (objects[i].oc_name_known && objects[i].oc_descr != NULL)
    );
}
