#pragma once


#include "exp_eval.h"
#include "exp_a.h"

#include <nstr.h>



typedef struct EXP_EvalFun
{
    EXP_Node src;
    u32 numIn;
    u32 numOut;
} EXP_EvalFun;


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
    EXP_EvalBlockCallbackType_NativeFun,
    EXP_EvalBlockCallbackType_Fun,
    EXP_EvalBlockCallbackType_Branch,
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





typedef struct EXP_EvalBlock
{
    EXP_Node srcNode;
    u32 defStackP;
    // todo remove
    u32 dataStackP;
    EXP_Node* seq;
    u32 seqLen;
    u32 p;
    EXP_EvalBlockCallback cb;
} EXP_EvalBlock;

typedef vec_t(EXP_EvalBlock) EXP_EvalBlockStack;






typedef vec_t(EXP_EvalValueTypeInfo) EXP_EvalValueTypeInfoTable;
typedef vec_t(EXP_EvalNativeFunInfo) EXP_EvalNativeFunInfoTable;


typedef struct EXP_EvalContext
{
    EXP_Space* space;
    EXP_EvalDataStack* dataStack;
    EXP_EvalValueTypeInfoTable valueTypeTable;
    EXP_EvalNativeFunInfoTable nativeFunTable;
    EXP_NodeSrcInfoTable* srcInfoTable;
    EXP_EvalRet ret;
    EXP_EvalDefStack defStack;
    EXP_EvalBlockStack blockStack;
    EXP_EvalValueData nativeCallOutBuf[EXP_EvalNativeFunOuts_MAX];
} EXP_EvalContext;



































































































