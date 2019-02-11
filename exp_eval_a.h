#pragma once


#include "exp_eval.h"
#include "exp_a.h"

#include <nstr.h>



typedef struct EXP_EvalModInfo
{
    EXP_Node url;
    EXP_Node root;
} EXP_EvalModInfo;

typedef vec_t(EXP_EvalModInfo) EXP_EvalModInfoTable;




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
    u32 defsCount;
    u32 defsOffset;
} EXP_EvalBlock;

typedef vec_t(EXP_EvalBlock) EXP_EvalBlockTable;




typedef enum EXP_EvalBlockCallbackType
{
    EXP_EvalBlockCallbackType_NONE,
    EXP_EvalBlockCallbackType_NativeCall,
    EXP_EvalBlockCallbackType_Call,
    EXP_EvalBlockCallbackType_Cond,
    EXP_EvalBlockCallbackType_Branch0,
    EXP_EvalBlockCallbackType_Branch1,
    EXP_EvalBlockCallbackType_BranchUnify,
} EXP_EvalBlockCallbackType;

typedef struct EXP_EvalBlockCallback
{
    EXP_EvalBlockCallbackType type;
    union
    {
        u32 nativeFun;
        EXP_Node fun;
        EXP_Node* branch[2];
    };
} EXP_EvalBlockCallback;

static EXP_EvalBlockCallback EXP_EvalBlockCallback_NONE = { EXP_EvalBlockCallbackType_NONE };






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



















































































