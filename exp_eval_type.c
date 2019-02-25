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

typedef struct EXP_EvalTypeDescList
{
    u32 count;
    u32 id;
} EXP_EvalTypeDescList;

typedef struct EXP_EvalTypeDescFun
{
    EXP_EvalTypeDescList ins;
    EXP_EvalTypeDescList outs;
} EXP_EvalTypeDescFun;

typedef struct EXP_EvalTypeDesc
{
    EXP_EvalTypeType type;
    union
    {
        u32 id;
        EXP_EvalTypeDescFun fun;
        EXP_EvalTypeDescList tuple;
        u32 aryElm;
    };
} EXP_EvalTypeDesc;




typedef struct EXP_EvalTypeContext
{
    Upool* listPool;
    Upool* typePool;
} EXP_EvalTypeContext;


EXP_EvalTypeContext* EXP_newEvalTypeContext(void)
{
    EXP_EvalTypeContext* ctx = zalloc(sizeof(EXP_EvalTypeContext));
    return ctx;
}

void EXP_evalTypeContextFree(EXP_EvalTypeContext* ctx)
{
    upoolFree(ctx->typePool);
    upoolFree(ctx->listPool);
    free(ctx);
}






static u32 EXP_evalTypeAdd(EXP_EvalTypeContext* ctx, const EXP_EvalTypeDesc* desc)
{
    u32 id = upoolAddElm(ctx->typePool, sizeof(*desc), desc, NULL);
    return id;
}

static u32 EXP_evalTypeAddList(EXP_EvalTypeContext* ctx, u32 count, const u32* elms)
{
    u32 id = upoolAddElm(ctx->listPool, sizeof(*elms)*count, elms, NULL);
    return id;
}





u32 EXP_evalTypeAddNval(EXP_EvalTypeContext* ctx, u32 nativeType)
{
    EXP_EvalTypeDesc desc = { EXP_EvalTypeType_Nval };
    desc.id = nativeType;
    return EXP_evalTypeAdd(ctx, &desc);
}

u32 EXP_evalTypeAddVar(EXP_EvalTypeContext* ctx, u32 varId)
{
    EXP_EvalTypeDesc desc = { EXP_EvalTypeType_Var };
    desc.id = varId;
    return EXP_evalTypeAdd(ctx, &desc);
}

u32 EXP_evalTypeAddFun(EXP_EvalTypeContext* ctx, u32 numIns, const u32* ins, u32 numOuts, const u32* outs)
{
    EXP_EvalTypeDesc desc = { EXP_EvalTypeType_Fun };
    desc.fun.ins.count = numIns;
    desc.fun.outs.count = numOuts;
    desc.fun.ins.id = EXP_evalTypeAddList(ctx, numIns, ins);
    desc.fun.outs.id = EXP_evalTypeAddList(ctx, numOuts, outs);
    return EXP_evalTypeAdd(ctx, &desc);
}

u32 EXP_evalTypeAddTuple(EXP_EvalTypeContext* ctx, u32 count, const u32* elms)
{
    EXP_EvalTypeDesc desc = { EXP_EvalTypeType_Tuple };
    desc.tuple.count = count;
    desc.tuple.id = EXP_evalTypeAddList(ctx, count, elms);
    return EXP_evalTypeAdd(ctx, &desc);
}

u32 EXP_evalTypeAddArray(EXP_EvalTypeContext* ctx, u32 elm)
{
    EXP_EvalTypeDesc desc = { EXP_EvalTypeType_Array };
    desc.aryElm = elm;
    return EXP_evalTypeAdd(ctx, &desc);
}










typedef struct EXP_EvalTypeVarBinding
{
    u32 var;
    u32 type;
} EXP_EvalTypeVarBinding;

typedef vec_t(EXP_EvalTypeVarBinding) EXP_EvalTypeVarEnv;


bool EXP_evalTypeUnifyX(EXP_EvalTypeContext* ctx, EXP_EvalTypeVarEnv* env, u32 a, u32 b)
{

    return true;
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





















































































































