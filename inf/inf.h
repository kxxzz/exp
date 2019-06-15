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





typedef enum INF_NodeType
{
    INF_NodeType_Tok,
    INF_NodeType_SeqNaked,
    INF_NodeType_SeqRound,
    INF_NodeType_SeqSquare,
    INF_NodeType_SeqCurly,

    INF_NumNodeTypes
} INF_NodeType;

static const char** INF_NodeTypeNameTable(void)
{
    static const char* a[INF_NumNodeTypes] =
    {
        "Tok",
        "SeqNaked",
        "SeqRound",
        "SeqSquare",
        "SeqCurly",
    };
    return a;
}


typedef struct INF_Space INF_Space;

INF_Space* INF_newSpace(void);
void INF_spaceFree(INF_Space* space);


u32 INF_spaceNodesTotal(const INF_Space* space);


typedef struct INF_Node { u32 id; } INF_Node;
typedef vec_t(INF_Node) INF_NodeVec;

static INF_Node INF_Node_Invalid = { (u32)-1 };


INF_NodeType INF_nodeType(const INF_Space* space, INF_Node node);

static bool INF_isTok(const INF_Space* space, INF_Node node)
{
    return INF_NodeType_Tok == INF_nodeType(space, node);
}
static bool INF_isSeq(const INF_Space* space, INF_Node node)
{
    return INF_nodeType(space, node) > INF_NodeType_Tok;
}
static bool INF_isSeqNaked(const INF_Space* space, INF_Node node)
{
    return INF_NodeType_SeqNaked == INF_nodeType(space, node);
}
static bool INF_isSeqRound(const INF_Space* space, INF_Node node)
{
    return INF_NodeType_SeqRound == INF_nodeType(space, node);
}
static bool INF_isSeqSquare(const INF_Space* space, INF_Node node)
{
    return INF_NodeType_SeqSquare == INF_nodeType(space, node);
}
static bool INF_isSeqCurly(const INF_Space* space, INF_Node node)
{
    return INF_NodeType_SeqCurly == INF_nodeType(space, node);
}



INF_Node INF_addTok(INF_Space* space, const char* str, bool quoted);
INF_Node INF_addTokL(INF_Space* space, const char* str, u32 len, bool quoted);

void INF_addSeqEnter(INF_Space* space, INF_NodeType type);
void INF_addSeqPush(INF_Space* space, INF_Node c);
void INF_addSeqCancel(INF_Space* space);
INF_Node INF_addSeqDone(INF_Space* space);


u32 INF_tokSize(const INF_Space* space, INF_Node node);
const char* INF_tokCstr(const INF_Space* space, INF_Node node);
bool INF_tokQuoted(const INF_Space* space, INF_Node node);

u32 INF_seqLen(const INF_Space* space, INF_Node node);
const INF_Node* INF_seqElm(const INF_Space* space, INF_Node node);


bool INF_nodeDataEq(const INF_Space* space, INF_Node a, INF_Node b);



typedef struct INF_NodeSrcInfo
{
    u32 file;
    u32 offset;
    u32 line;
    u32 column;
    bool isQuotStr;
} INF_NodeSrcInfo;

typedef vec_t(INF_NodeSrcInfo) INF_NodeSrcInfoVec;




typedef struct INF_SpaceSrcInfo
{
    vec_u32 baseNodeIds[1];
    INF_NodeSrcInfoVec nodes[1];
} INF_SpaceSrcInfo;

void INF_spaceSrcInfoFree(INF_SpaceSrcInfo* srcInfo);

const INF_NodeSrcInfo* INF_nodeSrcInfo(const INF_SpaceSrcInfo* srcInfo, INF_Node node);



INF_Node INF_parseAsCell(INF_Space* space, const char* src, INF_SpaceSrcInfo* srcInfo);
INF_Node INF_parseAsList(INF_Space* space, const char* src, INF_SpaceSrcInfo* srcInfo);





u32 INF_printSL(const INF_Space* space, INF_Node node, char* buf, u32 bufSize, const INF_SpaceSrcInfo* srcInfo);

typedef struct INF_PrintMlOpt
{
    u32 indent;
    u32 width;
    INF_SpaceSrcInfo* srcInfo;
} INF_PrintMlOpt;

u32 INF_printML(const INF_Space* space, INF_Node node, char* buf, u32 bufSize, const INF_PrintMlOpt* opt);












































































