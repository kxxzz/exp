#pragma once


#include "exp_eval.h"
#include "exp_a.h"

#include <nstr.h>





typedef vec_t(EXP_EvalValueTypeInfo) EXP_EvalValueTypeInfoTable;
typedef vec_t(EXP_EvalNativeFunInfo) EXP_EvalNativeFunInfoTable;








typedef struct EXP_EvalDef
{
    EXP_Node key;
    bool isVal;
    union
    {
        EXP_Node fun;
        EXP_EvalValue val;
    };
} EXP_EvalDef;

typedef vec_t(EXP_EvalDef) EXP_EvalDefStack;



typedef vec_t(EXP_EvalDef) EXP_EvalDefTable;

typedef struct EXP_EvalBlock
{
    EXP_Node parent;
    u32 funsOffset;
    u32 funsCount;
    u32 varsCount;
} EXP_EvalBlock;

typedef vec_t(EXP_EvalBlock) EXP_EvalBlockTable;











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












































































