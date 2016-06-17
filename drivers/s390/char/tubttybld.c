/*
 *  IBM/3270 Driver -- Copyright (C) 2000 UTS Global LLC
 *
 *  tubttybld.c -- Linemode tty driver screen-building functions
 *
 *
 *
 *
 *
 *  Author:  Richard Hitt
 */

#include "tubio.h"

extern int tty3270_io(tub_t *);
static void tty3270_set_status_area(tub_t *, char **);
static int tty3270_next_char(tub_t *);
static void tty3270_unnext_char(tub_t *, char);
static void tty3270_update_log_area(tub_t *, char **);
static int tty3270_update_log_area_esc(tub_t *, char **, int *);
static void tty3270_clear_log_area(tub_t *, char **);
static void tty3270_tub_bufadr(tub_t *, int, char **);
static void tty3270_set_bufadr(tub_t *, char **, int *);

/*
 * tty3270_clear_log_area(tub_t *tubp, char **cpp)
 */
static void
tty3270_clear_log_area(tub_t *tubp, char **cpp)
{
	*(*cpp)++ = TO_SBA;
	TUB_BUFADR(GEOM_LOG, cpp);
	*(*cpp)++ = TO_SF;
	*(*cpp)++ = TF_LOG;
	*(*cpp)++ = TO_RA;
	TUB_BUFADR(GEOM_INPUT, cpp);
	*(*cpp)++ = '\0';
	tubp->tty_oucol = tubp->tty_nextlogx = 0;
	*(*cpp)++ = TO_SBA;
	TUB_BUFADR(tubp->tty_nextlogx, cpp);
}

static void
tty3270_update_log_area(tub_t *tubp, char **cpp)
{
	int lastx = GEOM_INPUT;
	int c;
	int next, fill, i;
	int sba_needed = 1;
	char *overrun = &(*tubp->ttyscreen)[tubp->ttyscreenl - TS_LENGTH];

	/* Check for possible ESC sequence work to do */
	if (tubp->tty_escx != 0) {
		/* If compiling new escape sequence */
		if (tubp->tty_esca[0] == 0x1b) {
			if (tty3270_update_log_area_esc(tubp, cpp, &sba_needed))
				return;
		/* If esc seq needs refreshing after a write */
		} else if (tubp->tty_esca[0] == TO_SA) {
			tty3270_set_bufadr(tubp, cpp, &sba_needed);
			for (i = 0; i < tubp->tty_escx; i++)
				*(*cpp)++ = tubp->tty_esca[i];
		} else {
			printk(KERN_WARNING "tty3270_update_log_area esca "
			"character surprising:  %.2x\n", tubp->tty_esca[0]);
		}
	}

	/* Place characters */
	while (tubp->tty_bcb.bc_cnt != 0) {
		/* Check for room.  TAB could take up to 4 chars. */
		if (&(*cpp)[4] >= overrun)
			break;

		/* Fetch a character */
		if ((c = tty3270_next_char(tubp)) == -1)
			break;

		switch(c) {
		default:
			if (tubp->tty_nextlogx >= lastx) {
				if (sba_needed == 0 ||
				    tubp->stat == TBS_RUNNING) {
					tty3270_unnext_char(tubp, c);
					tubp->stat = TBS_MORE;
					tty3270_set_status_area(tubp, cpp);
					tty3270_scl_settimer(tubp);
				}
				goto do_return;
			}
			tty3270_set_bufadr(tubp, cpp, &sba_needed);
			/* Use blank if we don't know the character */
			*(*cpp)++ = tub_ascebc[(int)(c < ' '? ' ': c)];
			tubp->tty_nextlogx++;
			tubp->tty_oucol++;
			break;
		case 0x1b:              /* ESC */
			tubp->tty_escx = 0;
			if (tty3270_update_log_area_esc(tubp, cpp, &sba_needed))
				return;
			break;
		case '\r':		/* 0x0d -- Carriage Return */
			tubp->tty_nextlogx -=
				tubp->tty_nextlogx % GEOM_COLS;
			sba_needed = 1;
			break;
		case '\n':		/* 0x0a -- New Line */
			if (tubp->tty_oucol == GEOM_COLS) {
				tubp->tty_oucol = 0;
				break;
			}
			next = (tubp->tty_nextlogx + GEOM_COLS) /
				GEOM_COLS * GEOM_COLS;
			tubp->tty_nextlogx = next;
			tubp->tty_oucol = 0;
			sba_needed = 1;
			break;
		case '\t':		/* 0x09 -- Tabulate */
			tty3270_set_bufadr(tubp, cpp, &sba_needed);
			fill = (tubp->tty_nextlogx % GEOM_COLS) % 8;
			for (; fill < 8; fill++) {
				if (tubp->tty_nextlogx >= lastx)
					break;
				*(*cpp)++ = tub_ascebc[' '];
				tubp->tty_nextlogx++;
				tubp->tty_oucol++;
			}
			break;
		case '\a':		/* 0x07 -- Alarm */
			tubp->flags |= TUB_ALARM;
			break;
		case '\f':		/* 0x0c -- Form Feed */
			tty3270_clear_log_area(tubp, cpp);
			break;
		case 0xf:	/* SuSE "exit alternate mode" */
			break;
		}
	}
do_return:
}

#define NUMQUANT 8
static int
tty3270_update_log_area_esc(tub_t *tubp, char **cpp, int *sba_needed)
{
	int c;
	int i, j;
	int start, end, next;
	int quant[NUMQUANT];
	char *overrun = &(*tubp->ttyscreen)[tubp->ttyscreenl - TS_LENGTH];
	char sabuf[NUMQUANT*3], *sap = sabuf, *cp;

	/* If starting a sequence, stuff ESC at [0] */
	if (tubp->tty_escx == 0)
		tubp->tty_esca[tubp->tty_escx++] = 0x1b;

	/* Now that sequence is started, see if room in buffer */
	if (&(*cpp)[NUMQUANT * 3] >= overrun)
		return tubp->tty_escx;

	/* Gather the rest of the sequence's characters */
	while (tubp->tty_escx < sizeof tubp->tty_esca) {
		if ((c = tty3270_next_char(tubp)) == -1)
			return tubp->tty_escx;
		if (tubp->tty_escx == 1) {
			switch(c) {
			case '[':
				tubp->tty_esca[tubp->tty_escx++] = c;
				continue;
			case '7':
				tubp->tty_savecursor = tubp->tty_nextlogx;
				goto done_return;
			case '8':
				next = tubp->tty_savecursor;
				goto do_setcur;
			default:
				goto error_return;
			}
		}
		tubp->tty_esca[tubp->tty_escx++] = c;
		if (c != ';' && (c < '0' || c > '9'))
			break;
	}

	/* Check for overrun */
	if (tubp->tty_escx == sizeof tubp->tty_esca)
		goto error_return;

	/* Parse potentially empty string "nn;nn;nn..." */
	i = -1;
	j = 2;		/* skip ESC, [ */
	c = ';';
	do {
		if (c == ';') {
			if (++i == NUMQUANT)
				goto error_return;
			quant[i] = 0;
		} else if (c < '0' || c > '9') {
			break;
		} else {
			quant[i] = quant[i] * 10 + c - '0';
		}
		c = tubp->tty_esca[j];
	} while (j++ < tubp->tty_escx);

	/* Add 3270 data stream output to execute the sequence */
	switch(c) {
	case 'm':		/* Set Attribute */
		for (next = 0; next <= i; next++) {
			int type = -1, value = 0;

			switch(quant[next]) {
			case 0:		/* Reset */
				next = tubp->tty_nextlogx;
				tty3270_set_bufadr(tubp, cpp, sba_needed);
				*(*cpp)++ = TO_SA;
				*(*cpp)++ = TAT_EXTHI;
				*(*cpp)++ = TAX_RESET;
				*(*cpp)++ = TO_SA;
				*(*cpp)++ = TAT_COLOR;
				*(*cpp)++ = TAC_RESET;
				tubp->tty_nextlogx = next;
				*sba_needed = 1;
				sap = sabuf;
				break;
			case 1:		/* Bright */
				break;
			case 2:		/* Dim */
				break;
			case 4:		/* Underscore */
				type = TAT_EXTHI; value = TAX_UNDER;
				break;
			case 5:		/* Blink */
				type = TAT_EXTHI; value = TAX_BLINK;
				break;
			case 7:		/* Reverse */
				type = TAT_EXTHI; value = TAX_REVER;
				break;
			case 8:		/* Hidden */
				break;		/* For now ... */
			/* Foreground Colors */
			case 30:	/* Black */
				type = TAT_COLOR; value = TAC_DEFAULT;
				break;
			case 31:	/* Red */
				type = TAT_COLOR; value = TAC_RED;
				break;
			case 32:	/* Green */
				type = TAT_COLOR; value = TAC_GREEN;
				break;
			case 33:	/* Yellow */
				type = TAT_COLOR; value = TAC_YELLOW;
				break;
			case 34:	/* Blue */
				type = TAT_COLOR; value = TAC_BLUE;
				break;
			case 35:	/* Magenta */
				type = TAT_COLOR; value = TAC_PINK;
				break;
			case 36:	/* Cyan */
				type = TAT_COLOR; value = TAC_TURQ;
				break;
			case 37:	/* White */
				type = TAT_COLOR; value = TAC_WHITE;
				break;
			case 39:	/* Black */
				type = TAT_COLOR; value = TAC_DEFAULT;
				break;
			/* Background Colors */
			case 40:	/* Black */
			case 41:	/* Red */
			case 42:	/* Green */
			case 43:	/* Yellow */
			case 44:	/* Blue */
			case 45:	/* Magenta */
			case 46:	/* Cyan */
			case 47:	/* White */
				break;		/* For now ... */
			/* Oops */
			default:
				break;
			}
			if (type != -1) {
				tty3270_set_bufadr(tubp, cpp, sba_needed);
				*(*cpp)++ = TO_SA;
				*(*cpp)++ = type;
				*(*cpp)++ = value;
				*sap++ = TO_SA;
				*sap++ = type;
				*sap++ = value;
			}
		}
		break;

	case 'H':		/* Cursor Home */
	case 'f':		/* Force Cursor Position */
		if (quant[0]) quant[0]--;
		if (quant[1]) quant[1]--;
		next = quant[0] * GEOM_COLS + quant[1];
		goto do_setcur;
	case 'A':		/* Cursor Up */
		if (quant[i] == 0) quant[i] = 1;
		next = tubp->tty_nextlogx - GEOM_COLS * quant[i];
		goto do_setcur;
	case 'B':		/* Cursor Down */
		if (quant[i] == 0) quant[i] = 1;
		next = tubp->tty_nextlogx + GEOM_COLS * quant[i];
		goto do_setcur;
	case 'C':		/* Cursor Forward */
		if (quant[i] == 0) quant[i] = 1;
		next = tubp->tty_nextlogx % GEOM_COLS;
		start = tubp->tty_nextlogx - next;
		next = start + MIN(next + quant[i], GEOM_COLS - 1);
		goto do_setcur;
	case 'D':		/* Cursor Backward */
		if (quant[i] == 0) quant[i] = 1;
		next = MIN(quant[i], tubp->tty_nextlogx % GEOM_COLS);
		next = tubp->tty_nextlogx - next;
		goto do_setcur;
	case 'G':
		if (quant[0]) quant[0]--;
		next = tubp->tty_nextlogx / GEOM_COLS * GEOM_COLS + quant[0];
do_setcur:
		if (next < 0)
			break;
		tubp->tty_nextlogx = next;
		tubp->tty_oucol = tubp->tty_nextlogx % GEOM_COLS;
		*sba_needed = 1;
		break;

	case 'r':		/* Define scroll area */
		start = quant[0];
		if (start <= 0) start = 1;
		if (start > GEOM_ROWS - 2) start = GEOM_ROWS - 2;
		tubp->tty_nextlogx = (start - 1) * GEOM_COLS;
		tubp->tty_oucol = 0;
		*sba_needed = 1;
		break;

	case 'X':		/* Erase for n chars from cursor */
		start = tubp->tty_nextlogx;
		end = start + (quant[0]?: 1);
		goto do_fill;
	case 'J':		/* Erase to screen end from cursor */
		*(*cpp)++ = TO_SBA;
		TUB_BUFADR(tubp->tty_nextlogx, cpp);
		*(*cpp)++ = TO_RA;
		TUB_BUFADR(GEOM_INPUT, cpp);
		*(*cpp)++ = tub_ascebc[' '];
		*(*cpp)++ = TO_SBA;
		TUB_BUFADR(tubp->tty_nextlogx, cpp);
		break;
	case 'K':
		start = tubp->tty_nextlogx;
		end = (start + GEOM_COLS) / GEOM_COLS * GEOM_COLS;
do_fill:
		if (start >= GEOM_INPUT)
			break;
		if (end > GEOM_INPUT)
			end = GEOM_INPUT;
		if (end <= start)
			break;
		*(*cpp)++ = TO_SBA;
		TUB_BUFADR(start, cpp);
		if (end - start > 4) {
			*(*cpp)++ = TO_RA;
			TUB_BUFADR(end, cpp);
			*(*cpp)++ = tub_ascebc[' '];
		} else while (start++ < end) {
			*(*cpp)++ = tub_ascebc[' '];
		}
		tubp->tty_nextlogx = end;
		tubp->tty_oucol = tubp->tty_nextlogx % GEOM_COLS;
		*sba_needed = 1;
		break;
	}
done_return:
	tubp->tty_escx = 0;
	cp = sabuf;
	while (cp != sap)
		tubp->tty_esca[tubp->tty_escx++] = *cp++;
	return 0;
error_return:
	tubp->tty_escx = 0;
	return 0;
}

static int
tty3270_next_char(tub_t *tubp)
{
	int c;
	bcb_t *ib;

	ib = &tubp->tty_bcb;
	if (ib->bc_cnt == 0)
		return -1;
	c = ib->bc_buf[ib->bc_rd++];
	if (ib->bc_rd == ib->bc_len)
		ib->bc_rd = 0;
	ib->bc_cnt--;
	return c;
}

static void
tty3270_unnext_char(tub_t *tubp, char c)
{
	bcb_t *ib;

	ib = &tubp->tty_bcb;
	if (ib->bc_rd == 0)
		ib->bc_rd = ib->bc_len;
	ib->bc_buf[--ib->bc_rd] = c;
	ib->bc_cnt++;
}


static void
tty3270_clear_input_area(tub_t *tubp, char **cpp)
{
	*(*cpp)++ = TO_SBA;
	TUB_BUFADR(GEOM_INPUT, cpp);
	*(*cpp)++ = TO_SF;
	*(*cpp)++ = tubp->tty_inattr;
	*(*cpp)++ = TO_IC;
	*(*cpp)++ = TO_RA;
	TUB_BUFADR(GEOM_STAT, cpp);
	*(*cpp)++ = '\0';
}

static void
tty3270_update_input_area(tub_t *tubp, char **cpp)
{
	int len;

	*(*cpp)++ = TO_SBA;
	TUB_BUFADR(GEOM_INPUT, cpp);
	*(*cpp)++ = TO_SF;
	*(*cpp)++ = TF_INMDT;
	len = strlen(tubp->tty_input);
	memcpy(*cpp, tubp->tty_input, len);
	*cpp += len;
	*(*cpp)++ = TO_IC;
	len = GEOM_INPLEN - len;
	if (len > 4) {
		*(*cpp)++ = TO_RA;
		TUB_BUFADR(GEOM_STAT, cpp);
		*(*cpp)++ = '\0';
	} else {
		for (; len > 0; len--)
			*(*cpp)++ = '\0';
	}
}

/*
 * tty3270_set_status_area(tub_t *tubp, char **cpp)
 */
static void
tty3270_set_status_area(tub_t *tubp, char **cpp)
{
	char *sp;

	if (tubp->stat == TBS_RUNNING)
		sp = TS_RUNNING;
	else if (tubp->stat == TBS_MORE)
		sp = TS_MORE;
	else if (tubp->stat == TBS_HOLD)
		sp = TS_HOLD;
	else
		sp = "Linux Whatstat";

	*(*cpp)++ = TO_SBA;
	TUB_BUFADR(GEOM_STAT, cpp);
	*(*cpp)++ = TO_SF;
	*(*cpp)++ = TF_STAT;
	memcpy(*cpp, sp, sizeof TS_RUNNING);
	TUB_ASCEBC(*cpp, sizeof TS_RUNNING);
	*cpp += sizeof TS_RUNNING;
}

/*
 * tty3270_build() -- build an output stream
 */
int
tty3270_build(tub_t *tubp)
{
	char *cp, *startcp;
	int chancmd;
	int writecc = TW_KR;
	int force = 0;

	if (tubp->mode == TBM_FS)
		return 0;

	cp = startcp = *tubp->ttyscreen + 1;

	switch(tubp->cmd) {
	default:
		printk(KERN_WARNING "tty3270_build unknown command %d\n", tubp->cmd);
		return 0;
	case TBC_OPEN:
tbc_open:
		tubp->flags &= ~TUB_INPUT_HACK;
		chancmd = TC_EWRITEA;
		tty3270_clear_input_area(tubp, &cp);
		tty3270_set_status_area(tubp, &cp);
		tty3270_clear_log_area(tubp, &cp);
		break;
	case TBC_UPDLOG:
		if (tubp->flags & TUB_INPUT_HACK)
			goto tbc_open;
		chancmd = TC_WRITE;
		writecc = TW_NONE;
		tty3270_update_log_area(tubp, &cp);
		break;
	case TBC_KRUPDLOG:
		chancmd = TC_WRITE;
		force = 1;
		tty3270_update_log_area(tubp, &cp);
		break;
	case TBC_CLRUPDLOG:
		chancmd = TC_WRITE;
		tty3270_set_status_area(tubp, &cp);
		tty3270_clear_log_area(tubp, &cp);
		tty3270_update_log_area(tubp, &cp);
		break;
	case TBC_UPDATE:
		chancmd = TC_EWRITEA;
		tubp->tty_oucol = tubp->tty_nextlogx = 0;
		tty3270_clear_input_area(tubp, &cp);
		tty3270_set_status_area(tubp, &cp);
		tty3270_update_log_area(tubp, &cp);
		break;
	case TBC_UPDSTAT:
		chancmd = TC_WRITE;
		tty3270_set_status_area(tubp, &cp);
		break;
	case TBC_CLRINPUT:
		chancmd = TC_WRITE;
		tty3270_clear_input_area(tubp, &cp);
		break;
	case TBC_UPDINPUT:
		chancmd = TC_WRITE;
		tty3270_update_input_area(tubp, &cp);
		break;
	}

	/* Set Write Control Character and start I/O */
	if (force == 0 && cp == startcp &&
	    (tubp->flags & TUB_ALARM) == 0)
		return 0;
	if (tubp->flags & TUB_ALARM) {
		tubp->flags &= ~TUB_ALARM;
		writecc |= TW_PLUSALARM;
	}
	**tubp->ttyscreen = writecc;
	tubp->ttyccw.cmd_code = chancmd;
	tubp->ttyccw.flags = CCW_FLAG_SLI;
	tubp->ttyccw.cda = virt_to_phys(*tubp->ttyscreen);
	tubp->ttyccw.count = cp - *tubp->ttyscreen;
	tty3270_io(tubp);
	return 1;
}

static void
tty3270_tub_bufadr(tub_t *tubp, int adr, char **cpp)
{
	if (tubp->tty_14bitadr) {
		*(*cpp)++ = (adr >> 8) & 0x3f;
		*(*cpp)++ = adr & 0xff;
	} else {
		*(*cpp)++ = tub_ebcgraf[(adr >> 6) & 0x3f];
		*(*cpp)++ = tub_ebcgraf[adr & 0x3f];
	}
}

static void
tty3270_set_bufadr(tub_t *tubp, char **cpp, int *sba_needed)
{
	if (!*sba_needed)
		return;
	if (tubp->tty_nextlogx >= GEOM_INPUT) {
		tubp->tty_nextlogx = GEOM_INPUT - 1;
		tubp->tty_oucol = tubp->tty_nextlogx % GEOM_COLS;
	}
	*(*cpp)++ = TO_SBA;
	TUB_BUFADR(tubp->tty_nextlogx, cpp);
	*sba_needed = 0;
}
