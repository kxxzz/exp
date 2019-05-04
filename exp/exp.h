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
    EXP_NodeType_SeqNaked,
    EXP_NodeType_SeqRound,
    EXP_NodeType_SeqSquare,
    EXP_NodeType_SeqCurly,

    EXP_NumNodeTypes
} EXP_NodeType;

static const char** EXP_NodeTypeNameTable(void)
{
    static const char* a[EXP_NumNodeTypes] =
    {
        "Tok",
        "SeqNaked",
        "SeqRound",
        "SeqSquare",
        "SeqCurly",
    };
    return a;
}


typedef struct EXP_Space EXP_Space;

EXP_Space* EXP_newSpace(void);
void EXP_spaceFree(EXP_Space* space);


u32 EXP_spaceNodesTotal(EXP_Space* space);


typedef struct EXP_Node { u32 id; } EXP_Node;
typedef vec_t(EXP_Node) EXP_NodeVec;

static u32 EXP_NodeId_Invalid = (u32)-1;
static EXP_Node EXP_Node_Invalid = { (u32)-1 };


EXP_NodeType EXP_nodeType(EXP_Space* space, EXP_Node node);

static bool EXP_isTok(EXP_Space* space, EXP_Node node)
{
    return EXP_NodeType_Tok == EXP_nodeType(space, node);
}
static bool EXP_isSeq(EXP_Space* space, EXP_Node node)
{
    return EXP_nodeType(space, node) > EXP_NodeType_Tok;
}
static bool EXP_isSeqNaked(EXP_Space* space, EXP_Node node)
{
    return EXP_NodeType_SeqNaked == EXP_nodeType(space, node);
}
static bool EXP_isSeqRound(EXP_Space* space, EXP_Node node)
{
    return EXP_NodeType_SeqRound == EXP_nodeType(space, node);
}
static bool EXP_isSeqSquare(EXP_Space* space, EXP_Node node)
{
    return EXP_NodeType_SeqSquare == EXP_nodeType(space, node);
}
static bool EXP_isSeqCurly(EXP_Space* space, EXP_Node node)
{
    return EXP_NodeType_SeqCurly == EXP_nodeType(space, node);
}



EXP_Node EXP_addTok(EXP_Space* space, const char* str, bool quoted);
EXP_Node EXP_addTokL(EXP_Space* space, const char* str, u32 len, bool quoted);

void EXP_addSeqEnter(EXP_Space* space, EXP_NodeType type);
void EXP_addSeqPush(EXP_Space* space, EXP_Node c);
void EXP_addSeqCancel(EXP_Space* space);
EXP_Node EXP_addSeqDone(EXP_Space* space);


u32 EXP_tokSize(EXP_Space* space, EXP_Node node);
const char* EXP_tokCstr(EXP_Space* space, EXP_Node node);
bool EXP_tokQuoted(EXP_Space* space, EXP_Node node);

u32 EXP_seqLen(EXP_Space* space, EXP_Node node);
const EXP_Node* EXP_seqElm(EXP_Space* space, EXP_Node node);


bool EXP_nodeDataEq(EXP_Space* space, EXP_Node a, EXP_Node b);



typedef struct EXP_NodeSrcInfo
{
    u32 file;
    u32 offset;
    u32 line;
    u32 column;
    bool isQuotStr;
} EXP_NodeSrcInfo;

typedef vec_t(EXP_NodeSrcInfo) EXP_NodeSrcInfoVec;




typedef struct EXP_SpaceSrcInfo
{
    u32 fileCount;
    EXP_NodeSrcInfoVec nodes;
} EXP_SpaceSrcInfo;

void EXP_spaceSrcInfoFree(EXP_SpaceSrcInfo* srcInfo);


EXP_Node EXP_loadSrcAsCell(EXP_Space* space, const char* src, EXP_SpaceSrcInfo* srcInfo);
EXP_Node EXP_loadSrcAsList(EXP_Space* space, const char* src, EXP_SpaceSrcInfo* srcInfo);





u32 EXP_saveSL(const EXP_Space* space, EXP_Node node, char* buf, u32 bufSize, const EXP_SpaceSrcInfo* srcInfo);

typedef struct EXP_SaveMLopt
{
    u32 indent;
    u32 width;
    EXP_SpaceSrcInfo* srcInfo;
} EXP_SaveMLopt;

u32 EXP_saveML(const EXP_Space* space, EXP_Node node, char* buf, u32 bufSize, const EXP_SaveMLopt* opt);












































































