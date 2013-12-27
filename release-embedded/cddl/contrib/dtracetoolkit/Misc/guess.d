#!/usr/sbin/dtrace -wqs
/*
 * guess.d - guessing game in D (DTrace)
 *
 * $Id: guess.d 32 2007-09-15 05:08:49Z brendan $
 *
 * USAGE: guess.d
 *
 * SEE: http://www.brendangregg.com/guessinggame.html
 *
 * This is written to demonstrate this language versus the same program
 * written in other languages.
 *
 * 11-May-2005	Brendan Gregg	Created this.
 */

inline string scorefile = "highscores_d";

dtrace:::BEGIN
{
	printf("guess.d - Guess a number between 1 and 100\n\n");
	num = 1;
	state = 1;

	/* Generate random number */
	answer = (rand() % 100) + 1;
	answer = answer > 0 ? answer : - answer;
}

syscall::write:entry
/state == 1 && pid == $pid/
{
	state = 2;
	printf("Enter guess %d: ", num);
	system("read guess");
	pos = 0;
}

syscall::read:entry
/state == 2 && ppid == $pid && arg0 == 3/
{
	self->inguess = 1;
	self->buf = arg1;
}

syscall::read:return
/self->inguess/
{
	key = copyin(self->buf, arg0);
	keys[pos] = *(char *)key;
	self->buf = 0;
	pos++;
}

syscall::read:return
/self->inguess && keys[pos-1] == '\n'/
{
	pos -= 2;
	fac = 1;
	guess = fac * (keys[pos] - '0');
	pos--;
	fac *= 10;
	guess = pos >= 0 ? guess + fac * (keys[pos] - '0') : guess;
	pos--;
	fac *= 10;
	guess = pos >= 0 ? guess + fac * (keys[pos] - '0') : guess;
	self->doneguess = 1;
}

syscall::read:return
/self->inguess/
{
	self->inguess = 0;
}

/* Play game */
syscall::read:return
/self->doneguess && guess == answer/
{
	printf("Correct! That took %d guesses.\n\n", num);
	self->doneguess = 0;
	state = 3;
	printf("Please enter your name: ");
	system("/usr/bin/read name");
}

syscall::read:return
/self->doneguess && guess != answer/
{
	num++;

	printf("%s...\n", guess < answer ? "Higher" : "Lower");

	printf("Enter guess %d: ", num);
	system("read line");
	pos = 0;
}

syscall::read:entry
/state == 3 && curthread->t_procp->p_parent->p_ppid == $pid && arg0 == 0/
{
	self->inname = 1;
	self->buf = arg1;
}

/* Save high score */
syscall::read:return
/self->inname/
{
	self->inname = 0;
	name = stringof(copyin(self->buf, arg0 - 1));
	system("echo %s %d >> %s", name, num, scorefile);

	/* Print high scores */
	printf("\nPrevious high scores,\n");
	system("cat %s", scorefile);
	exit(0);
}
