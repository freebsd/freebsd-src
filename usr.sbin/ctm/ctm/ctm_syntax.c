/*
 * We redefine the names to make it look nice...
 */

#include "ctm.h"

/* The fields... */
#define Name	CTM_F_Name
#define Uid	CTM_F_Uid
#define Gid	CTM_F_Gid
#define Mode	CTM_F_Mode
#define MD5	CTM_F_MD5
#define Count	CTM_F_Count
#define Bytes	CTM_F_Bytes

/* The qualifiers... */
#define File	CTM_Q_Name_File
#define Dir	CTM_Q_Name_Dir
#define New	CTM_Q_Name_New
#define After	CTM_Q_MD5_After
#define Before	CTM_Q_MD5_Before
#define Chunk	CTM_Q_MD5_Chunk

static int ctmFM[] = /* File Make */
    { Name|File|New, Uid, Gid, Mode, MD5|After|Chunk, Count, Bytes,0 };

static int ctmFS[] = /* File Substitute */
    { Name|File, Uid, Gid, Mode, MD5|Before, MD5|After|Chunk, Count, Bytes,0 };

static int ctmFE[] = /* File Edit */
    { Name|File, Uid, Gid, Mode, MD5|Before, MD5|After, Count, Bytes,0 };

static int ctmFR[] = /* File Remove */
    { Name|File, MD5|Before, 0 };

static int ctmAS[] = /* Attribute Substitute */
    { Name, Uid, Gid, Mode, 0 };

static int ctmDM[] = /* Directory Make */
    { Name|Dir|New , Uid, Gid, Mode, 0 };

static int ctmDR[] = /* Directory Remove */
    { Name|Dir, 0 };

struct CTM_Syntax Syntax[] = {
    { "FM", ctmFM },
    { "FS", ctmFS },
    { "FE", ctmFE },
    { "FR", ctmFR },
    { "AS", ctmAS },
    { "DM", ctmDM },
    { "DR", ctmDR },
    { 0, 0} };
