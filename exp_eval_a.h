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





typedef enum EXP_EvalBlockCallbackType
{
    EXP_EvalBlockCallbackType_NONE,
    EXP_EvalBlockCallbackType_NativeCall,
    EXP_EvalBlockCallbackType_Call,
    EXP_EvalBlockCallbackType_Cond,
    EXP_EvalBlockCallbackType_Branch,
    EXP_EvalBlockCallbackType_BranchCheck,
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




typedef struct EXP_EvalCall
{
    EXP_Node srcNode;
    u32 defStackP;
    // todo remove
    u32 dataStackP;
    EXP_Node* seq;
    u32 seqLen;
    u32 p;
    EXP_EvalBlockCallback cb;
} EXP_EvalCall;

typedef vec_t(EXP_EvalCall) EXP_EvalCallStack;






typedef vec_t(EXP_EvalValueTypeInfo) EXP_EvalValueTypeInfoTable;
typedef vec_t(EXP_EvalNativeFunInfo) EXP_EvalNativeFunInfoTable;


typedef struct EXP_EvalContext
{
    EXP_Space* space;
    EXP_EvalDataStack* dataStack;
    EXP_EvalValueTypeInfoTable valueTypeTable;
    EXP_EvalNativeFunInfoTable nativeFunTable;
    EXP_NodeSrcInfoTable* srcInfoTable;
    EXP_EvalDefStack defStack;
    EXP_EvalCallStack blockStack;
    EXP_EvalValueData nativeCallOutBuf[EXP_EvalNativeFunOuts_MAX];
    EXP_EvalError error;
} EXP_EvalContext;









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







EXP_EvalError EXP_evalVerif
(
    EXP_Space* space, EXP_Node root,
    EXP_EvalValueTypeInfoTable* valueTypeTable, EXP_EvalNativeFunInfoTable* nativeFunTable,
    EXP_NodeSrcInfoTable* srcInfoTable
);


















































































