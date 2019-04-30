#include "exp_eval_a.h"
#include "exp_eval_type_a.h"




typedef struct EXP_EvalTypeBuildLevel
{
    u32 src;
    u32 src1;
    bool hasRet;
    vec_u32 elms;
} EXP_EvalTypeBuildLevel;

typedef vec_t(EXP_EvalTypeBuildLevel) EXP_EvalTypeBuildStack;

static void EXP_evalTypeBuildStackPop(EXP_EvalTypeBuildStack* stack)
{
    EXP_EvalTypeBuildLevel* l = &vec_last(stack);
    vec_free(&l->elms);
    vec_pop(stack);
}

static void EXP_evalTypeBuildStackResize(EXP_EvalTypeBuildStack* stack, u32 n)
{
    assert(n < stack->length);
    for (u32 i = n; i < stack->length; ++i)
    {
        EXP_EvalTypeBuildLevel* l = stack->data + i;
        vec_free(&l->elms);
    }
    vec_resize(stack, n);
}

static void EXP_evalTypeBuildStackFree(EXP_EvalTypeBuildStack* stack)
{
    for (u32 i = 0; i < stack->length; ++i)
    {
        EXP_EvalTypeBuildLevel* l = stack->data + i;
        vec_free(&l->elms);
    }
    vec_free(stack);
}





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
        EXP_evalTypeBuildStackPop(buildStack);
        break;
    }
    case EXP_EvalTypeType_Fun:
    {
        const EXP_EvalTypeDescFun* fun = &desc->fun;
        if (top->hasRet)
        {
            vec_push(&top->elms, r);
        }
        else
        {
            top->hasRet = true;
        }
        u32 p = top->elms.length;
        if (p < fun->ins.count + fun->outs.count)
        {
            const u32* funIns = EXP_evalTypeListById(ctx, fun->ins.list);
            const u32* funOuts = EXP_evalTypeListById(ctx, fun->outs.list);
            EXP_EvalTypeBuildLevel l = { 0 };
            if (p < fun->ins.count)
            {
                l.src = funIns[p];
            }
            else
            {
                l.src = funOuts[p - fun->ins.count];
            }
            vec_push(buildStack, l);
        }
        else
        {
            r = EXP_evalTypeFun(ctx, fun->ins.count, top->elms.data, fun->outs.count, top->elms.data + fun->ins.count);
            EXP_evalTypeBuildStackPop(buildStack);
        }
        break;
    }
    case EXP_EvalTypeType_Var:
    {
        u32* pV = EXP_evalTypeVarValue(varSpace, desc->varId);
        if (pV)
        {
            EXP_evalTypeBuildStackPop(buildStack);
            EXP_EvalTypeBuildLevel l = { *pV };
            vec_push(buildStack, l);
        }
        else
        {
            r = top->src;
            EXP_evalTypeBuildStackPop(buildStack);
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
        EXP_evalTypeBuildStackPop(buildStack);
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
        case EXP_EvalTypeType_Fun:
        {
            const EXP_EvalTypeDescFun* funA = &descA->fun;
            const EXP_EvalTypeDescFun* funB = &descB->fun;
            if (top->hasRet)
            {
                vec_push(&top->elms, r);
                assert(funA->ins.count == funB->ins.count);
                assert(funA->outs.count == funB->outs.count);
            }
            else
            {
                top->hasRet = true;
                if (funA->ins.count != funB->ins.count)
                {
                    goto failed;
                }
                if (funA->outs.count != funB->outs.count)
                {
                    goto failed;
                }
            }
            u32 p = top->elms.length;
            if (p < funA->ins.count + funA->outs.count)
            {
                const u32* funInsA = EXP_evalTypeListById(ctx, funA->ins.list);
                const u32* funOutsA = EXP_evalTypeListById(ctx, funA->outs.list);
                const u32* funInsB = EXP_evalTypeListById(ctx, funB->ins.list);
                const u32* funOutsB = EXP_evalTypeListById(ctx, funB->outs.list);
                EXP_EvalTypeBuildLevel l = { 0 };
                if (p < funA->ins.count)
                {
                    l.src = funInsA[p];
                    l.src1 = funInsB[p];
                }
                else
                {
                    l.src = funOutsA[p - funA->ins.count];
                    l.src1 = funOutsB[p - funA->ins.count];
                }
                vec_push(buildStack, l);
            }
            else
            {
                r = EXP_evalTypeFun(ctx, funA->ins.count, top->elms.data, funA->outs.count, top->elms.data + funA->ins.count);
                EXP_evalTypeBuildStackPop(buildStack);
            }
            goto next;
        }
        case EXP_EvalTypeType_Var:
        {
            EXP_evalTypeBuildStackPop(buildStack);
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
        EXP_evalTypeBuildStackPop(buildStack);
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
    EXP_evalTypeBuildStackResize(buildStack, 0);
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
        EXP_evalTypeBuildStackPop(buildStack);
        break;
    }
    case EXP_EvalTypeType_Fun:
    {
        const EXP_EvalTypeDescFun* fun = &desc->fun;
        if (top->hasRet)
        {
            vec_push(&top->elms, r);
        }
        else
        {
            top->hasRet = true;
        }
        u32 p = top->elms.length;
        if (p < fun->ins.count + fun->outs.count)
        {
            const u32* funIns = EXP_evalTypeListById(ctx, fun->ins.list);
            const u32* funOuts = EXP_evalTypeListById(ctx, fun->outs.list);
            EXP_EvalTypeBuildLevel l = { 0 };
            if (p < fun->ins.count)
            {
                l.src = funIns[p];
            }
            else
            {
                l.src = funOuts[p - fun->ins.count];
            }
            vec_push(buildStack, l);
        }
        else
        {
            r = EXP_evalTypeFun(ctx, fun->ins.count, top->elms.data, fun->outs.count, top->elms.data + fun->ins.count);
            EXP_evalTypeBuildStackPop(buildStack);
        }
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
        EXP_evalTypeBuildStackPop(buildStack);
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






















































































































