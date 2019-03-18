#include "exp_eval_a.h"
#include "exp_eval_type_a.h"




typedef struct EXP_EvalTypeBuildLevel
{
    u32 id;
    u32 progress;
    union
    {
        EXP_EvalTypeDescFun fun;
        EXP_EvalTypeDescList tuple;
    };
} EXP_EvalTypeBuildLevel;

typedef vec_t(EXP_EvalTypeBuildLevel) EXP_EvalTypeBuildStack;




typedef struct EXP_EvalTypeContext
{
    Upool* listPool;
    Upool* typePool;
    EXP_EvalTypeBuildStack buildStack;
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
    vec_free(&ctx->buildStack);
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

u32 EXP_evalTypeVar(EXP_EvalTypeContext* ctx, u32 varId)
{
    EXP_EvalTypeDesc desc = { EXP_EvalTypeType_Var };
    desc.varId = varId;
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















void EXP_evalTypeVarSpaceFree(EXP_EvalTypeVarSpace* varSpace)
{
    vec_free(&varSpace->bvars);
}

void EXP_evalTypeVarSpaceReset(EXP_EvalTypeVarSpace* varSpace)
{
    vec_resize(&varSpace->bvars, 0);
    varSpace->varCount = 0;
}










u32 EXP_evalTypeNewVar(EXP_EvalTypeVarSpace* varSpace)
{
    return varSpace->varCount++;
}





bool EXP_evalTypeVarIsExist(EXP_EvalTypeVarSpace* varSpace, u32 varId)
{
    return varSpace->varCount > varId;
}

bool EXP_evalTypeVarIsFree(EXP_EvalTypeVarSpace* varSpace, u32 varId)
{
    if (!EXP_evalTypeVarIsExist(varSpace, varId))
    {
        return false;
    }
    for (u32 i = 0; i < varSpace->bvars.length; ++i)
    {
        if (varSpace->bvars.data[i].id == varId)
        {
            return false;
        }
    }
    return true;
}

bool EXP_evalTypeVarIsBound(EXP_EvalTypeVarSpace* varSpace, u32 varId)
{
    return !EXP_evalTypeVarIsFree(varSpace, varId);
}






u32* EXP_evalTypeVarValue(EXP_EvalTypeVarSpace* varSpace, u32 varId)
{
    if (!EXP_evalTypeVarIsExist(varSpace, varId))
    {
        return NULL;
    }
    for (u32 i = 0; i < varSpace->bvars.length; ++i)
    {
        if (varSpace->bvars.data[i].id == varId)
        {
            return &varSpace->bvars.data[i].val;
        }
    }
    return NULL;
}

void EXP_evalTypeVarBind(EXP_EvalTypeVarSpace* varSpace, u32 varId, u32 value)
{
    for (u32 i = 0; i < varSpace->bvars.length; ++i)
    {
        if (varSpace->bvars.data[i].id == varId)
        {
            varSpace->bvars.data[i].val = value;
            return;
        }
    }
    EXP_EvalTypeBvar b = { varId, value };
    vec_push(&varSpace->bvars, b);
}



















u32 EXP_evalTypeReduct(EXP_EvalTypeContext* ctx, EXP_EvalTypeVarSpace* varSpace, u32 x)
{
    EXP_EvalTypeBuildStack* buildStack = &ctx->buildStack;
    bool recheck = false;
    u32 lRet = -1;
    EXP_EvalTypeBuildLevel root = { x };
    vec_push(buildStack, root);
    EXP_EvalTypeBuildLevel* top = NULL;
    const EXP_EvalTypeDesc* desc = NULL;
next:
    if (!buildStack->length)
    {
        assert(lRet != -1);
        if (recheck)
        {
            recheck = false;
            vec_push(buildStack, root);
            goto next;
        }
        return lRet;
    }
    top = &vec_last(buildStack);
    desc = EXP_evalTypeDescById(ctx, top->id);
    switch (desc->type)
    {
    case EXP_EvalTypeType_Atom:
    {
        lRet = top->id;
        vec_pop(buildStack);
        break;
    }
    case EXP_EvalTypeType_Var:
    {
        if (0 == top->progress)
        {
            ++top->progress;
            u32* pV = EXP_evalTypeVarValue(varSpace, desc->varId);
            if (pV)
            {
                u32 v = *pV;
                EXP_EvalTypeBuildLevel l1 = { v };
                vec_push(buildStack, l1);
            }
            else
            {
                lRet = top->id;
            }
        }
        else
        {
            assert(1 == top->progress);
            // Update varSpace
            u32* pV0 = EXP_evalTypeVarValue(varSpace, desc->varId);
            if (pV0)
            {
                u32 v0 = *pV0;
                if (v0 != lRet)
                {
                    recheck = true;
                    EXP_evalTypeVarBind(varSpace, desc->varId, lRet);
                }
            }
            vec_pop(buildStack);
        }
        break;
    }
    default:
        assert(false);
        break;
    }
    goto next;
}


















bool EXP_evalTypeUnify(EXP_EvalTypeContext* ctx, EXP_EvalTypeVarSpace* varSpace, u32 a, u32 b, u32* pU)
{
enter:
    if (a == b)
    {
        *pU = EXP_evalTypeReduct(ctx, varSpace, a);
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
            u32* pV = EXP_evalTypeVarValue(varSpace, descA->varId);
            if (pV)
            {
                a = *pV;
                goto enter;
            }
            *pU = EXP_evalTypeReduct(ctx, varSpace, b);
            return true;
        }
        else
        {
            return false;
        }
    }
}


















u32 EXP_evalTypeFromPat
(
    EXP_EvalTypeContext* ctx, EXP_EvalTypeVarSpace* varSpace, EXP_EvalTypeVarSpace* varRenMap, u32 pat
)
{
    EXP_EvalTypeBuildStack* buildStack = &ctx->buildStack;
    u32 lRet = -1;
    EXP_EvalTypeBuildLevel root = { pat };
    vec_push(buildStack, root);
    EXP_EvalTypeBuildLevel* top = NULL;
    const EXP_EvalTypeDesc* desc = NULL;
next:
    if (!buildStack->length)
    {
        assert(lRet != -1);
        lRet = EXP_evalTypeReduct(ctx, varSpace, lRet);
        return lRet;
    }
    top = &vec_last(buildStack);
    desc = EXP_evalTypeDescById(ctx, top->id);
    switch (desc->type)
    {
    case EXP_EvalTypeType_Atom:
    {
        lRet = top->id;
        vec_pop(buildStack);
        break;
    }
    case EXP_EvalTypeType_Var:
    {
        u32* pV = EXP_evalTypeVarValue(varRenMap, desc->varId);
        if (pV)
        {
            lRet = *pV;
        }
        else
        {
            u32 varId = EXP_evalTypeNewVar(varSpace);
            EXP_evalTypeVarBind(varRenMap, varId, top->id);
            lRet = EXP_evalTypeVar(ctx, varId);
        }
        vec_pop(buildStack);
        break;
    }
    default:
        assert(false);
        break;
    }
    goto next;
}

















bool EXP_evalTypeUnifyPat
(
    EXP_EvalTypeContext* ctx,
    EXP_EvalTypeVarSpace* varSpace, u32 a,
    EXP_EvalTypeVarSpace* varRenMap, u32 pat,
    u32* pU
)
{
    u32 b = EXP_evalTypeFromPat(ctx, varSpace, varRenMap, pat);
    return EXP_evalTypeUnify(ctx, varSpace, a, b, pU);
}























































































































