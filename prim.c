#include "a.h"


typedef enum PRIM_NodeType
{
    PRIM_NodeType_Str,
    PRIM_NodeType_Exp,
} PRIM_NodeType;


typedef struct PRIM_Exp
{
    u32 len;
    PRIM_Node ptr;
} PRIM_Exp;

typedef struct PRIM_NodeImpl
{
    PRIM_NodeType type;
    union
    {
        PRIM_Exp exp;
    };
} PRIM_NodeImpl;


typedef vec_t(PRIM_NodeImpl) PRIM_NodeImplVec;
typedef vec_t(PRIM_Node) PRIM_NodeVec;


typedef struct PRIM_Space
{
    PRIM_NodeImplVec nodePool;
} PRIM_Space;


PRIM_Space PRIM_newSpace(void)
{
    PRIM_Space space = { 0 };
    return space;
}

void PRIM_spaceFree(PRIM_Space* space)
{
    vec_free(&space->nodePool);
}







static PRIM_Node PRIM_newNode(PRIM_Space* space)
{
    PRIM_NodeImpl nodeImpl = { 0 };
    vec_push(&space->nodePool, nodeImpl);
    PRIM_Node node = { space->nodePool.length - 1 };
    return node;
}











PRIM_Node PRIM_newExpNode(PRIM_Space* space)
{
    PRIM_Node node = PRIM_newNode(space);
    PRIM_NodeImpl* nodeImpl = space->nodePool.data + node.id;
    nodeImpl->type = PRIM_NodeType_Exp;
    nodeImpl->exp.len = 0;
    return node;
}






























































