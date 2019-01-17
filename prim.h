#pragma once



#include <stdbool.h>
#include <stdint.h>
#include <vec.h>


typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;

typedef float f32;
typedef double f64;



typedef enum PRIM_NodeType
{
    PRIM_NodeType_Str,
    PRIM_NodeType_Exp,

    PRIM_NumNodeTypes
} PRIM_NodeType;

static const char* PRIM_NodeTypeNameTable[PRIM_NumNodeTypes] =
{
    "Str",
    "Exp",
};


typedef struct PRIM_Space PRIM_Space;

PRIM_Space* PRIM_newSpace(void);
void PRIM_spaceFree(PRIM_Space* space);


typedef struct PRIM_Node { u32 id; } PRIM_Node;

static u32 PRIM_InvalidNodeId = (u32)-1;


PRIM_NodeType PRIM_nodeType(PRIM_Space* space, PRIM_Node node);

static bool PRIM_isStr(PRIM_Space* space, PRIM_Node node)
{
    return PRIM_NodeType_Str == PRIM_nodeType(space, node);
}
static bool PRIM_isExp(PRIM_Space* space, PRIM_Node node)
{
    return PRIM_NodeType_Exp == PRIM_nodeType(space, node);
}



PRIM_Node PRIM_addStr(PRIM_Space* space, const char* str);
PRIM_Node PRIM_addLenStr(PRIM_Space* space, u32 len, const char* str);

void PRIM_addExpEnter(PRIM_Space* space);
void PRIM_addExpPush(PRIM_Space* space, PRIM_Node c);
void PRIM_addExpCancel(PRIM_Space* space);
PRIM_Node PRIM_addExpDone(PRIM_Space* space);

void PRIM_undoAdd1(PRIM_Space* space);
void PRIM_undoAdd(PRIM_Space* space, u32 n);


u32 PRIM_strSize(PRIM_Space* space, PRIM_Node node);
const char* PRIM_strCstr(PRIM_Space* space, PRIM_Node node);

u32 PRIM_expLen(PRIM_Space* space, PRIM_Node node);
PRIM_Node* PRIM_expElm(PRIM_Space* space, PRIM_Node node);



typedef struct PRIM_NodeSrcInfo
{
    bool exist;
    u32 offset;
    u32 line;
    u32 column;
    bool isStrTok;
} PRIM_NodeSrcInfo;

typedef vec_t(PRIM_NodeSrcInfo) PRIM_NodeSrcInfoTable;


PRIM_Node PRIM_loadSrcAsCell(PRIM_Space* space, const char* src, PRIM_NodeSrcInfoTable* srcInfoTable);
PRIM_Node PRIM_loadSrcAsList(PRIM_Space* space, const char* src, PRIM_NodeSrcInfoTable* srcInfoTable);





u32 PRIM_saveSL
(
    const PRIM_Space* space, PRIM_Node node, char* buf, u32 bufSize,
    const PRIM_NodeSrcInfoTable* srcInfoTable
);

typedef struct PRIM_SaveMLopt
{
    u32 indent;
    u32 width;
    PRIM_NodeSrcInfoTable* srcInfoTable;
} PRIM_SaveMLopt;

u32 PRIM_saveML(const PRIM_Space* space, PRIM_Node node, char* buf, u32 bufSize, const PRIM_SaveMLopt* opt);










int PRIM_exec(PRIM_Space* space, PRIM_Node root);

int PRIM_execFile(const char* srcFile, PRIM_NodeSrcInfoTable* srcInfoTable);




































































