#pragma once


#include "exp_eval.h"
#include "exp_a.h"

#include <nstr.h>




typedef vec_t(EXP_EvalAtypeInfo) EXP_EvalAtypeInfoVec;
typedef vec_t(EXP_EvalAfunInfo) EXP_EvalAfunInfoVec;






typedef enum EXP_EvalNodeType
{
    EXP_EvalNodeType_None = 0,

    EXP_EvalNodeType_VarDefBegin,
    EXP_EvalNodeType_VarDefEnd,
    EXP_EvalNodeType_GC,
    EXP_EvalNodeType_Afun,
    EXP_EvalNodeType_Apply,
    EXP_EvalNodeType_BlockExe,

    EXP_EvalNodeType_Var,
    EXP_EvalNodeType_Word,
    EXP_EvalNodeType_Atom,
    EXP_EvalNodeType_Block,

    EXP_EvalNodeType_CallAfun,
    EXP_EvalNodeType_CallWord,
    EXP_EvalNodeType_CallVar,

    EXP_EvalNodeType_Def,
    EXP_EvalNodeType_If,
} EXP_EvalNodeType;

typedef struct EXP_EvalNodeVar
{
    EXP_Node block;
    u32 id;
} EXP_EvalNodeVar;

typedef struct EXP_EvalNode
{
    EXP_EvalNodeType type;
    union
    {
        u32 afun;
        u32 atype;
        u32 numIns;
        EXP_Node blkSrc;
        EXP_EvalNodeVar var;
    };
    u32 varsCount;
} EXP_EvalNode;

typedef vec_t(EXP_EvalNode) EXP_EvalNodeTable;













static bool EXP_evalCheckCall(EXP_Space* space, EXP_Node node)
{
    if (!EXP_isSeqRound(space, node))
    {
        return false;
    }
    u32 len = EXP_seqLen(space, node);
    if (!len)
    {
        return false;
    }
    const EXP_Node* elms = EXP_seqElm(space, node);
    if (!EXP_isTok(space, elms[0]))
    {
        return false;
    }
    if (EXP_tokQuoted(space, elms[0]))
    {
        return false;
    }
    return true;
}





static const EXP_Node* EXP_evalIfBranch0(EXP_Space* space, EXP_Node node)
{
    const EXP_Node* elms = EXP_seqElm(space, node);
    return elms + 2;
}
static const EXP_Node* EXP_evalIfBranch1(EXP_Space* space, EXP_Node node)
{
    const EXP_Node* elms = EXP_seqElm(space, node);
    return elms + 3;
}








static void EXP_evalErrorFound
(
    EXP_EvalError* err, const EXP_SpaceSrcInfo* srcInfo, EXP_EvalErrCode errCode, EXP_Node node
)
{
    err->code = errCode;
    if (srcInfo)
    {
        assert(node.id - srcInfo->baseNodeId < srcInfo->nodes.length);
        const EXP_NodeSrcInfo* nodeSrcInfo = srcInfo->nodes.data + node.id - srcInfo->baseNodeId;
        err->file = nodeSrcInfo->file;
        err->line = nodeSrcInfo->line;
        err->column = nodeSrcInfo->column;
    }
}







































































