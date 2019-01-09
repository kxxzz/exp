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
    PRIM_NodeType_Atom,
    PRIM_NodeType_Expr,

    PRIM_NumNodeTypes
} PRIM_NodeType;

static const char* PRIM_NodeTypeNameTable[PRIM_NumNodeTypes] =
{
    "Atom",
    "Expr",
};

typedef struct PRIM_Node PRIM_Node;



void PRIM_nodeFree(PRIM_Node* node);
PRIM_Node* PRIM_nodeDup(const PRIM_Node* node);



PRIM_NodeType PRIM_type(const PRIM_Node* node);

static bool PRIM_isAtom(const PRIM_Node* node)
{
    return PRIM_NodeType_Atom == PRIM_type(node);
}
static bool PRIM_isExp(const PRIM_Node* node)
{
    return PRIM_NodeType_Expr == PRIM_type(node);
}



typedef struct PRIM_NodeSrcInfo
{
    bool hasSrcInfo;
    u32 offset;
    u32 line;
    u32 column;
    bool isStrTok;
} PRIM_NodeSrcInfo;

PRIM_NodeSrcInfo PRIM_srcInfo(const PRIM_Node* node);


PRIM_Node* PRIM_atom(const char* str);
u32 PRIM_atomSize(const PRIM_Node* node);
const char* PRIM_atomCstr(const PRIM_Node* node);


PRIM_Node* PRIM_expr(void);
u32 PRIM_exprLen(const PRIM_Node* node);
PRIM_Node** PRIM_exprElm(const PRIM_Node* node);


void PRIM_exprPush(PRIM_Node* node, PRIM_Node* c);
void PRIM_exprConcat(PRIM_Node* node, PRIM_Node* a);




PRIM_Node* PRIM_loadCell(const char* str);
PRIM_Node* PRIM_loadList(const char* str);





u32 PRIM_saveSL(const PRIM_Node* node, char* buf, u32 bufSize, bool withSrcInfo);

typedef struct PRIM_SaveMLopt
{
    u32 indent;
    u32 width;
    bool withSrcInfo;
} PRIM_SaveMLopt;

u32 PRIM_saveML(const PRIM_Node* node, char* buf, u32 bufSize, const PRIM_SaveMLopt* opt);
































