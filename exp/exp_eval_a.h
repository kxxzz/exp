#pragma once


#include "exp_eval.h"
#include "exp_a.h"

#include <nstr.h>
#include <upool.h>




typedef vec_t(EXP_EvalAtypeInfo) EXP_EvalAtypeInfoVec;
typedef vec_t(EXP_EvalAfunInfo) EXP_EvalAfunInfoVec;






typedef enum EXP_EvalNodeType
{
    EXP_EvalNodeType_None = 0,

    EXP_EvalNodeType_VarDefBegin,
    EXP_EvalNodeType_VarDefEnd,
    EXP_EvalNodeType_Drop,
    EXP_EvalNodeType_Afun,

    EXP_EvalNodeType_Var,
    EXP_EvalNodeType_Fun,
    EXP_EvalNodeType_Value,
    EXP_EvalNodeType_String,

    EXP_EvalNodeType_CallVar,
    EXP_EvalNodeType_CallFun,
    EXP_EvalNodeType_CallAfun,

    EXP_EvalNodeType_Def,
    EXP_EvalNodeType_If,
    EXP_EvalNodeType_Block,
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
        EXP_Node funDef;
        EXP_EvalValue value;
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
    EXP_Node* elms = EXP_seqElm(space, node);
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




static bool EXP_evalIfHasBranch1(EXP_Space* space, EXP_Node node)
{
    EXP_Node* elms = EXP_seqElm(space, node);
    u32 len = EXP_seqLen(space, node);
    assert((3 == len) || (4 == len));
    return 4 == len;
}
static EXP_Node* EXP_evalIfBranch0(EXP_Space* space, EXP_Node node)
{
    EXP_Node* elms = EXP_seqElm(space, node);
    return elms + 2;
}
static EXP_Node* EXP_evalIfBranch1(EXP_Space* space, EXP_Node node)
{
    EXP_Node* elms = EXP_seqElm(space, node);
    return elms + 3;
}
















































































