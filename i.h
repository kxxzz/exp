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



typedef enum saga_NodeType
{
    saga_NodeType_Str,
    saga_NodeType_Vec,

    saga_NumNodeTypes
} saga_NodeType;

static const char* saga_NodeTypeNameTable[saga_NumNodeTypes] =
{
    "Str",
    "Vec",
};

typedef struct saga_Node saga_Node;



saga_NodeType saga_nodeType(const saga_Node* node);

static bool saga_nodeIsStr(const saga_Node* node)
{
    return saga_NodeType_Str == saga_nodeType(node);
}
static bool saga_nodeIsVec(const saga_Node* node)
{
    return saga_NodeType_Vec == saga_nodeType(node);
}


typedef struct saga_NodeSrcInfo
{
    u32 offset;
    u32 line;
    u32 column;
    bool isStrTok;
} saga_NodeSrcInfo;

const saga_NodeSrcInfo* saga_nodeSrcInfo(const saga_Node* node);
void saga_nodeFree(saga_Node* node);
saga_Node* saga_nodeDup(const saga_Node* node);


saga_Node* saga_str(const char* str);
u32 saga_strLen(const saga_Node* node);
const char* saga_strCstr(const saga_Node* node);


saga_Node* saga_vec(void);
u32 saga_vecLen(const saga_Node* node);
saga_Node** saga_vecElm(const saga_Node* node);


void saga_vecPush(saga_Node* node, saga_Node* c);
void saga_vecConcat(saga_Node* node, saga_Node* a);




saga_Node* saga_loadCell(const char* str);
saga_Node* saga_loadSeq(const char* str);





u32 saga_saveSL(const saga_Node* node, char* buf, u32 bufSize, bool withSrcInfo);

typedef struct saga_SaveMLopt
{
    u32 indent;
    u32 width;
    bool withSrcInfo;
} saga_SaveMLopt;

u32 saga_saveML(const saga_Node* node, char* buf, u32 bufSize, const saga_SaveMLopt* opt);
































