/*
 *  Copyright (c) 2001 LSI Logic Corporation.
 *
 *
 *           Name:  MPI_TOOL.H
 *          Title:  MPI Toolbox structures and definitions
 *  Creation Date:  July 30, 2001
 *
 *    MPI Version:  01.02.02
 *
 *  Version History
 *  ---------------
 *
 *  Date      Version   Description
 *  --------  --------  ------------------------------------------------------
 *  08-08-01  01.02.01  Original release.
 *  08-29-01  01.02.02  Added DIAG_DATA_UPLOAD_HEADER and related defines.
 *  --------------------------------------------------------------------------
 */

#ifndef MPI_TOOL_H
#define MPI_TOOL_H

#define MPI_TOOLBOX_CLEAN_TOOL                      (0x00)
#define MPI_TOOLBOX_MEMORY_MOVE_TOOL                (0x01)
#define MPI_TOOLBOX_DIAG_DATA_UPLOAD_TOOL           (0x02)


/****************************************************************************/
/* Toolbox reply                                                            */
/****************************************************************************/

typedef struct _MSG_TOOLBOX_REPLY
{
    U8                      Tool;                       /* 00h */
    U8                      Reserved;                   /* 01h */
    U8                      MsgLength;                  /* 02h */
    U8                      Function;                   /* 03h */
    U16                     Reserved1;                  /* 04h */
    U8                      Reserved2;                  /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U16                     Reserved3;                  /* 0Ch */
    U16                     IOCStatus;                  /* 0Eh */
    U32                     IOCLogInfo;                 /* 10h */
} MSG_TOOLBOX_REPLY, MPI_POINTER PTR_MSG_TOOLBOX_REPLY,
  ToolboxReply_t, MPI_POINTER pToolboxReply_t;


/****************************************************************************/
/* Toolbox Clean Tool request                                               */
/****************************************************************************/

typedef struct _MSG_TOOLBOX_CLEAN_REQUEST
{
    U8                      Tool;                       /* 00h */
    U8                      Reserved;                   /* 01h */
    U8                      ChainOffset;                /* 02h */
    U8                      Function;                   /* 03h */
    U16                     Reserved1;                  /* 04h */
    U8                      Reserved2;                  /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U32                     Flags;                      /* 0Ch */
} MSG_TOOLBOX_CLEAN_REQUEST, MPI_POINTER PTR_MSG_TOOLBOX_CLEAN_REQUEST,
  ToolboxCleanRequest_t, MPI_POINTER pToolboxCleanRequest_t;

#define MPI_TOOLBOX_CLEAN_NVSRAM                    (0x00000001)
#define MPI_TOOLBOX_CLEAN_SEEPROM                   (0x00000002)
#define MPI_TOOLBOX_CLEAN_FLASH                     (0x00000004)


/****************************************************************************/
/* Toolbox Memory Move request                                              */
/****************************************************************************/

typedef struct _MSG_TOOLBOX_MEM_MOVE_REQUEST
{
    U8                      Tool;                       /* 00h */
    U8                      Reserved;                   /* 01h */
    U8                      ChainOffset;                /* 02h */
    U8                      Function;                   /* 03h */
    U16                     Reserved1;                  /* 04h */
    U8                      Reserved2;                  /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    SGE_SIMPLE_UNION        SGL;                        /* 0Ch */
} MSG_TOOLBOX_MEM_MOVE_REQUEST, MPI_POINTER PTR_MSG_TOOLBOX_MEM_MOVE_REQUEST,
  ToolboxMemMoveRequest_t, MPI_POINTER pToolboxMemMoveRequest_t;


/****************************************************************************/
/* Toolbox Diagnostic Data Upload request                                   */
/****************************************************************************/

typedef struct _MSG_TOOLBOX_DIAG_DATA_UPLOAD_REQUEST
{
    U8                      Tool;                       /* 00h */
    U8                      Reserved;                   /* 01h */
    U8                      ChainOffset;                /* 02h */
    U8                      Function;                   /* 03h */
    U16                     Reserved1;                  /* 04h */
    U8                      Reserved2;                  /* 06h */
    U8                      MsgFlags;                   /* 07h */
    U32                     MsgContext;                 /* 08h */
    U32                     Flags;                      /* 0Ch */
    U32                     Reserved3;                  /* 10h */
    SGE_SIMPLE_UNION        SGL;                        /* 14h */
} MSG_TOOLBOX_DIAG_DATA_UPLOAD_REQUEST, MPI_POINTER PTR_MSG_TOOLBOX_DIAG_DATA_UPLOAD_REQUEST,
  ToolboxDiagDataUploadRequest_t, MPI_POINTER pToolboxDiagDataUploadRequest_t;

typedef struct _DIAG_DATA_UPLOAD_HEADER
{
    U32                     DiagDataLength;             /* 00h */
    U8                      FormatCode;                 /* 04h */
    U8                      Reserved;                   /* 05h */
    U16                     Reserved1;                  /* 06h */
} DIAG_DATA_UPLOAD_HEADER, MPI_POINTER PTR_DIAG_DATA_UPLOAD_HEADER,
  DiagDataUploadHeader_t, MPI_POINTER pDiagDataUploadHeader_t;

#define MPI_TB_DIAG_FORMAT_SCSI_PRINTF_1            (0x01)
#define MPI_TB_DIAG_FORMAT_SCSI_2                   (0x02)
#define MPI_TB_DIAG_FORMAT_SCSI_3                   (0x03)
#define MPI_TB_DIAG_FORMAT_FC_TRACE_1               (0x04)


#endif


