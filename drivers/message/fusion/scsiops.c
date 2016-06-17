
static const char *ScsiOpcodeString[256] = {
	"TEST UNIT READY\0\01",				/* 00h */
	"REWIND\0\002"
		"\001REZERO UNIT",			/* 01h */
	"\0\0",						/* 02h */
	"REQUEST SENSE\0\01",				/* 03h */
	"FORMAT UNIT\0\03"
		"\001FORMAT MEDIUM\0"
		"\002FORMAT",				/* 04h */
	"READ BLOCK LIMITS\0\1",			/* 05h */
	"\0\0",						/* 06h */
	"REASSIGN BLOCKS\0\02"
		"\010INITIALIZE ELEMENT STATUS",	/* 07h */
	"READ(06)\0\04"
		"\001READ\0"
		"\003RECEIVE\0"
		"\011GET MESSAGE(06)",			/* 08h */
	"\0\0",						/* 09h */
	"WRITE(06)\0\05"
		"\001WRITE\0"
		"\002PRINT\0"
		"\003SEND(6)\0"
		"\011SEND MESSAGE(06)",			/* 0Ah */
	"SEEK(06)\0\02"
		"\003SLEW AND PRINT",			/* 0Bh */
	"\0\0",						/* 0Ch */
	"\0\0",						/* 0Dh */
	"\0\0",						/* 0Eh */
	"READ REVERSE\0\01",				/* 0Fh */
	"WRITE FILEMARKS\0\02"
		"\003SYNCRONIZE BUFFER",		/* 10h */
	"SPACE(6)\0\01",				/* 11h */
	"INQUIRY\0\01",					/* 12h */
	"VERIFY\0\01",					/* 13h */
	"RECOVER BUFFERED DATA\0\01",			/* 14h */
	"MODE SELECT(06)\0\01",				/* 15h */
	"RESERVE(06)\0\02"
		"\010RESERVE ELEMENT(06)",		/* 16h */
	"RELEASE(06)\0\02"
		"\010RELEASE ELEMENT(06)",		/* 17h */
	"COPY\0\01",					/* 18h */
	"ERASE\0\01",					/* 19h */
	"MODE SENSE(06)\0\01",				/* 1Ah */
	"STOP START UNIT\0\04"
		"\001LOAD UNLOAD\0"
		"\002STOP PRINT\0"
		"\006SCAN\0\002",			/* 1Bh */
	"RECEIVE DIAGNOSTIC RESULTS\0\01",		/* 1Ch */
	"SEND DIAGNOSTIC\0\01",				/* 1Dh */
	"PREVENT ALLOW MEDIUM REMOVAL\0\01",		/* 1Eh */
	"\0\0",						/* 1Fh */
	"\0\0",						/* 20h */
	"\0\0",						/* 21h */
	"\0\0",						/* 22h */
	"READ FORMAT CAPACITIES\0\01",			/* 23h */
	"SET WINDOW\0\01",				/* 24h */
	"READ CAPACITY\0\03"
		"\006GET WINDOW\0"
		"\037FREAD CARD CAPACITY",		/* 25h */
	"\0\0",						/* 26h */
	"\0\0",						/* 27h */
	"READ(10)\0\02"
		"\011GET MESSAGE(10)",			/* 28h */
	"READ GENERATION\0\01",				/* 29h */
	"WRITE(10)\0\03"
		"\011SEND(10)\0"
		"\011SEND MESSAGE(10)",			/* 2Ah */
	"SEEK(10)\0\03"
		"LOCATE(10)\0"
		"POSITION TO ELEMENT",			/* 2Bh */
	"ERASE(10)\0\01",				/* 2Ch */
	"READ UPDATED BLOCK\0\01",			/* 2Dh */
	"WRITE AND VERIFY(10)\0\01",			/* 2Eh */
	"VERIFY(10)\0\01",				/* 2Fh */
	"SEARCH DATA HIGH(10)\0\01",			/* 30h */
	"SEARCH DATA EQUAL(10)\0\02"
		"OBJECT POSITION",			/* 31h */
	"SEARCH DATA LOW(10)\0\01",			/* 32h */
	"SET LIMITS(10)\0\01",				/* 33h */
	"PRE-FETCH(10)\0\03"
		"READ POSITION\0"
		"GET DATA BUFFER STATUS",		/* 34h */
	"SYNCHRONIZE CACHE(10)\0\01",			/* 35h */
	"LOCK UNLOCK CACHE(10)\0\01",			/* 36h */
	"READ DEFECT DATA(10)\0\01",			/* 37h */
	"MEDIUM SCAN\0\01",				/* 38h */
	"COMPARE\0\01",					/* 39h */
	"COPY AND VERIFY\0\01",				/* 3Ah */
	"WRITE BUFFER\0\01",				/* 3Bh */
	"READ BUFFER\0\01",				/* 3Ch */
	"UPDATE BLOCK\0\01",				/* 3Dh */
	"READ LONG\0\01",				/* 3Eh */
	"WRITE LONG\0\01",				/* 3Fh */
	"CHANGE DEFINITION\0\01",			/* 40h */
	"WRITE SAME(10)\0\01",				/* 41h */
	"READ SUB-CHANNEL\0\01",			/* 42h */
	"READ TOC/PMA/ATIP\0\01",			/* 43h */
	"REPORT DENSITY SUPPORT\0\01",			/* 44h */
	"READ HEADER\0\01",				/* 44h */
	"PLAY AUDIO(10)\0\01",				/* 45h */
	"GET CONFIGURATION\0\01",			/* 46h */
	"PLAY AUDIO MSF\0\01",				/* 47h */
	"PLAY AUDIO TRACK INDEX\0\01",			/* 48h */
	"PLAY TRACK RELATIVE(10)\0\01",			/* 49h */
	"GET EVENT STATUS NOTIFICATION\0\01",		/* 4Ah */
	"PAUSE/RESUME\0\01",				/* 4Bh */
	"LOG SELECT\0\01",				/* 4Ch */
	"LOG SENSE\0\01",				/* 4Dh */
	"STOP PLAY/SCAN\0\01",				/* 4Eh */
	"\0\0",						/* 4Fh */
	"XDWRITE(10)\0\01",				/* 50h */
	"XPWRITE(10)\0\02"
		"READ DISC INFORMATION",		/* 51h */
	"XDREAD(10)\0\01"
		"READ TRACK INFORMATION",		/* 52h */
	"RESERVE TRACK\0\01",				/* 53h */
	"SEND OPC INFORMATION\0\01",			/* 54h */
	"MODE SELECT(10)\0\01",				/* 55h */
	"RESERVE(10)\0\02"
		"RESERVE ELEMENT(10)",			/* 56h */
	"RELEASE(10)\0\02"
		"RELEASE ELEMENT(10)",			/* 57h */
	"REPAIR TRACK\0\01",				/* 58h */
	"READ MASTER CUE\0\01",				/* 59h */
	"MODE SENSE(10)\0\01",				/* 5Ah */
	"CLOSE TRACK/SESSION\0\01",			/* 5Bh */
	"READ BUFFER CAPACITY\0\01",			/* 5Ch */
	"SEND CUE SHEET\0\01",				/* 5Dh */
	"PERSISTENT RESERVE IN\0\01",			/* 5Eh */
	"PERSISTENT RESERVE OUT\0\01",			/* 5Fh */
	"\0\0",						/* 60h */
	"\0\0",						/* 61h */
	"\0\0",						/* 62h */
	"\0\0",						/* 63h */
	"\0\0",						/* 64h */
	"\0\0",						/* 65h */
	"\0\0",						/* 66h */
	"\0\0",						/* 67h */
	"\0\0",						/* 68h */
	"\0\0",						/* 69h */
	"\0\0",						/* 6Ah */
	"\0\0",						/* 6Bh */
	"\0\0",						/* 6Ch */
	"\0\0",						/* 6Dh */
	"\0\0",						/* 6Eh */
	"\0\0",						/* 6Fh */
	"\0\0",						/* 70h */
	"\0\0",						/* 71h */
	"\0\0",						/* 72h */
	"\0\0",						/* 73h */
	"\0\0",						/* 74h */
	"\0\0",						/* 75h */
	"\0\0",						/* 76h */
	"\0\0",						/* 77h */
	"\0\0",						/* 78h */
	"\0\0",						/* 79h */
	"\0\0",						/* 7Ah */
	"\0\0",						/* 7Bh */
	"\0\0",						/* 7Ch */
	"\0\0",						/* 7Eh */
	"\0\0",						/* 7Eh */
	"\0\0",						/* 7Fh */
	"XDWRITE EXTENDED(16)\0\01",			/* 80h */
	"REBUILD(16)\0\01",				/* 81h */
	"REGENERATE(16)\0\01",				/* 82h */
	"EXTENDED COPY\0\01",				/* 83h */
	"RECEIVE COPY RESULTS\0\01",			/* 84h */
	"ACCESS CONTROL IN  [proposed]\0\01",		/* 86h */
	"ACCESS CONTROL OUT  [proposed]\0\01",		/* 87h */
	"READ(16)\0\01",				/* 88h */
	"DEVICE LOCKS  [proposed]\0\01",		/* 89h */
	"WRITE(16)\0\01",				/* 8Ah */
	"\0\0",						/* 8Bh */
	"READ ATTRIBUTES [proposed]\0\01",		/* 8Ch */
	"WRITE ATTRIBUTES [proposed]\0\01",		/* 8Dh */
	"WRITE AND VERIFY(16)\0\01",			/* 8Eh */
	"VERIFY(16)\0\01",				/* 8Fh */
	"PRE-FETCH(16)\0\01",				/* 90h */
	"SYNCHRONIZE CACHE(16)\0\02"
		"SPACE(16) [1]",			/* 91h */
	"LOCK UNLOCK CACHE(16)\0\02"
		"LOCATE(16) [1]",			/* 92h */
	"WRITE SAME(16)\0\01",				/* 93h */
	"[usage proposed by SCSI Socket Services project]\0\01",	/* 94h */
	"[usage proposed by SCSI Socket Services project]\0\01",	/* 95h */
	"[usage proposed by SCSI Socket Services project]\0\01",	/* 96h */
	"[usage proposed by SCSI Socket Services project]\0\01",	/* 97h */
	"MARGIN CONTROL [proposed]\0\01",		/* 98h */
	"\0\0",						/* 99h */
	"\0\0",						/* 9Ah */
	"\0\0",						/* 9Bh */
	"\0\0",						/* 9Ch */
	"\0\0",						/* 9Dh */
	"SERVICE ACTION IN [proposed]\0\01",		/* 9Eh */
	"SERVICE ACTION OUT [proposed]\0\01",		/* 9Fh */
	"REPORT LUNS\0\01",				/* A0h */
	"BLANK\0\01",					/* A1h */
	"SEND EVENT\0\01",				/* A2h */
	"MAINTENANCE (IN)\0\02"
		"SEND KEY",				/* A3h */
	"MAINTENANCE (OUT)\0\02"
		"REPORT KEY",				/* A4h */
	"MOVE MEDIUM\0\02"
		"PLAY AUDIO(12)",			/* A5h */
	"EXCHANGE MEDIUM\0\02"
		"LOAD/UNLOAD C/DVD",			/* A6h */
	"MOVE MEDIUM ATTACHED\0\02"
		"SET READ AHEAD\0\01",			/* A7h */
	"READ(12)\0\02"
		"GET MESSAGE(12)",			/* A8h */
	"PLAY TRACK RELATIVE(12)\0\01",			/* A9h */
	"WRITE(12)\0\02"
		"SEND MESSAGE(12)",			/* AAh */
	"\0\0",						/* ABh */
	"ERASE(12)\0\02"
		"GET PERFORMANCE",			/* ACh */
	"READ DVD STRUCTURE\0\01",			/* ADh */
	"WRITE AND VERIFY(12)\0\01",			/* AEh */
	"VERIFY(12)\0\01",				/* AFh */
	"SEARCH DATA HIGH(12)\0\01",			/* B0h */
	"SEARCH DATA EQUAL(12)\0\01",			/* B1h */
	"SEARCH DATA LOW(12)\0\01",			/* B2h */
	"SET LIMITS(12)\0\01",				/* B3h */
	"READ ELEMENT STATUS ATTACHED\0\01",		/* B4h */
	"REQUEST VOLUME ELEMENT ADDRESS\0\01",		/* B5h */
	"SEND VOLUME TAG\0\02"
		"SET STREAMING",			/* B6h */
	"READ DEFECT DATA(12)\0\01",			/* B7h */
	"READ ELEMENT STATUS\0\01",			/* B8h */
	"READ CD MSF\0\01",				/* B9h */
	"REDUNDANCY GROUP (IN)\0\02"
		"SCAN",					/* BAh */
	"REDUNDANCY GROUP (OUT)\0\02"
		"SET CD-ROM SPEED",			/* BBh */
	"SPARE (IN)\0\02"
		"PLAY CD",				/* BCh */
	"SPARE (OUT)\0\02"
		"MECHANISM STATUS",			/* BDh */
	"VOLUME SET (IN)\0\02"
		"READ CD",				/* BEh */
	"VOLUME SET (OUT)\0\0\02"
		"SEND DVD STRUCTURE",			/* BFh */
	"\0\0",						/* C0h */
	"\0\0",						/* C1h */
	"\0\0",						/* C2h */
	"\0\0",						/* C3h */
	"\0\0",						/* C4h */
	"\0\0",						/* C5h */
	"\0\0",						/* C6h */
	"\0\0",						/* C7h */
	"\0\0",						/* C8h */
	"\0\0",						/* C9h */
	"\0\0",						/* CAh */
	"\0\0",						/* CBh */
	"\0\0",						/* CCh */
	"\0\0",						/* CDh */
	"\0\0",						/* CEh */
	"\0\0",						/* CFh */
	"\0\0",						/* D0h */
	"\0\0",						/* D1h */
	"\0\0",						/* D2h */
	"\0\0",						/* D3h */
	"\0\0",						/* D4h */
	"\0\0",						/* D5h */
	"\0\0",						/* D6h */
	"\0\0",						/* D7h */
	"\0\0",						/* D8h */
	"\0\0",						/* D9h */
	"\0\0",						/* DAh */
	"\0\0",						/* DBh */
	"\0\0",						/* DCh */
	"\0\0",						/* DEh */
	"\0\0",						/* DEh */
	"\0\0",						/* DFh */
	"\0\0",						/* E0h */
	"\0\0",						/* E1h */
	"\0\0",						/* E2h */
	"\0\0",						/* E3h */
	"\0\0",						/* E4h */
	"\0\0",						/* E5h */
	"\0\0",						/* E6h */
	"\0\0",						/* E7h */
	"\0\0",						/* E8h */
	"\0\0",						/* E9h */
	"\0\0",						/* EAh */
	"\0\0",						/* EBh */
	"\0\0",						/* ECh */
	"\0\0",						/* EDh */
	"\0\0",						/* EEh */
	"\0\0",						/* EFh */
	"\0\0",						/* F0h */
	"\0\0",						/* F1h */
	"\0\0",						/* F2h */
	"\0\0",						/* F3h */
	"\0\0",						/* F4h */
	"\0\0",						/* F5h */
	"\0\0",						/* F6h */
	"\0\0",						/* F7h */
	"\0\0",						/* F8h */
	"\0\0",						/* F9h */
	"\0\0",						/* FAh */
	"\0\0",						/* FBh */
	"\0\0",						/* FEh */
	"\0\0",						/* FEh */
	"\0\0",						/* FEh */
	"\0\0"						/* FFh */
};

