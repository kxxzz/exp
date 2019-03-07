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






typedef struct EXP_EvalTypeVarBinding
{
    u32 varId;
    u32 value;
} EXP_EvalTypeVarBinding;

typedef vec_t(EXP_EvalTypeVarBinding) EXP_EvalTypeVarBindingVec;



typedef struct EXP_EvalTypeVarTable
{
    EXP_EvalTypeVarBindingVec vars;
} EXP_EvalTypeVarTable;

EXP_EvalTypeVarTable* EXP_newEvalTypeVarTable(void)
{
    EXP_EvalTypeVarTable* t = zalloc(sizeof(*t));
    return t;
}

void EXP_evalTypeVarTableFree(EXP_EvalTypeVarTable* table)
{
    vec_free(&table->vars);
    free(table);
}




u32* EXP_evalTypeGetVarValue(EXP_EvalTypeVarTable* varTable, u32 varId)
{
    for (u32 i = 0; i < varTable->vars.length; ++i)
    {
        if (varTable->vars.data[i].varId == varId)
        {
            return &varTable->vars.data[i].value;
        }
    }
    return NULL;
}

void EXP_evalTypeAddVar(EXP_EvalTypeVarTable* varTable, u32 varId, u32 value)
{
    EXP_EvalTypeVarBinding b = { varId, value };
    vec_push(&varTable->vars, b);
}






















bool EXP_evalTypeUnifyX(EXP_EvalTypeContext* ctx, EXP_EvalTypeVarTable* varTable, u32 a, u32 b, u32* t)
{
enter:
    if (a == b)
    {
        *t = a;
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
            u32* pValue = EXP_evalTypeGetVarValue(varTable, descA->atom);
            if (pValue)
            {
                a = *pValue;
                goto enter;
            }
            EXP_evalTypeAddVar(varTable, descA->atom, b);
            *t = b;
            return true;
        }
        else
        {
            return false;
        }
    }
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





















































































































