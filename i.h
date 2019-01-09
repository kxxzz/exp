#pragma once



#include <stdbool.h>
#include <stdint.h>


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
    PRIM_NodeType_Vec,

    PRIM_NumNodeTypes
} PRIM_NodeType;

static const char* PRIM_NodeTypeNameTable[PRIM_NumNodeTypes] =
{
    "Str",
    "Vec",
};

typedef struct PRIM_Node PRIM_Node;



PRIM_NodeType PRIM_nodeType(const PRIM_Node* node);

static bool PRIM_nodeIsStr(const PRIM_Node* node)
{
    return PRIM_NodeType_Str == PRIM_nodeType(node);
}
static bool PRIM_nodeIsVec(const PRIM_Node* node)
{
    return PRIM_NodeType_Vec == PRIM_nodeType(node);
}


typedef struct PRIM_NodeSrcInfo
{
    u32 offset;
    u32 line;
    u32 column;
    bool isStrTok;
} PRIM_NodeSrcInfo;

const PRIM_NodeSrcInfo* PRIM_nodeSrcInfo(const PRIM_Node* node);
void PRIM_nodeFree(PRIM_Node* node);
PRIM_Node* PRIM_nodeDup(const PRIM_Node* node);


PRIM_Node* PRIM_str(const char* str);
u32 PRIM_strLen(const PRIM_Node* node);
const char* PRIM_strCstr(const PRIM_Node* node);


PRIM_Node* PRIM_vec(void);
u32 PRIM_vecLen(const PRIM_Node* node);
PRIM_Node** PRIM_vecElm(const PRIM_Node* node);


void PRIM_vecPush(PRIM_Node* node, PRIM_Node* c);
void PRIM_vecConcat(PRIM_Node* node, PRIM_Node* a);




PRIM_Node* PRIM_loadCell(const char* str);
PRIM_Node* PRIM_loadSeq(const char* str);





u32 PRIM_saveSL(const PRIM_Node* node, char* buf, u32 bufSize, bool withSrcInfo);

typedef struct PRIM_SaveMLopt
{
    u32 indent;
    u32 width;
    bool withSrcInfo;
} PRIM_SaveMLopt;

u32 PRIM_saveML(const PRIM_Node* node, char* buf, u32 bufSize, const PRIM_SaveMLopt* opt);
































