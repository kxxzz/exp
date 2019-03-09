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

u32 EXP_evalTypeVar(EXP_EvalTypeContext* ctx, u32 var)
{
    EXP_EvalTypeDesc desc = { EXP_EvalTypeType_Var };
    desc.var = var;
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






const EXP_EvalTypeDesc* EXP_evalTypeDescById(EXP_EvalTypeContext* ctx, u32 typeId)
{
    return upoolElmData(ctx->typePool, typeId);
}

const u32* EXP_evalTypeListById(EXP_EvalTypeContext* ctx, u32 listId)
{
    return upoolElmData(ctx->listPool, listId);
}













u32* EXP_evalTypeVarTableGet(EXP_EvalTypeVarTable* vtable, u32 vtableBase, u32 var)
{
    assert(vtableBase <= vtable->length);
    for (u32 i = vtableBase; i < vtable->length; ++i)
    {
        if (vtable->data[i].id == var)
        {
            return &vtable->data[i].val;
        }
    }
    return NULL;
}

void EXP_evalTypeVarTableAdd(EXP_EvalTypeVarTable* vtable, u32 var, u32 value)
{
    EXP_EvalTypeVarBinding b = { var, value };
    vec_push(vtable, b);
}



















u32 EXP_evalTypeNormForm(EXP_EvalTypeContext* ctx, EXP_EvalTypeVarTable* vtable, u32 vtableBase, u32 x)
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
        u32* pValue = EXP_evalTypeVarTableGet(vtable, vtableBase, desc->var);
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




















bool EXP_evalTypeUnify
(
    EXP_EvalTypeContext* ctx, EXP_EvalTypeVarTable* vtable, u32 vtableBase,
    u32 a, u32 b, u32* u
)
{
enter:
    if (a == b)
    {
        *u = a;
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
            u32* pValue = EXP_evalTypeVarTableGet(vtable, vtableBase, descA->var);
            if (pValue)
            {
                a = *pValue;
                goto enter;
            }
            EXP_evalTypeVarTableAdd(vtable, descA->var, b);
            *u = b;
            return true;
        }
        else
        {
            return false;
        }
    }
}






















bool EXP_evalTypePatBindUnify(EXP_EvalTypeContext* ctx, EXP_EvalTypeVarTable* patVtable, u32 pat, u32 x)
{
    const EXP_EvalTypeDesc* descPat = EXP_evalTypeDescById(ctx, pat);
    const EXP_EvalTypeDesc* descX = EXP_evalTypeDescById(ctx, x);
    u32* pValue = NULL;
    if (EXP_EvalTypeType_Var == descPat->type)
    {
        pValue = EXP_evalTypeVarTableGet(patVtable, 0, descPat->var);
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
        EXP_evalTypeVarTableAdd(patVtable, descPat->var, x);
        return true;
    }
    return true;
}





u32 EXP_evalTypePatSubst(EXP_EvalTypeContext* ctx, EXP_EvalTypeVarTable* patVtable, u32 pat)
{
    u32 x = EXP_evalTypeNormForm(ctx, patVtable, 0, pat);
    return x;
}


















































































































