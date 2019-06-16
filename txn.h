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





typedef enum TXN_NodeType
{
    TXN_NodeType_Tok,
    TXN_NodeType_SeqNaked,
    TXN_NodeType_SeqRound,
    TXN_NodeType_SeqSquare,
    TXN_NodeType_SeqCurly,

    TXN_NumNodeTypes
} TXN_NodeType;

static const char** TXN_NodeTypeNameTable(void)
{
    static const char* a[TXN_NumNodeTypes] =
    {
        "Tok",
        "SeqNaked",
        "SeqRound",
        "SeqSquare",
        "SeqCurly",
    };
    return a;
}


typedef struct TXN_Space TXN_Space;

TXN_Space* TXN_spaceNew(void);
void TXN_spaceFree(TXN_Space* space);


u32 TXN_spaceNodesTotal(const TXN_Space* space);


typedef struct TXN_Node { u32 id; } TXN_Node;
typedef vec_t(TXN_Node) TXN_NodeVec;

static TXN_Node TXN_Node_Invalid = { (u32)-1 };


TXN_NodeType TXN_nodeType(const TXN_Space* space, TXN_Node node);

static bool TXN_isTok(const TXN_Space* space, TXN_Node node)
{
    return TXN_NodeType_Tok == TXN_nodeType(space, node);
}
static bool TXN_isSeq(const TXN_Space* space, TXN_Node node)
{
    return TXN_nodeType(space, node) > TXN_NodeType_Tok;
}
static bool TXN_isSeqNaked(const TXN_Space* space, TXN_Node node)
{
    return TXN_NodeType_SeqNaked == TXN_nodeType(space, node);
}
static bool TXN_isSeqRound(const TXN_Space* space, TXN_Node node)
{
    return TXN_NodeType_SeqRound == TXN_nodeType(space, node);
}
static bool TXN_isSeqSquare(const TXN_Space* space, TXN_Node node)
{
    return TXN_NodeType_SeqSquare == TXN_nodeType(space, node);
}
static bool TXN_isSeqCurly(const TXN_Space* space, TXN_Node node)
{
    return TXN_NodeType_SeqCurly == TXN_nodeType(space, node);
}



TXN_Node TXN_tokFromCstr(TXN_Space* space, const char* str, bool quoted);
TXN_Node TXN_tokFromLenStr(TXN_Space* space, const char* str, u32 len, bool quoted);

TXN_Node TXN_seqNew(TXN_Space* space, TXN_NodeType type, const TXN_Node* elms, u32 len);


u32 TXN_tokSize(const TXN_Space* space, TXN_Node node);
const char* TXN_tokCstr(const TXN_Space* space, TXN_Node node);
bool TXN_tokQuoted(const TXN_Space* space, TXN_Node node);

u32 TXN_seqLen(const TXN_Space* space, TXN_Node node);
const TXN_Node* TXN_seqElm(const TXN_Space* space, TXN_Node node);


bool TXN_nodeDataEq(const TXN_Space* space, TXN_Node a, TXN_Node b);



typedef struct TXN_NodeSrcInfo
{
    u32 file;
    u32 offset;
    u32 line;
    u32 column;
    bool isQuotStr;
} TXN_NodeSrcInfo;

typedef vec_t(TXN_NodeSrcInfo) TXN_NodeSrcInfoVec;




typedef struct TXN_SpaceSrcInfo
{
    vec_u32 fileBases[1];
    TXN_NodeSrcInfoVec nodes[1];
} TXN_SpaceSrcInfo;

void TXN_spaceSrcInfoFree(TXN_SpaceSrcInfo* srcInfo);

const TXN_NodeSrcInfo* TXN_nodeSrcInfo(const TXN_SpaceSrcInfo* srcInfo, TXN_Node node);



TXN_Node TXN_parseAsCell(TXN_Space* space, const char* src, TXN_SpaceSrcInfo* srcInfo);
TXN_Node TXN_parseAsList(TXN_Space* space, const char* src, TXN_SpaceSrcInfo* srcInfo);





u32 TXN_printSL(const TXN_Space* space, TXN_Node node, char* buf, u32 bufSize, const TXN_SpaceSrcInfo* srcInfo);

typedef struct TXN_PrintMlOpt
{
    u32 indent;
    u32 width;
    TXN_SpaceSrcInfo* srcInfo;
} TXN_PrintMlOpt;

u32 TXN_printML(const TXN_Space* space, TXN_Node node, char* buf, u32 bufSize, const TXN_PrintMlOpt* opt);












































































