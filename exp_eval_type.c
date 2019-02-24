#include "exp_eval_a.h"



typedef enum EXP_EvalTypeType
{
    EXP_EvalTypeType_Nval,
    EXP_EvalTypeType_Var,
    EXP_EvalTypeType_Fun,
    EXP_EvalTypeType_Tuple,
    EXP_EvalTypeType_Array,

    EXP_NumEvalTypeTypes
} EXP_EvalTypeType;

typedef struct EXP_EvalTypeList
{
    u32 count;
    u32 offset;
} EXP_EvalTypeList;

typedef struct EXP_EvalTypeFun
{
    EXP_EvalTypeList ins;
    EXP_EvalTypeList outs;
} EXP_EvalTypeFun;

typedef struct EXP_EvalTypeDesc
{
    bool hasVar;
    EXP_EvalTypeType type;
    union
    {
        EXP_EvalTypeFun fun;
        EXP_EvalTypeList tuple;
        u32 aryElm;
    };
} EXP_EvalTypeDesc;

typedef vec_t(EXP_EvalTypeDesc) EXP_EvalTypeDescVec;




typedef struct EXP_EvalTypeContext
{
    vec_u32 listBuf;
    EXP_EvalTypeDescVec typeDescs;
} EXP_EvalTypeContext;


EXP_EvalTypeContext* EXP_newEvalTypeContext(void)
{
    EXP_EvalTypeContext* ctx = zalloc(sizeof(EXP_EvalTypeContext));
    return ctx;
}

void EXP_evalTypeContextFree(EXP_EvalTypeContext* ctx)
{
    vec_free(&ctx->typeDescs);
    free(ctx);
}


static u32 EXP_evalTypeAdd(EXP_EvalTypeContext* ctx, const EXP_EvalTypeDesc* desc)
{
    u32 id = ctx->typeDescs.length;
    vec_push(&ctx->typeDescs, *desc);
    return id;
}

static u32 EXP_evalTypeAddList(EXP_EvalTypeContext* ctx, u32 count, const u32* elms)
{
}

u32 EXP_evalTypeAddNval(EXP_EvalTypeContext* ctx)
{
    EXP_EvalTypeDesc desc = { EXP_EvalTypeType_Nval };
    return EXP_evalTypeAdd(ctx, &desc);
}

u32 EXP_evalTypeAddVar(EXP_EvalTypeContext* ctx)
{
    EXP_EvalTypeDesc desc = { EXP_EvalTypeType_Var };
    return EXP_evalTypeAdd(ctx, &desc);
}

u32 EXP_evalTypeAddFun(EXP_EvalTypeContext* ctx, u32 numIns, const u32* ins, u32 numOuts, const u32* outs)
{
    EXP_EvalTypeDesc desc = { EXP_EvalTypeType_Fun };
    return EXP_evalTypeAdd(ctx, &desc);
}








bool EXP_evalTypeMatch(u32 pat, u32 x)
{
    if (EXP_EvalValueType_Any == pat)
    {
        return true;
    }
    return pat == x;
}


bool EXP_evalTypeUnify(u32 a, u32 b, u32* out)
{
    if (EXP_EvalValueType_Any == a)
    {
        *out = b;
        return true;
    }
    if (EXP_EvalValueType_Any == b)
    {
        *out = a;
        return true;
    }
    if (a != b)
    {
        return false;
    }
    *out = a;
    return true;
}





















































































































