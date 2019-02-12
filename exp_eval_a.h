#pragma once


#include "exp_eval.h"
#include "exp_a.h"

#include <nstr.h>





typedef vec_t(EXP_EvalValueTypeInfo) EXP_EvalValueTypeInfoTable;
typedef vec_t(EXP_EvalNativeFunInfo) EXP_EvalNativeFunInfoTable;






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












































































