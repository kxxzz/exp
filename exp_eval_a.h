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



















































































