#include "exp_eval_a.h"
#include "exp_eval_type_a.h"




typedef struct EXP_EvalTypeBuildLevel
{
    u32 src;
    u32 src1;
    u32 progress;
    union
    {
        EXP_EvalTypeDescFun fun;
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












u32 EXP_evalTypeNewVar(EXP_EvalTypeVarSpace* varSpace)
{
    assert(varSpace->varCount != -1);
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
    assert(varSpace->varCount > varId);
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
    u32 r = -1;
    EXP_EvalTypeBuildLevel root = { x };
    vec_push(buildStack, root);
    EXP_EvalTypeBuildLevel* top = NULL;
    const EXP_EvalTypeDesc* desc = NULL;
next:
    if (!buildStack->length)
    {
        assert(r != -1);
        return r;
    }
    top = &vec_last(buildStack);
    desc = EXP_evalTypeDescById(ctx, top->src);
    switch (desc->type)
    {
    case EXP_EvalTypeType_Atom:
    {
        r = top->src;
        vec_pop(buildStack);
        break;
    }
    case EXP_EvalTypeType_Var:
    {
        u32* pV = EXP_evalTypeVarValue(varSpace, desc->varId);
        if (pV)
        {
            vec_pop(buildStack);
            EXP_EvalTypeBuildLevel l = { *pV };
            vec_push(buildStack, l);
        }
        else
        {
            r = top->src;
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
    EXP_EvalTypeBuildStack* buildStack = &ctx->buildStack;
    u32 r = -1;
    EXP_EvalTypeBuildLevel root = { a, b };
    vec_push(buildStack, root);
    EXP_EvalTypeBuildLevel* top = NULL;
    const EXP_EvalTypeDesc* descA = NULL;
    const EXP_EvalTypeDesc* descB = NULL;
next:
    if (!buildStack->length)
    {
        assert(r != -1);
        *pU = EXP_evalTypeReduct(ctx, varSpace, r);
        return true;
    }
    top = &vec_last(buildStack);
    a = top->src;
    b = top->src1;
    if (a == b)
    {
        vec_pop(buildStack);
        r = EXP_evalTypeReduct(ctx, varSpace, a);
        goto next;
    }
    descA = EXP_evalTypeDescById(ctx, a);
    descB = EXP_evalTypeDescById(ctx, b);
    if (descA->type == descB->type)
    {
        switch (descA->type)
        {
        case EXP_EvalTypeType_Atom:
        {
            assert(descA->atom != descB->atom);
            goto failed;
        }
        case EXP_EvalTypeType_Var:
        {
            vec_pop(buildStack);
            u32* aV = EXP_evalTypeVarValue(varSpace, descA->varId);
            u32* bV = EXP_evalTypeVarValue(varSpace, descB->varId);
            if (aV && bV)
            {
                a = *aV;
                b = *bV;
                EXP_EvalTypeBuildLevel l = { a, b };
                vec_push(buildStack, l);
                goto next;
            }
            else if (aV && !bV)
            {
                r = EXP_evalTypeReduct(ctx, varSpace, *aV);
                goto next;
            }
            else if (!aV && bV)
            {
                r = EXP_evalTypeReduct(ctx, varSpace, *bV);
                goto next;
            }
            else
            {
                assert(!aV && !bV);
                r = a;
                EXP_evalTypeVarBind(varSpace, descA->varId, b);
                goto next;
            }
        }
        default:
            goto failed;
        }
    }
    else
    {
        vec_pop(buildStack);
        if (EXP_EvalTypeType_Var == descA->type)
        {
            u32* pV = EXP_evalTypeVarValue(varSpace, descA->varId);
            if (pV)
            {
                a = *pV;
                EXP_EvalTypeBuildLevel l = { a, b };
                vec_push(buildStack, l);
                goto next;
            }
            r = EXP_evalTypeReduct(ctx, varSpace, b);
            EXP_evalTypeVarBind(varSpace, descA->varId, r);
            goto next;
        }
        else if (EXP_EvalTypeType_Var == descB->type)
        {
            u32* pV = EXP_evalTypeVarValue(varSpace, descB->varId);
            if (pV)
            {
                b = *pV;
                EXP_EvalTypeBuildLevel l = { a, b };
                vec_push(buildStack, l);
                goto next;
            }
            r = EXP_evalTypeReduct(ctx, varSpace, a);
            EXP_evalTypeVarBind(varSpace, descB->varId, r);
            goto next;
        }
        else
        {
            goto failed;
        }
    }
failed:
    vec_resize(buildStack, 0);
    return false;
}


















u32 EXP_evalTypeFromPat
(
    EXP_EvalTypeContext* ctx, EXP_EvalTypeVarSpace* varSpace, EXP_EvalTypeVarSpace* varRenMap, u32 pat
)
{
    EXP_EvalTypeBuildStack* buildStack = &ctx->buildStack;
    u32 r = -1;
    EXP_EvalTypeBuildLevel root = { pat };
    vec_push(buildStack, root);
    EXP_EvalTypeBuildLevel* top = NULL;
    const EXP_EvalTypeDesc* desc = NULL;
next:
    if (!buildStack->length)
    {
        assert(r != -1);
        r = EXP_evalTypeReduct(ctx, varSpace, r);
        return r;
    }
    top = &vec_last(buildStack);
    desc = EXP_evalTypeDescById(ctx, top->src);
    switch (desc->type)
    {
    case EXP_EvalTypeType_Atom:
    {
        r = top->src;
        vec_pop(buildStack);
        break;
    }
    case EXP_EvalTypeType_Var:
    {
        u32* pV = EXP_evalTypeVarValue(varRenMap, desc->varId);
        if (pV)
        {
            r = *pV;
        }
        else
        {
            u32 varId = EXP_evalTypeNewVar(varSpace);
            r = EXP_evalTypeVar(ctx, varId);
            EXP_evalTypeVarBind(varRenMap, desc->varId, r);
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






















































































































