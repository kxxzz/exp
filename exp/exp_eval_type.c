#include "exp_eval_a.h"
#include "exp_eval_type_a.h"




typedef struct EXP_EvalTypeBuildLevel
{
    u32 src;
    u32 src1;
    u32 p;
    vec_u32 elms;
} EXP_EvalTypeBuildLevel;

static void EXP_evalTypeBuildLevel(EXP_EvalTypeBuildLevel* l)
{
    vec_free(&l->elms);
}


typedef vec_t(EXP_EvalTypeBuildLevel) EXP_EvalTypeBuildStack;

static void EXP_evalTypeBuildStackPop(EXP_EvalTypeBuildStack* stack)
{
    EXP_EvalTypeBuildLevel* l = &vec_last(stack);
    EXP_evalTypeBuildLevel(l);
    vec_pop(stack);
}

static void EXP_evalTypeBuildStackResize(EXP_EvalTypeBuildStack* stack, u32 n)
{
    assert(n < stack->length);
    for (u32 i = n; i < stack->length; ++i)
    {
        EXP_EvalTypeBuildLevel* l = stack->data + i;
        EXP_evalTypeBuildLevel(l);
    }
    vec_resize(stack, n);
}

static void EXP_evalTypeBuildStackFree(EXP_EvalTypeBuildStack* stack)
{
    for (u32 i = 0; i < stack->length; ++i)
    {
        EXP_EvalTypeBuildLevel* l = stack->data + i;
        EXP_evalTypeBuildLevel(l);
    }
    vec_free(stack);
}





typedef struct EXP_EvalTypeContext
{
    Upool* dataPool;
    vec_char descBuffer;
    EXP_EvalTypeBuildStack buildStack;
} EXP_EvalTypeContext;


EXP_EvalTypeContext* EXP_newEvalTypeContext(void)
{
    EXP_EvalTypeContext* ctx = zalloc(sizeof(EXP_EvalTypeContext));
    ctx->dataPool = newUpool(256);
    return ctx;
}

void EXP_evalTypeContextFree(EXP_EvalTypeContext* ctx)
{
    assert(0 == ctx->buildStack.length);
    vec_free(&ctx->buildStack);
    vec_free(&ctx->descBuffer);
    upoolFree(ctx->dataPool);
    free(ctx);
}






static EXP_EvalTypeDesc* EXP_evalTypeDescBufferReset(EXP_EvalTypeContext* ctx, u32 size)
{
    vec_resize(&ctx->descBuffer, size);
    memset(ctx->descBuffer.data, 0, sizeof(ctx->descBuffer.data[0])*size);
    return (EXP_EvalTypeDesc*)ctx->descBuffer.data;
}


static u32 EXP_evalTypeId(EXP_EvalTypeContext* ctx)
{
    u32 id = upoolElm(ctx->dataPool, ctx->descBuffer.data, ctx->descBuffer.length, NULL);
    return id;
}






u32 EXP_evalTypeAtom(EXP_EvalTypeContext* ctx, u32 atype)
{
    EXP_EvalTypeDesc* desc = EXP_evalTypeDescBufferReset(ctx, sizeof(EXP_EvalTypeDesc));
    desc->type = EXP_EvalTypeType_Atom;
    desc->atype = atype;
    return EXP_evalTypeId(ctx);
}

u32 EXP_evalTypeList(EXP_EvalTypeContext* ctx, u32 count, const u32* elms)
{
    u32 size = offsetof(EXP_EvalTypeDesc, list.elms) + sizeof(elms[0])*count;
    EXP_EvalTypeDesc* desc = EXP_evalTypeDescBufferReset(ctx, size);
    desc->type = EXP_EvalTypeType_List;
    desc->list.count = count;
    memcpy(desc->list.elms, elms, sizeof(elms[0])*count);
    return EXP_evalTypeId(ctx);
}

u32 EXP_evalTypeVar(EXP_EvalTypeContext* ctx, u32 varId)
{
    EXP_EvalTypeDesc* desc = EXP_evalTypeDescBufferReset(ctx, sizeof(EXP_EvalTypeDesc));
    desc->type = EXP_EvalTypeType_Var;
    desc->varId = varId;
    return EXP_evalTypeId(ctx);
}

u32 EXP_evalTypeFun(EXP_EvalTypeContext* ctx, u32 ins, u32 outs)
{
    EXP_EvalTypeDesc* desc = EXP_evalTypeDescBufferReset(ctx, sizeof(EXP_EvalTypeDesc));
    desc->type = EXP_EvalTypeType_Fun;
    desc->fun.ins = ins;
    desc->fun.outs = outs;
    return EXP_evalTypeId(ctx);
}

u32 EXP_evalTypeArray(EXP_EvalTypeContext* ctx, u32 elm)
{
    EXP_EvalTypeDesc* desc = EXP_evalTypeDescBufferReset(ctx, sizeof(EXP_EvalTypeDesc));
    desc->type = EXP_EvalTypeType_Array;
    desc->ary.elm = elm;
    return EXP_evalTypeId(ctx);
}










const EXP_EvalTypeDesc* EXP_evalTypeDescById(EXP_EvalTypeContext* ctx, u32 typeId)
{
    return upoolElmData(ctx->dataPool, typeId);
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
    case EXP_EvalTypeType_List:
    {
        const EXP_EvalTypeDescList* list = &desc->list;
        if (top->p > 0)
        {
            vec_push(&top->elms, r);
        }
        u32 p = top->p++;
        if (p < list->count)
        {
            EXP_EvalTypeBuildLevel l = { list->elms[p] };
            vec_push(buildStack, l);
        }
        else
        {
            assert(p == list->count);
            r = EXP_evalTypeList(ctx, list->count, top->elms.data);
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
    case EXP_EvalTypeType_Fun:
    {
        const EXP_EvalTypeDescFun* fun = &desc->fun;
        if (top->p > 0)
        {
            vec_push(&top->elms, r);
        }
        u32 p = top->p++;
        if (p < 2)
        {
            EXP_EvalTypeBuildLevel l = { 0 };
            if (p < 1)
            {
                l.src = fun->ins;
            }
            else
            {
                l.src = fun->outs;
            }
            vec_push(buildStack, l);
        }
        else
        {
            assert(2 == p);
            assert(2 == top->elms.length);
            r = EXP_evalTypeFun(ctx, top->elms.data[0], top->elms.data[1]);
            EXP_evalTypeBuildStackPop(buildStack);
        }
        break;
    }
    case EXP_EvalTypeType_Array:
    {
        const EXP_EvalTypeDescArray* ary = &desc->ary;
        if (top->p > 0)
        {
            r = EXP_evalTypeArray(ctx, r);
            EXP_evalTypeBuildStackPop(buildStack);
        }
        else
        {
            top->p++;
            EXP_EvalTypeBuildLevel l = { ary->elm };
            vec_push(buildStack, l);
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
            assert(descA->atype != descB->atype);
            goto failed;
        }
        case EXP_EvalTypeType_List:
        {
            const EXP_EvalTypeDescList* listA = &descA->list;
            const EXP_EvalTypeDescList* listB = &descB->list;
            if (top->p > 0)
            {
                vec_push(&top->elms, r);
                assert(listA->count == listB->count);
            }
            else
            {
                if (listA->count != listB->count)
                {
                    goto failed;
                }
            }
            u32 p = top->p++;
            if (p < listA->count)
            {
                EXP_EvalTypeBuildLevel l = { listA->elms[p], listB->elms[p] };
                vec_push(buildStack, l);
            }
            else
            {
                assert(p == listA->count);
                r = EXP_evalTypeList(ctx, listA->count, top->elms.data);
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
            }
            else if (aV && !bV)
            {
                r = EXP_evalTypeReduct(ctx, varSpace, *aV);
            }
            else if (!aV && bV)
            {
                r = EXP_evalTypeReduct(ctx, varSpace, *bV);
            }
            else
            {
                assert(!aV && !bV);
                r = a;
                EXP_evalTypeVarBind(varSpace, descA->varId, b);
            }
            goto next;
        }
        case EXP_EvalTypeType_Fun:
        {
            const EXP_EvalTypeDescFun* funA = &descA->fun;
            const EXP_EvalTypeDescFun* funB = &descB->fun;
            if (top->p > 0)
            {
                vec_push(&top->elms, r);
            }
            u32 p = top->p++;
            if (p < 2)
            {
                EXP_EvalTypeBuildLevel l = { 0 };
                if (p < 1)
                {
                    l.src = funA->ins;
                    l.src1 = funB->ins;
                }
                else
                {
                    l.src = funA->outs;
                    l.src1 = funB->outs;
                }
                vec_push(buildStack, l);
            }
            else
            {
                assert(2 == p);
                assert(2 == top->elms.length);
                r = EXP_evalTypeFun(ctx, top->elms.data[0], top->elms.data[1]);
                EXP_evalTypeBuildStackPop(buildStack);
            }
            goto next;
        }
        case EXP_EvalTypeType_Array:
        {
            const EXP_EvalTypeDescArray* aryA = &descA->ary;
            const EXP_EvalTypeDescArray* aryB = &descB->ary;
            if (top->p > 0)
            {
                r = EXP_evalTypeArray(ctx, r);
                EXP_evalTypeBuildStackPop(buildStack);
            }
            else
            {
                top->p++;
                EXP_EvalTypeBuildLevel l = { aryA->elm, aryB->elm };
                vec_push(buildStack, l);
            }
            break;
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
    case EXP_EvalTypeType_List:
    {
        const EXP_EvalTypeDescList* list = &desc->list;
        if (top->p > 0)
        {
            vec_push(&top->elms, r);
        }
        u32 p = top->p++;
        if (p < list->count)
        {
            EXP_EvalTypeBuildLevel l = { list->elms[p] };
            vec_push(buildStack, l);
        }
        else
        {
            assert(p == list->count);
            r = EXP_evalTypeList(ctx, list->count, top->elms.data);
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
    case EXP_EvalTypeType_Fun:
    {
        const EXP_EvalTypeDescFun* fun = &desc->fun;
        if (top->p > 0)
        {
            vec_push(&top->elms, r);
        }
        u32 p = top->p++;
        if (p < 2)
        {
            EXP_EvalTypeBuildLevel l = { 0 };
            if (p < 1)
            {
                l.src = fun->ins;
            }
            else
            {
                l.src = fun->outs;
            }
            vec_push(buildStack, l);
        }
        else
        {
            assert(2 == p);
            assert(2 == top->elms.length);
            r = EXP_evalTypeFun(ctx, top->elms.data[0], top->elms.data[1]);
            EXP_evalTypeBuildStackPop(buildStack);
        }
        break;
    }
    case EXP_EvalTypeType_Array:
    {
        const EXP_EvalTypeDescArray* ary = &desc->ary;
        if (top->p > 0)
        {
            r = EXP_evalTypeArray(ctx, r);
            EXP_evalTypeBuildStackPop(buildStack);
        }
        else
        {
            top->p++;
            EXP_EvalTypeBuildLevel l = { ary->elm };
            vec_push(buildStack, l);
        }
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






















































































































