#ifndef TUI_DATA_H
#define TUI_DATA_H

/* Constant definitions */
#define DEFAULT_TAB_LEN                8
#define NO_SRC_STRING                  "[ No Source Available ]"
#define NO_DISASSEM_STRING             "[ No Assembly Available ]"
#define NO_REGS_STRING                 "[ Register Values Unavailable ]"
#define NO_DATA_STRING                 "[ No Data Values Displayed ]"
#define MAX_CONTENT_COUNT              100
#define SRC_NAME                       "SRC"
#define CMD_NAME                       "CMD"
#define DATA_NAME                      "REGS"
#define DISASSEM_NAME                  "ASM"
#define TUI_NULL_STR                   ""
#define DEFAULT_HISTORY_COUNT          25
#define BOX_WINDOW                     TRUE
#define DONT_BOX_WINDOW                FALSE
#define HILITE                         TRUE
#define NO_HILITE                      FALSE
#define WITH_LOCATOR                   TRUE
#define NO_LOCATOR                     FALSE
#define EMPTY_SOURCE_PROMPT            TRUE
#define NO_EMPTY_SOURCE_PROMPT         FALSE
#define UNDEFINED_ITEM                 -1
#define MIN_WIN_HEIGHT                 3
#define MIN_CMD_WIN_HEIGHT             3

#define FILE_PREFIX                    "File: "
#define PROC_PREFIX                    "Procedure: "
#define LINE_PREFIX                    "Line: "
#define PC_PREFIX                      "pc: "

#define TUI_FLOAT_REGS_NAME                  "$FREGS"
#define TUI_FLOAT_REGS_NAME_LOWER            "$fregs"
#define TUI_GENERAL_REGS_NAME                "$GREGS"
#define TUI_GENERAL_REGS_NAME_LOWER          "$gregs"
#define TUI_SPECIAL_REGS_NAME                "$SREGS"
#define TUI_SPECIAL_REGS_NAME_LOWER          "$sregs"
#define TUI_GENERAL_SPECIAL_REGS_NAME        "$REGS"
#define TUI_GENERAL_SPECIAL_REGS_NAME_LOWER  "$regs"

/* Scroll direction enum */
typedef enum {
    FORWARD_SCROLL,
    BACKWARD_SCROLL,
    LEFT_SCROLL,
    RIGHT_SCROLL
} TuiScrollDirection, *TuiScrollDirectionPtr;


/* General list struct */
typedef struct    _TuiList {
    OpaqueList    list;
    int            count;
} TuiList, *TuiListPtr;


/* The kinds of layouts available */
typedef enum {
    SRC_COMMAND,
    DISASSEM_COMMAND,
    SRC_DISASSEM_COMMAND,
    SRC_DATA_COMMAND,
    DISASSEM_DATA_COMMAND,
    UNDEFINED_LAYOUT
} TuiLayoutType, *TuiLayoutTypePtr;

/* Basic data types that can be displayed in the data window. */
typedef enum _TuiDataType {
    TUI_REGISTER,
    TUI_SCALAR,
    TUI_COMPLEX,
    TUI_STRUCT
} TuiDataType, TuiDataTypePtr;

/* Types of register displays */
typedef enum _TuiRegisterDisplayType {
    TUI_UNDEFINED_REGS,
    TUI_GENERAL_REGS,
    TUI_SFLOAT_REGS,
    TUI_DFLOAT_REGS,
    TUI_SPECIAL_REGS,
    TUI_GENERAL_AND_SPECIAL_REGS
} TuiRegisterDisplayType, *TuiRegisterDisplayTypePtr;

/* Structure describing source line or line address */
typedef union _TuiLineOrAddress {
    int           lineNo;
    Opaque        addr;
} TuiLineOrAddress, *TuiLineOrAddressPtr;

/* Current Layout definition */
typedef struct _TuiLayoutDef {
    TuiWinType                 displayMode;
    int                        split;
    TuiRegisterDisplayType     regsDisplayType;
    TuiRegisterDisplayType     floatRegsDisplayType;
} TuiLayoutDef, *TuiLayoutDefPtr;

/* Elements in the Source/Disassembly Window */
typedef struct _TuiSourceElement
{
    char                *line;
    TuiLineOrAddress    lineOrAddr;
    int                 isExecPoint;
    int                 hasBreak;
} TuiSourceElement, *TuiSourceElementPtr;


/* Elements in the data display window content */
typedef struct _TuiDataElement
{
    char        *name;
    int         itemNo;    /* the register number, or data display number */
    TuiDataType type;
    Opaque      value;
    int         highlight;
} TuiDataElement, *TuiDataElementPtr;


/* Elements in the command window content */
typedef struct _TuiCommandElement
{
    char    *line;
} TuiCommandElement, *TuiCommandElementPtr;


#define MAX_LOCATOR_ELEMENT_LEN        100

/* Elements in the locator window content */
typedef struct _TuiLocatorElement
{
    char        fileName[MAX_LOCATOR_ELEMENT_LEN];
    char        procName[MAX_LOCATOR_ELEMENT_LEN];
    int         lineNo;
    Opaque      addr;
} TuiLocatorElement, *TuiLocatorElementPtr;


/* An content element in a window */
typedef union
{
    TuiSourceElement       source; /* the source elements */
    TuiGenWinInfo          dataWindow; /* data display elements */
    TuiDataElement         data; /* elements of dataWindow */
    TuiCommandElement      command; /* command elements */
    TuiLocatorElement      locator; /* locator elements */
    char                   *simpleString; /* simple char based elements */
} TuiWhichElement, *TuiWhichElementPtr;

typedef struct _TuiWinElement
{
    int            highlight;
    TuiWhichElement whichElement;
} TuiWinElement, *TuiWinElementPtr;


/* This describes the content of the window. */
typedef        TuiWinElementPtr    *TuiWinContent;


/* This struct defines the specific information about a data display window */
typedef struct _TuiDataInfo {
    TuiWinContent            dataContent; /* start of data display content */
    int                      dataContentCount;
    TuiWinContent            regsContent; /* start of regs display content */
    int                      regsContentCount;
    TuiRegisterDisplayType   regsDisplayType;
    int                      regsColumnCount;
    int                      displayRegs; /* Should regs be displayed at all? */
} TuiDataInfo, *TuiDataInfoPtr;


typedef struct _TuiSourceInfo {
    int                 hasLocator; /* Does locator belongs to this window? */
    TuiGenWinInfoPtr    executionInfo; /* execution information window */
    int                 horizontalOffset; /* used for horizontal scroll */
    TuiLineOrAddress    startLineOrAddr;
} TuiSourceInfo, *TuiSourceInfoPtr;


typedef struct _TuiCommandInfo {
    int            curLine;   /* The current line position */
    int            curch;     /* The current cursor position */
} TuiCommandInfo, *TuiCommandInfoPtr;


/* This defines information about each logical window */
typedef struct _TuiWinInfo {
    TuiGenWinInfo            generic;        /* general window information */
    union {
        TuiSourceInfo        sourceInfo;
        TuiDataInfo          dataDisplayInfo;
        TuiCommandInfo       commandInfo;
        Opaque               opaque;
    } detail;
    int                 canHighlight; /* Can this window ever be highlighted? */
    int                 isHighlighted; /* Is this window highlighted? */
} TuiWinInfo, *TuiWinInfoPtr;

/* MACROS (prefixed with m_) */

/* Testing macros */
#define        m_genWinPtrIsNull(winInfo) \
                ((winInfo) == (TuiGenWinInfoPtr)NULL)
#define        m_genWinPtrNotNull(winInfo) \
                ((winInfo) != (TuiGenWinInfoPtr)NULL)
#define        m_winPtrIsNull(winInfo) \
                ((winInfo) == (TuiWinInfoPtr)NULL)
#define        m_winPtrNotNull(winInfo) \
                ((winInfo) != (TuiWinInfoPtr)NULL)

#define        m_winIsSourceType(type) \
                (type == SRC_WIN || type == DISASSEM_WIN)
#define        m_winIsAuxillary(winType) \
                (winType > MAX_MAJOR_WINDOWS)
#define        m_hasLocator(winInfo) \
                ( ((winInfo) != (TuiWinInfoPtr)NULL) ? \
                    (winInfo->detail.sourceInfo.hasLocator) : \
                    FALSE )

#define     m_setWinHighlightOn(winInfo) \
                if ((winInfo) != (TuiWinInfoPtr)NULL) \
                              (winInfo)->isHighlighted = TRUE
#define     m_setWinHighlightOff(winInfo) \
                if ((winInfo) != (TuiWinInfoPtr)NULL) \
                              (winInfo)->isHighlighted = FALSE


/* Global Data */
extern TuiWinInfoPtr      winList[MAX_MAJOR_WINDOWS];
extern int                tui_version;

/* Macros */
#define srcWin            winList[SRC_WIN]
#define disassemWin       winList[DISASSEM_WIN]
#define dataWin           winList[DATA_WIN]
#define cmdWin            winList[CMD_WIN]

/* Data Manipulation Functions */
extern void               initializeStaticData PARAMS ((void));
extern TuiGenWinInfoPtr   allocGenericWinInfo PARAMS ((void));
extern TuiWinInfoPtr      allocWinInfo PARAMS ((TuiWinType));
extern void               initGenericPart PARAMS ((TuiGenWinInfoPtr));
extern void               initWinInfo PARAMS ((TuiWinInfoPtr));
extern TuiWinContent      allocContent PARAMS ((int, TuiWinType));
extern int                addContentElements 
                             PARAMS ((TuiGenWinInfoPtr, int));
extern void               initContentElement 
                             PARAMS ((TuiWinElementPtr, TuiWinType));
extern void               freeWindow PARAMS ((TuiWinInfoPtr));
extern void               freeAllWindows PARAMS ((void));
extern void               freeWinContent PARAMS ((TuiGenWinInfoPtr));
extern void               freeDataContent PARAMS ((TuiWinContent, int));
extern void               freeAllSourceWinsContent PARAMS ((void));
extern void               tuiDelWindow PARAMS ((TuiWinInfoPtr));
extern void               tuiDelDataWindows PARAMS ((TuiWinContent, int));
extern TuiWinInfoPtr      winByName PARAMS ((char *));
extern TuiWinInfoPtr      partialWinByName PARAMS ((char *));
extern char               *winName PARAMS ((TuiGenWinInfoPtr));
extern char               *displayableWinContentOf 
                             PARAMS ((TuiGenWinInfoPtr, TuiWinElementPtr));
extern char               *displayableWinContentAt 
                             PARAMS ((TuiGenWinInfoPtr, int));
extern int                winElementHeight 
                             PARAMS ((TuiGenWinInfoPtr, TuiWinElementPtr));
extern TuiLayoutType      currentLayout PARAMS ((void));
extern void               setCurrentLayoutTo PARAMS ((TuiLayoutType));
extern int                termHeight PARAMS ((void));
extern void               setTermHeight PARAMS ((int));
extern int                termWidth PARAMS ((void));
extern void               setTermWidth PARAMS ((int));
extern int                historyLimit PARAMS ((void));
extern void               setHistoryLimit PARAMS ((int));
extern void               setGenWinOrigin PARAMS ((TuiGenWinInfoPtr, int, int));
extern TuiGenWinInfoPtr   locatorWinInfoPtr PARAMS ((void));
extern TuiGenWinInfoPtr   sourceExecInfoWinPtr PARAMS ((void));
extern TuiGenWinInfoPtr   disassemExecInfoWinPtr PARAMS ((void));
extern char               *nullStr PARAMS ((void));
extern char               *blankStr PARAMS ((void));
extern char               *locationStr PARAMS ((void));
extern char               *breakStr PARAMS ((void));
extern char               *breakLocationStr PARAMS ((void));
extern TuiListPtr         sourceWindows PARAMS ((void));
extern void               clearSourceWindows PARAMS ((void));
extern void               clearSourceWindowsDetail PARAMS ((void));
extern void               clearWinDetail PARAMS ((TuiWinInfoPtr   winInfo));
extern void               tuiAddToSourceWindows PARAMS ((TuiWinInfoPtr));
extern int                tuiDefaultTabLen PARAMS ((void));
extern void               tuiSetDefaultTabLen PARAMS ((int));
extern TuiWinInfoPtr      tuiWinWithFocus PARAMS ((void));
extern void               tuiSetWinWithFocus PARAMS ((TuiWinInfoPtr));
extern TuiLayoutDefPtr    tuiLayoutDef PARAMS ((void));
extern int                tuiWinResized PARAMS ((void));
extern void               tuiSetWinResizedTo PARAMS ((int));

extern TuiWinInfoPtr      tuiNextWin PARAMS ((TuiWinInfoPtr));
extern TuiWinInfoPtr      tuiPrevWin PARAMS ((TuiWinInfoPtr));


#endif /* TUI_DATA_H */
