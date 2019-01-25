#include "exp_eval_a.h"



//typedef struct EXP_EvalValueTypeDef
//{
//    int x;
//} EXP_EvalValueTypeDef;



typedef struct EXP_EvalFunInfo
{
    bool hasInited;
    u32 numIn;
    u32 numOut;
    u32* insType;
    u32* outsType;
} EXP_EvalFunInfo;

typedef vec_t(EXP_EvalFunInfo) EXP_EvalFunInfoTable;




typedef struct EXP_EvalVerifContext
{
    EXP_Space* space;
    EXP_EvalValueTypeInfoTable* valueTypeTable;
    EXP_EvalNativeFunInfoTable* nativeFunTable;
    EXP_NodeSrcInfoTable* srcInfoTable;
    EXP_EvalFunInfoTable funTable;
    vec_u32 dataStack;
    EXP_EvalError error;
} EXP_EvalVerifContext;





static EXP_EvalVerifContext EXP_newEvalVerifContext
(
    EXP_Space* space, EXP_EvalValueTypeInfoTable* valueTypeTable, EXP_EvalNativeFunInfoTable* nativeFunTable,
    EXP_NodeSrcInfoTable* srcInfoTable
)
{
    EXP_EvalVerifContext _ctx = { 0 };
    EXP_EvalVerifContext* ctx = &_ctx;
    ctx->space = space;
    ctx->valueTypeTable = valueTypeTable;
    ctx->nativeFunTable = nativeFunTable;
    ctx->srcInfoTable = srcInfoTable;
    return *ctx;
}
static void EXP_evalVerifContextFree(EXP_EvalVerifContext* ctx)
{
    vec_free(&ctx->dataStack);
    vec_free(&ctx->funTable);
}
















static void EXP_evalVerif(EXP_EvalVerifContext* ctx)
{




}

















































































































































