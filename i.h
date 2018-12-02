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
    saga_NodeType_Term,
    saga_NodeType_Inode,

    saga_NumNodeTypes
} saga_NodeType;

static const char* saga_NodeTypeNameTable[saga_NumNodeTypes] =
{
    "Term",
    "Inode",
};

typedef struct saga_Node saga_Node;




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


saga_Node* saga_term(const char* str);
u32 saga_termStrLen(const saga_Node* nodea);


saga_Node* saga_inode(void);
u32 saga_inodeNumChilden(const saga_Node* node);
saga_Node** saga_inodeChilden(const saga_Node* node);


void saga_inodeAddChild(saga_Node* node, saga_Node* c);
void saga_inodeConcat(saga_Node* node, saga_Node* a);




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































