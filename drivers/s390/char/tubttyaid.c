/*
 *  IBM/3270 Driver -- Copyright (C) 2000 UTS Global LLC
 *
 *  tubttyaid.c -- Linemode Attention-ID functionality
 *
 *
 *
 *
 *
 *  Author:  Richard Hitt
 */
#include "tubio.h"

#define PA1_STR "^C"
#define PF3_STR "^D"
#define PF9_STR "\033j"
#define PF10_STR "\033k"
#define PF11_STR "\033j"
/* other AID-key default strings */

aid_t aidtab[64] = {
/* 00         */	{ 0, 0 },
/* C1 = PF13  */	{ TA_DOSTRING, 0 },
/* C2 = PF14  */	{ TA_DOSTRING, 0 },
/* C3 = PF15  */	{ TA_DOSTRING, 0 },
/* C4 = PF16  */	{ TA_DOSTRING, 0 },
/* C5 = PF17  */	{ TA_DOSTRING, 0 },
/* C6 = PF18  */	{ TA_DOSTRING, 0 },
/* C7 = PF19  */	{ TA_DOSTRING, 0 },
/* C8 = PF20  */	{ TA_DOSTRING, 0 },
/* C9 = PF21  */	{ TA_DOSTRING, 0 },
/* 4A = PF22  */	{ TA_DOSTRING, 0 },
/* 4B = PF23  */	{ TA_DOSTRING, 0 },
/* 4C = PF24  */	{ TA_DOSTRING, 0 },
/* 0D         */	{ 0, 0 },
/* 0E         */	{ 0, 0 },
/* 0F         */	{ 0, 0 },
/* 10         */	{ 0, 0 },
/* 11         */	{ 0, 0 },
/* 12         */	{ 0, 0 },
/* 13         */	{ 0, 0 },
/* 14         */	{ 0, 0 },
/* 15         */	{ 0, 0 },
/* 16         */	{ 0, 0 },
/* 17         */	{ 0, 0 },
/* 18         */	{ 0, 0 },
/* 19         */	{ 0, 0 },
/* 1A         */	{ 0, 0 },
/* 1B         */	{ 0, 0 },
/* 1C         */	{ 0, 0 },
/* 1D         */	{ 0, 0 },
/* 1E         */	{ 0, 0 },
/* 1F         */	{ 0, 0 },
/* 60 = NoAID */	{ 0, 0 },
/* 21         */	{ 0, 0 },
/* 22         */	{ 0, 0 },
/* 23         */	{ 0, 0 },
/* 24         */	{ 0, 0 },
/* 25         */	{ 0, 0 },
/* E6 = OpRdr */	{ 0, 0 },
/* E7 = MSRdr */	{ 0, 0 },
/* E8 = NoAID */	{ 0, 0 },
/* 29         */	{ 0, 0 },
/* 2A         */	{ 0, 0 },
/* 6B =  PA3  */        { TA_SHORTREAD, 0 },
/* 6C =  PA1  */        { TA_SHORTREAD | TA_DOSTRING, PA1_STR },
/* 6D = CLEAR */        { TA_SHORTREAD | TA_CLEARKEY, 0 },
/* 6E =  PA2  */        { TA_SHORTREAD | TA_CLEARLOG, 0 },
/* 2F         */	{ 0, 0 },
/* F0 = TstRq */        { 0, 0 },
/* F1 =  PF1  */	{ TA_DOSTRING, 0 },
/* F2 =  PF2  */	{ TA_DOSTRING, 0 },
/* F3 =  PF3  */        { TA_DOSTRING, PF3_STR },
/* F4 =  PF4  */	{ TA_DOSTRING, 0 },
/* F5 =  PF5  */	{ TA_DOSTRING, 0 },
/* F6 =  PF6  */	{ TA_DOSTRING, 0 },
/* F7 =  PF7  */	{ TA_DOSTRING, 0 },
/* F8 =  PF8  */	{ TA_DOSTRING, 0 },
/* F9 =  PF9  */        { TA_DOSTRING, PF9_STR },
/* 7A = PF10  */        { TA_DOSTRING, PF10_STR },
/* 7B = PF11  */        { TA_DOSTRING, PF11_STR },
/* 7C = PF12  */	{ TA_DOSTRING, 0 },
/* 7D = ENTER */        { TA_DOENTER, 0 },
/* 7E = Pen   */        { 0, 0 },
/* 3F         */	{ 0, 0 },
};

int
tty3270_aid_init(tub_t *tubp)
{
	memcpy(tubp->tty_aid, aidtab, sizeof aidtab);
	tubp->tty_aidinit = 1;
	return 0;
}

void
tty3270_aid_fini(tub_t *tubp)
{
	int i;
	char *sp;

	if (tubp->tty_aidinit == 0)
		return;
	for (i = 0; i < 64; i++) {
		if ((sp = tubp->tty_aid[i].string) == NULL)
			continue;
		if (sp == aidtab[i].string)
			continue;
		kfree(sp);
	}
	tubp->tty_aidinit = 0;
}

void
tty3270_aid_reinit(tub_t *tubp)
{
	tty3270_aid_fini(tubp);
	tty3270_aid_init(tubp);
}

int
tty3270_aid_get(tub_t *tubp, int aid, int *aidflags, char **aidstring)
{
	aid_t *ap;

	ap = AIDENTRY(aid, tubp);
	*aidflags = ap->aid;
	*aidstring = ap->string;
	return 0;
}

/*
 * tty3270_aid_set() -- write_proc extension
 * Parse written string as an AID name.  Return 0 if it's not.
 * Otherwise absorb the string and return count or -error.
 */
int
tty3270_aid_set(tub_t *tubp, char *buf, int count)
{
	char name[8];
	char *sp;
	int aidn, aidx;
	aid_t *ap;
	int len;
	char *pfp;

	if (tubp->tty_aidinit == 0)
		return 0;
	if (count < 3)          /* If AID-key name too short */
		return 0;
	name[0] = buf[0] < 0x60? buf[0]: (buf[0] & 0x5f);
	name[1] = buf[1] < 0x60? buf[1]: (buf[1] & 0x5f);
	if (name[0] == 'P' && name[1] == 'F') {
		aidn = simple_strtoul(buf+2, &sp, 10);
		if (aidn < 1 || aidn > 24)
			return 0;
		aidx = aidn > 12? aidn - 12: aidn + 0x30;
		ap = &tubp->tty_aid[aidx];
	} else if (name[0] == 'P' && name[1] == 'A') {
		aidn = simple_strtoul(buf+2, &sp, 10);
		if (aidn < 1 || aidn > 3)
			return 0;
		switch(aidn) {
		case 1:  aidx = 0x2c; break;
		case 2:  aidx = 0x2e; break;
		case 3:  aidx = 0x2b; break;
		default:  aidx = 0; break;
		}
		ap = &tubp->tty_aid[aidx];
	} else {
		return 0;
	}

	if (*sp == '\0') {
		tubp->tty_showaidx = ap - tubp->tty_aid;
		return count;
	} else if (*sp == '=') {
		len = strlen(++sp);
		if (len == 0) {
			if (ap->string != NULL &&
			    ap->string != aidtab[aidx].string)
				kfree(ap->string);
			ap->string = aidtab[aidx].string;
			ap->aid = aidtab[aidx].aid;
			return count;
		}
		if ((pfp = kmalloc(len + 1, GFP_KERNEL)) == NULL)
			return -ENOMEM;
		if (ap->string != NULL &&
		    ap->string != aidtab[aidx].string)
			kfree(ap->string);
		if (sp[len - 1] == '\n') {
			ap->aid = TA_DOSTRING;
			sp[len - 1] = '\0';
			len--;
		} else {
			ap->aid = TA_DOSTRINGD;
		}
		memcpy(pfp, sp, len + 1);
		ap->string = pfp;
		return count;
	} else {
		return -EINVAL;
	}
}
