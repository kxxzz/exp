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




#include "vec.h"



typedef enum saga_NodeType
{
    saga_NodeType_Term,
    saga_NodeType_Inode,

    saga_NumNodeTypes
} saga_NodeType;

static const char* saga_NodeTypeNameTable[saga_NumNodeTypes] =
{
    "Term",
    "Inode",
};



typedef vec_t(char) saga_Str;
typedef vec_t(struct saga_Node) saga_Vec;



typedef struct saga_NodeSrcInfo
{
    u32 offset;
    u32 line;
    u32 column;
    bool isStrTok;
} saga_NodeSrcInfo;

typedef struct saga_Node
{
    saga_NodeType type;
    union
    {
        saga_Str str;
        saga_Vec vec;
    };
    bool hasSrcInfo;
    saga_NodeSrcInfo srcInfo;
} saga_Node;


void saga_nodeFree(saga_Node* node);
void saga_nodeDup(saga_Node* a, const saga_Node* b);




saga_Node saga_nodeStr(const char* s);
u32 saga_strLen(saga_Node* a);



saga_Node* saga_load(const char* str);

u32 saga_saveSL(const saga_Node* node, char* buf, u32 bufSize, bool withSrcInfo);





typedef struct saga_SaveMLopt
{
    u32 indent;
    u32 width;
    bool withSrcInfo;
} saga_SaveMLopt;

u32 saga_saveML(const saga_Node* node, char* buf, u32 bufSize, const saga_SaveMLopt* opt);































