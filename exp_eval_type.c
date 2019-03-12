#include "exp_eval_a.h"
#include "exp_eval_type_a.h"





typedef struct EXP_EvalTypeContext
{
    Upool* listPool;
    Upool* typePool;
} EXP_EvalTypeContext;


EXP_EvalTypeContext* EXP_newEvalTypeContext(void)
{
    EXP_EvalTypeContext* ctx = zalloc(sizeof(EXP_EvalTypeContext));
    ctx->listPool = newUpool(256);
    ctx->typePool = newUpool(256);
    return ctx;
}

void EXP_evalTypeContextFree(EXP_EvalTypeContext* ctx)
{
    upoolFree(ctx->typePool);
    upoolFree(ctx->listPool);
    free(ctx);
}






static u32 EXP_evalTypeIdByDesc(EXP_EvalTypeContext* ctx, const EXP_EvalTypeDesc* desc)
{
    u32 id = upoolAddElm(ctx->typePool, desc, sizeof(*desc), NULL);
    return id;
}

static u32 EXP_evalTypeList(EXP_EvalTypeContext* ctx, u32 count, const u32* elms)
{
    u32 id = upoolAddElm(ctx->listPool, elms, sizeof(*elms)*count, NULL);
    return id;
}





u32 EXP_evalTypeAtom(EXP_EvalTypeContext* ctx, u32 atom)
{
    EXP_EvalTypeDesc desc = { EXP_EvalTypeType_Atom };
    desc.atom = atom;
    return EXP_evalTypeIdByDesc(ctx, &desc);
}

u32 EXP_evalTypeFun(EXP_EvalTypeContext* ctx, u32 numIns, const u32* ins, u32 numOuts, const u32* outs)
{
    EXP_EvalTypeDesc desc = { EXP_EvalTypeType_Fun };
    desc.fun.ins.count = numIns;
    desc.fun.outs.count = numOuts;
    desc.fun.ins.list = EXP_evalTypeList(ctx, numIns, ins);
    desc.fun.outs.list = EXP_evalTypeList(ctx, numOuts, outs);
    return EXP_evalTypeIdByDesc(ctx, &desc);
}

u32 EXP_evalTypeTuple(EXP_EvalTypeContext* ctx, u32 count, const u32* elms)
{
    EXP_EvalTypeDesc desc = { EXP_EvalTypeType_Tuple };
    desc.tuple.count = count;
    desc.tuple.list = EXP_evalTypeList(ctx, count, elms);
    return EXP_evalTypeIdByDesc(ctx, &desc);
}

u32 EXP_evalTypeArray(EXP_EvalTypeContext* ctx, u32 elm)
{
    EXP_EvalTypeDesc desc = { EXP_EvalTypeType_Array };
    desc.aryElm = elm;
    return EXP_evalTypeIdByDesc(ctx, &desc);
}

u32 EXP_evalTypeVar(EXP_EvalTypeContext* ctx, u32 var)
{
    EXP_EvalTypeDesc desc = { EXP_EvalTypeType_Var };
    desc.var = var;
    return EXP_evalTypeIdByDesc(ctx, &desc);
}

u32 EXP_evalTypeVarS1(EXP_EvalTypeContext* ctx, u32 var)
{
    EXP_EvalTypeDesc desc = { EXP_EvalTypeType_VarS1 };
    desc.var = var;
    return EXP_evalTypeIdByDesc(ctx, &desc);
}






const EXP_EvalTypeDesc* EXP_evalTypeDescById(EXP_EvalTypeContext* ctx, u32 typeId)
{
    return upoolElmData(ctx->typePool, typeId);
}

const u32* EXP_evalTypeListById(EXP_EvalTypeContext* ctx, u32 listId)
{
    return upoolElmData(ctx->listPool, listId);
}













u32* EXP_evalTypeVarValue(EXP_EvalTypeVarSpace* space, u32 var)
{
    for (u32 i = 0; i < space->length; ++i)
    {
        if (space->data[i].id == var)
        {
            return &space->data[i].val;
        }
    }
    return NULL;
}

void EXP_evalTypeVarAdd(EXP_EvalTypeVarSpace* space, u32 var, u32 value)
{
    EXP_EvalTypeVarBinding b = { var, value };
    vec_push(space, b);
}



















u32 EXP_evalTypeNormForm(EXP_EvalTypeContext* ctx, EXP_EvalTypeVarSpace* space, u32 x)
{
    const EXP_EvalTypeDesc* desc = NULL;
enter:
    desc = EXP_evalTypeDescById(ctx, x);
    switch (desc->type)
    {
    case EXP_EvalTypeType_Atom:
    {
        return x;
    }
    case EXP_EvalTypeType_Var:
    {
        u32* pValue = EXP_evalTypeVarValue(space, desc->var);
        if (pValue)
        {
            x = *pValue;
            goto enter;
        }
        return x;
    }
    default:
        assert(false);
        return x;
    }
}







u32 EXP_evalTypeToS1Form(EXP_EvalTypeContext* ctx, EXP_EvalTypeVarSpace* space, u32 x)
{
    const EXP_EvalTypeDesc* desc = NULL;
//enter:
    desc = EXP_evalTypeDescById(ctx, x);
    switch (desc->type)
    {
    case EXP_EvalTypeType_Atom:
    {
        return x;
    }
    case EXP_EvalTypeType_Var:
    {
        return EXP_evalTypeVarS1(ctx, desc->var);
    }
    default:
        assert(false);
        return x;
    }
}















bool EXP_evalTypeUnify(EXP_EvalTypeContext* ctx, EXP_EvalTypeVarSpace* space, u32 a, u32 b, u32* u)
{
enter:
    if (a == b)
    {
        *u = EXP_evalTypeNormForm(ctx, space, a);
        return true;
    }
    const EXP_EvalTypeDesc* descA = EXP_evalTypeDescById(ctx, a);
    const EXP_EvalTypeDesc* descB = EXP_evalTypeDescById(ctx, b);
    if (descA->type == descB->type)
    {
        switch (descA->type)
        {
        case EXP_EvalTypeType_Atom:
        {
            assert(descA->atom != descB->atom);
            return false;
        }
        default:
            assert(false);
            return false;
        }
    }
    else
    {
    swap:
        if (EXP_EvalTypeType_Var == descB->type)
        {
            u32 t = b;
            b = a;
            a = t;
            const EXP_EvalTypeDesc* descC = descB;
            descB = descA;
            descA = descC;
            goto swap;
        }
        if (EXP_EvalTypeType_Var == descA->type)
        {
            u32* pValue = EXP_evalTypeVarValue(space, descA->var);
            if (pValue)
            {
                a = *pValue;
                goto enter;
            }
            EXP_evalTypeVarAdd(space, descA->var, b);
            *u = EXP_evalTypeNormForm(ctx, space, b);
            return true;
        }
        else
        {
            return false;
        }
    }
}






















bool EXP_evalTypeUnifyVarS1
(
    EXP_EvalTypeContext* ctx, EXP_EvalTypeVarSpace* space, u32 x, EXP_EvalTypeVarSpace* space1, u32 x1
)
{
    const EXP_EvalTypeDesc* descX = EXP_evalTypeDescById(ctx, x);
    const EXP_EvalTypeDesc* descX1 = EXP_evalTypeDescById(ctx, x1);
    u32* pValue = NULL;
    if (EXP_EvalTypeType_Var == descX1->type)
    {
        pValue = EXP_evalTypeVarValue(space1, descX1->var);
    }
    if (pValue)
    {
        u32 v = *pValue;
        if (EXP_EvalTypeType_Var == descX->type)
        {
            return true;
        }
        else
        {
            return v == x;
        }
    }
    else
    {
        EXP_evalTypeVarAdd(space1, descX1->var, x);
        return true;
    }
    return true;
}























































































































