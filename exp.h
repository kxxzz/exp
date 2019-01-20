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



typedef enum EXP_NodeType
{
    EXP_NodeType_Tok,
    EXP_NodeType_Seq,

    EXP_NumNodeTypes
} EXP_NodeType;

static const char* EXP_NodeTypeNameTable[EXP_NumNodeTypes] =
{
    "Tok",
    "Seq",
};


typedef struct EXP_Space EXP_Space;

EXP_Space* EXP_newSpace(void);
void EXP_spaceFree(EXP_Space* space);


typedef struct EXP_Node { u32 id; } EXP_Node;

static u32 EXP_NodeId_Invalid = (u32)-1;


EXP_NodeType EXP_nodeType(EXP_Space* space, EXP_Node node);

static bool EXP_isTok(EXP_Space* space, EXP_Node node)
{
    return EXP_NodeType_Tok == EXP_nodeType(space, node);
}
static bool EXP_isSeq(EXP_Space* space, EXP_Node node)
{
    return EXP_NodeType_Seq == EXP_nodeType(space, node);
}



EXP_Node EXP_addTok(EXP_Space* space, const char* str);
EXP_Node EXP_addTokL(EXP_Space* space, u32 len, const char* str);

void EXP_addSeqEnter(EXP_Space* space);
void EXP_addSeqPush(EXP_Space* space, EXP_Node c);
void EXP_addSeqCancel(EXP_Space* space);
EXP_Node EXP_addSeqDone(EXP_Space* space);

void EXP_undoAdd1(EXP_Space* space);
void EXP_undoAdd(EXP_Space* space, u32 n);


u32 EXP_tokSize(EXP_Space* space, EXP_Node node);
const char* EXP_tokCstr(EXP_Space* space, EXP_Node node);

u32 EXP_seqLen(EXP_Space* space, EXP_Node node);
EXP_Node* EXP_seqElm(EXP_Space* space, EXP_Node node);



typedef struct EXP_NodeSrcInfo
{
    bool exist;
    u32 offset;
    u32 line;
    u32 column;
    bool isQuotStr;
} EXP_NodeSrcInfo;

typedef vec_t(EXP_NodeSrcInfo) EXP_NodeSrcInfoTable;


EXP_Node EXP_loadSrcAsCell(EXP_Space* space, const char* src, EXP_NodeSrcInfoTable* srcInfoTable);
EXP_Node EXP_loadSrcAsList(EXP_Space* space, const char* src, EXP_NodeSrcInfoTable* srcInfoTable);





u32 EXP_saveSL
(
    const EXP_Space* space, EXP_Node node, char* buf, u32 bufSize,
    const EXP_NodeSrcInfoTable* srcInfoTable
);

typedef struct EXP_SaveMLopt
{
    u32 indent;
    u32 width;
    EXP_NodeSrcInfoTable* srcInfoTable;
} EXP_SaveMLopt;

u32 EXP_saveML(const EXP_Space* space, EXP_Node node, char* buf, u32 bufSize, const EXP_SaveMLopt* opt);












































































