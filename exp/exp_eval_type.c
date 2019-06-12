#include "exp_eval_a.h"
#include "exp_eval_type_a.h"



enum
{
    EXP_EvalTypeInStackCmd_Reduce = -1,
};



typedef struct EXP_EvalTypeReduce
{
    EXP_EvalTypeType typeType;
    u32 retBase;
} EXP_EvalTypeReduce;

typedef vec_t(EXP_EvalTypeReduce) EXP_EvalTypeReduceStack;


typedef struct EXP_EvalTypeContext
{
    upool_t dataPool;
    vec_char descBuffer;
    vec_u32 inStack[2];
    EXP_EvalTypeReduceStack reduceStack;
    vec_u32 retBuf;
} EXP_EvalTypeContext;


EXP_EvalTypeContext* EXP_newEvalTypeContext(void)
{
    EXP_EvalTypeContext* ctx = zalloc(sizeof(EXP_EvalTypeContext));
    ctx->dataPool = upoolNew(256);
    return ctx;
}

void EXP_evalTypeContextFree(EXP_EvalTypeContext* ctx)
{
    vec_free(&ctx->retBuf);
    vec_free(&ctx->reduceStack);
    vec_free(&ctx->inStack[1]);
    vec_free(&ctx->inStack[0]);
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





const EXP_EvalTypeDesc* EXP_evalTypeDescById(EXP_EvalTypeContext* ctx, u32 typeId)
{
    return upoolElmData(ctx->dataPool, typeId);
}






u32 EXP_evalTypeAtom(EXP_EvalTypeContext* ctx, u32 atype)
{
    EXP_EvalTypeDesc* desc = EXP_evalTypeDescBufferReset(ctx, sizeof(EXP_EvalTypeDesc));
    desc->type = EXP_EvalTypeType_Atom;
    desc->atype = atype;
    return EXP_evalTypeId(ctx);
}

u32 EXP_evalTypeList(EXP_EvalTypeContext* ctx, const u32* elms, u32 count)
{
    u32 size = offsetof(EXP_EvalTypeDesc, list.elms) + sizeof(elms[0])*count;
    EXP_EvalTypeDesc* desc = EXP_evalTypeDescBufferReset(ctx, size);
    desc->type = EXP_EvalTypeType_List;
    desc->list.count = count;
    memcpy(desc->list.elms, elms, sizeof(elms[0])*count);
    for (u32 i = 0; i < count; ++i)
    {
        const EXP_EvalTypeDesc* elmDesc = EXP_evalTypeDescById(ctx, elms[i]);
        if ((EXP_EvalTypeType_List == elmDesc->type) || (EXP_EvalTypeType_ListVar == elmDesc->type))
        {
            desc->list.hasListElm = true;
            break;
        }
    }
    return EXP_evalTypeId(ctx);
}

u32 EXP_evalTypeVar(EXP_EvalTypeContext* ctx, u32 varId)
{
    EXP_EvalTypeDesc* desc = EXP_evalTypeDescBufferReset(ctx, sizeof(EXP_EvalTypeDesc));
    desc->type = EXP_EvalTypeType_Var;
    desc->varId = varId;
    return EXP_evalTypeId(ctx);
}

u32 EXP_evalTypeListVar(EXP_EvalTypeContext* ctx, u32 varId)
{
    EXP_EvalTypeDesc* desc = EXP_evalTypeDescBufferReset(ctx, sizeof(EXP_EvalTypeDesc));
    desc->type = EXP_EvalTypeType_ListVar;
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


























u32 EXP_evalTypeListOrListVar(EXP_EvalTypeContext* ctx, const u32* elms, u32 count)
{
    const EXP_EvalTypeDesc* elm0desc = NULL;
    if (1 == count)
    {
        elm0desc = EXP_evalTypeDescById(ctx, elms[0]);
        assert(elm0desc->type != EXP_EvalTypeType_List);
    }
    if ((1 == count) && (EXP_EvalTypeType_ListVar == elm0desc->type))
    {
        return elms[0];
    }
    else
    {
        return EXP_evalTypeList(ctx, elms, count);
    }
}




























u32 EXP_evalTypeReduct(EXP_EvalTypeContext* ctx, EXP_EvalTypeVarSpace* varSpace, u32 x)
{
    vec_u32* inStack = &ctx->inStack[0];
    EXP_EvalTypeReduceStack* reduceStack = &ctx->reduceStack;
    vec_u32* retBuf = &ctx->retBuf;

    const u32 inBP = inStack->length;
    const u32 reduceBP = reduceStack->length;
    const u32 retBP = retBuf->length;
    vec_push(inStack, x);

    u32 cur = -1;
    const EXP_EvalTypeReduce* topReduce = NULL;
    const EXP_EvalTypeDesc* desc = NULL;
step:
    assert(inStack->length >= inBP);
    if (inBP == inStack->length)
    {
        assert(reduceBP == reduceStack->length);
        assert(1 == retBuf->length - retBP);
        u32 t = retBuf->data[retBP];
        vec_pop(retBuf);
        return t;
    }
    cur = vec_last(inStack);
    vec_pop(inStack);
    if (EXP_EvalTypeInStackCmd_Reduce == cur)
    {
        assert(reduceStack->length > reduceBP);
        topReduce = &vec_last(reduceStack);
        switch (topReduce->typeType)
        {
        case EXP_EvalTypeType_List:
        {
            assert(retBuf->length >= topReduce->retBase);
            u32* elms = retBuf->data + topReduce->retBase;
            u32 count = retBuf->length - topReduce->retBase;
            u32 t = EXP_evalTypeListOrListVar(ctx, elms, count);
            vec_resize(retBuf, topReduce->retBase);
            vec_push(retBuf, t);
            break;
        }
        case EXP_EvalTypeType_Fun:
        {
            assert(retBuf->length == topReduce->retBase + 2);
            u32 t = EXP_evalTypeFun(ctx, retBuf->data[topReduce->retBase], retBuf->data[topReduce->retBase + 1]);
            vec_resize(retBuf, topReduce->retBase);
            vec_push(retBuf, t);
            break;
        }
        case EXP_EvalTypeType_Array:
        {
            assert(retBuf->length == topReduce->retBase + 1);
            u32 t = EXP_evalTypeArray(ctx, retBuf->data[topReduce->retBase]);
            vec_resize(retBuf, topReduce->retBase);
            vec_push(retBuf, t);
            break;
        }
        default:
            assert(false);
            break;
        }
        vec_pop(reduceStack);
    }
    else
    {
        desc = EXP_evalTypeDescById(ctx, cur);
        switch (desc->type)
        {
        case EXP_EvalTypeType_Atom:
        {
            vec_push(retBuf, cur);
            break;
        }
        case EXP_EvalTypeType_Var:
        case EXP_EvalTypeType_ListVar:
        {
            u32* pV = EXP_evalTypeVarValue(varSpace, desc->varId);
            if (pV)
            {
                vec_push(inStack, *pV);
            }
            else
            {
                vec_push(retBuf, cur);
            }
            break;
        }
        case EXP_EvalTypeType_List:
        {
            topReduce = (reduceStack->length > reduceBP) ? &vec_last(reduceStack) : NULL;
            bool listInList = topReduce && (EXP_EvalTypeType_List == topReduce->typeType);
            if (!listInList)
            {
                EXP_EvalTypeReduce newReduce = { EXP_EvalTypeType_List, retBuf->length };
                vec_push(reduceStack, newReduce);
                vec_push(inStack, EXP_EvalTypeInStackCmd_Reduce);
            }

            const EXP_EvalTypeDescList* list = &desc->list;
            for (u32 i = 0; i < list->count; ++i)
            {
                u32 e = list->elms[list->count - 1 - i];
                vec_push(inStack, e);
            }
            break;
        }
        case EXP_EvalTypeType_Fun:
        {
            EXP_EvalTypeReduce newReduce = { EXP_EvalTypeType_Fun, retBuf->length };
            vec_push(reduceStack, newReduce);
            vec_push(inStack, EXP_EvalTypeInStackCmd_Reduce);

            const EXP_EvalTypeDescFun* fun = &desc->fun;
            vec_push(inStack, fun->outs);
            vec_push(inStack, fun->ins);
            break;
        }
        case EXP_EvalTypeType_Array:
        {
            EXP_EvalTypeReduce newReduce = { EXP_EvalTypeType_Array, retBuf->length };
            vec_push(reduceStack, newReduce);
            vec_push(inStack, EXP_EvalTypeInStackCmd_Reduce);

            const EXP_EvalTypeDescArray* ary = &desc->ary;
            vec_push(inStack, ary->elm);
            break;
        }
        default:
            assert(false);
            break;
        }
    }
    goto step;
}


















bool EXP_evalTypeUnify(EXP_EvalTypeContext* ctx, EXP_EvalTypeVarSpace* varSpace, u32 a, u32 b, u32* pU)
{
    vec_u32* inStack0 = &ctx->inStack[0];
    vec_u32* inStack1 = &ctx->inStack[1];
    EXP_EvalTypeReduceStack* reduceStack = &ctx->reduceStack;
    vec_u32* retBuf = &ctx->retBuf;

    assert(inStack0->length == inStack1->length);
    const u32 inBP = inStack0->length;
    const u32 reduceBP = reduceStack->length;
    const u32 retBP = retBuf->length;
    vec_push(inStack0, a);
    vec_push(inStack1, b);

    const EXP_EvalTypeReduce* topReduce = NULL;
    const EXP_EvalTypeDesc* desc0 = NULL;
    const EXP_EvalTypeDesc* desc1 = NULL;
step:
    assert(inStack0->length >= inBP);
    assert(inStack1->length >= inBP);
    if ((inBP == inStack0->length) && (inBP == inStack1->length))
    {
        assert(reduceBP == reduceStack->length);
        assert(1 == retBuf->length - retBP);
        u32 t = retBuf->data[retBP];
        vec_pop(retBuf);
        *pU = EXP_evalTypeReduct(ctx, varSpace, t);
        return true;
    }
    if ((inBP == inStack0->length) || (inBP == inStack1->length))
    {
        goto failed;
    }

    a = vec_last(inStack0);
    b = vec_last(inStack1);

    if ((EXP_EvalTypeInStackCmd_Reduce == a) || (EXP_EvalTypeInStackCmd_Reduce == b))
    {
        vec_pop(inStack0);
        vec_pop(inStack1);
        if ((EXP_EvalTypeInStackCmd_Reduce != a) || (EXP_EvalTypeInStackCmd_Reduce != b))
        {
            goto failed;
        }
        assert(reduceStack->length > reduceBP);
        topReduce = &vec_last(reduceStack);
        switch (topReduce->typeType)
        {
        case EXP_EvalTypeType_List:
        {
            assert(retBuf->length >= topReduce->retBase);
            u32* elms = retBuf->data + topReduce->retBase;
            u32 count = retBuf->length - topReduce->retBase;
            u32 t = EXP_evalTypeListOrListVar(ctx, elms, count);
            vec_resize(retBuf, topReduce->retBase);
            vec_push(retBuf, t);
            break;
        }
        case EXP_EvalTypeType_Fun:
        {
            assert(retBuf->length == topReduce->retBase + 2);
            u32 t = EXP_evalTypeFun(ctx, retBuf->data[topReduce->retBase], retBuf->data[topReduce->retBase + 1]);
            vec_resize(retBuf, topReduce->retBase);
            vec_push(retBuf, t);
            break;
        }
        case EXP_EvalTypeType_Array:
        {
            assert(retBuf->length == topReduce->retBase + 1);
            u32 t = EXP_evalTypeArray(ctx, retBuf->data[topReduce->retBase]);
            vec_resize(retBuf, topReduce->retBase);
            vec_push(retBuf, t);
            break;
        }
        default:
            assert(false);
            break;
        }
        vec_pop(reduceStack);
    }
    else
    {
        desc0 = EXP_evalTypeDescById(ctx, a);
        desc1 = EXP_evalTypeDescById(ctx, b);

        topReduce = (reduceStack->length > reduceBP) ? &vec_last(reduceStack) : NULL;
        bool listInList = topReduce && (EXP_EvalTypeType_List == topReduce->typeType);
        if (listInList)
        {
            if (EXP_EvalTypeType_ListVar == desc0->type)
            {
                a = EXP_evalTypeReduct(ctx, varSpace, a);
                desc0 = EXP_evalTypeDescById(ctx, a);
            }
            if (EXP_EvalTypeType_ListVar == desc1->type)
            {
                b = EXP_evalTypeReduct(ctx, varSpace, b);
                desc1 = EXP_evalTypeDescById(ctx, b);
            }

            if (EXP_EvalTypeType_List == desc0->type)
            {
                vec_pop(inStack0);
                const EXP_EvalTypeDescList* list0 = &desc0->list;
                for (u32 i = 0; i < list0->count; ++i)
                {
                    u32 e = list0->elms[list0->count - 1 - i];
                    vec_push(inStack0, e);
                }
                goto step;
            }
            if (EXP_EvalTypeType_List == desc1->type)
            {
                vec_pop(inStack1);
                const EXP_EvalTypeDescList* list1 = &desc1->list;
                for (u32 i = 0; i < list1->count; ++i)
                {
                    u32 e = list1->elms[list1->count - 1 - i];
                    vec_push(inStack1, e);
                }
                goto step;
            }
        }

        vec_pop(inStack0);
        vec_pop(inStack1);

        if (a == b)
        {
            u32 t = EXP_evalTypeReduct(ctx, varSpace, a);
            vec_push(retBuf, t);
            goto step;
        }
        if (desc0->type == desc1->type)
        {
            switch (desc0->type)
            {
            case EXP_EvalTypeType_Atom:
            {
                assert(desc0->atype != desc1->atype);
                goto failed;
            }
            case EXP_EvalTypeType_Var:
            case EXP_EvalTypeType_ListVar:
            {
                u32* aV = EXP_evalTypeVarValue(varSpace, desc0->varId);
                u32* bV = EXP_evalTypeVarValue(varSpace, desc1->varId);
                if (aV && bV)
                {
                    vec_push(inStack0, *aV);
                    vec_push(inStack1, *bV);
                }
                else if (aV && !bV)
                {
                    u32 t = EXP_evalTypeReduct(ctx, varSpace, *aV);
                    vec_push(retBuf, t);
                }
                else if (!aV && bV)
                {
                    u32 t = EXP_evalTypeReduct(ctx, varSpace, *bV);
                    vec_push(retBuf, t);
                }
                else
                {
                    assert(!aV && !bV);
                    vec_push(retBuf, a);
                    EXP_evalTypeVarBind(varSpace, desc0->varId, b);
                }
                break;
            }
            case EXP_EvalTypeType_List:
            {
                assert(!listInList);

                EXP_EvalTypeReduce newReduce = { EXP_EvalTypeType_List, retBuf->length };
                vec_push(reduceStack, newReduce);
                vec_push(inStack0, EXP_EvalTypeInStackCmd_Reduce);
                vec_push(inStack1, EXP_EvalTypeInStackCmd_Reduce);

                const EXP_EvalTypeDescList* list0 = &desc0->list;
                const EXP_EvalTypeDescList* list1 = &desc1->list;

                for (u32 i = 0; i < list0->count; ++i)
                {
                    u32 e = list0->elms[list0->count - 1 - i];
                    vec_push(inStack0, e);
                }
                for (u32 i = 0; i < list1->count; ++i)
                {
                    u32 e = list1->elms[list1->count - 1 - i];
                    vec_push(inStack1, e);
                }
                break;
            }
            case EXP_EvalTypeType_Fun:
            {
                EXP_EvalTypeReduce newReduce = { EXP_EvalTypeType_Fun, retBuf->length };
                vec_push(reduceStack, newReduce);
                vec_push(inStack0, EXP_EvalTypeInStackCmd_Reduce);
                vec_push(inStack1, EXP_EvalTypeInStackCmd_Reduce);

                const EXP_EvalTypeDescFun* fun0 = &desc0->fun;
                const EXP_EvalTypeDescFun* fun1 = &desc1->fun;
                vec_push(inStack0, fun0->outs);
                vec_push(inStack0, fun0->ins);
                vec_push(inStack1, fun1->outs);
                vec_push(inStack1, fun1->ins);
                break;
            }
            case EXP_EvalTypeType_Array:
            {
                EXP_EvalTypeReduce newReduce = { EXP_EvalTypeType_Array, retBuf->length };
                vec_push(reduceStack, newReduce);
                vec_push(inStack0, EXP_EvalTypeInStackCmd_Reduce);
                vec_push(inStack1, EXP_EvalTypeInStackCmd_Reduce);

                const EXP_EvalTypeDescArray* ary0 = &desc0->ary;
                const EXP_EvalTypeDescArray* ary1 = &desc1->ary;
                vec_push(inStack0, ary0->elm);
                vec_push(inStack1, ary1->elm);
                break;
            }
            default:
                assert(false);
                break;
            }
        }
        else
        {
            if (((EXP_EvalTypeType_Var == desc0->type) && (EXP_EvalTypeType_ListVar != desc1->type)) ||
                ((EXP_EvalTypeType_ListVar == desc0->type) && (EXP_EvalTypeType_Var != desc1->type)))
            {
                u32* pV = EXP_evalTypeVarValue(varSpace, desc0->varId);
                if (pV)
                {
                    vec_push(inStack0, *pV);
                    vec_push(inStack1, b);
                    goto step;
                }
                u32 t = EXP_evalTypeReduct(ctx, varSpace, b);
                EXP_evalTypeVarBind(varSpace, desc0->varId, t);
                vec_push(retBuf, t);
            }
            else
            if (((EXP_EvalTypeType_Var == desc1->type) && (EXP_EvalTypeType_ListVar != desc0->type)) ||
                ((EXP_EvalTypeType_ListVar == desc1->type) && (EXP_EvalTypeType_Var != desc0->type)))
            {
                u32* pV = EXP_evalTypeVarValue(varSpace, desc1->varId);
                if (pV)
                {
                    vec_push(inStack0, a);
                    vec_push(inStack1, *pV);
                    goto step;
                }
                u32 t = EXP_evalTypeReduct(ctx, varSpace, a);
                EXP_evalTypeVarBind(varSpace, desc1->varId, t);
                vec_push(retBuf, t);
            }
            else
            {
                goto failed;
            }
        }
    }
    goto step;
failed:
    vec_resize(inStack0, inBP);
    vec_resize(inStack1, inBP);
    vec_resize(reduceStack, reduceBP);
    vec_resize(retBuf, retBP);
    return false;
}


















u32 EXP_evalTypeFromPat
(
    EXP_EvalTypeContext* ctx, EXP_EvalTypeVarSpace* varSpace, EXP_EvalTypeVarSpace* varRenMap, u32 pat
)
{
    vec_u32* inStack = &ctx->inStack[0];
    EXP_EvalTypeReduceStack* reduceStack = &ctx->reduceStack;
    vec_u32* retBuf = &ctx->retBuf;

    const u32 inBP = inStack->length;
    const u32 reduceBP = reduceStack->length;
    const u32 retBP = retBuf->length;
    vec_push(inStack, pat);

    u32 cur = -1;
    const EXP_EvalTypeReduce* topReduce = NULL;
    const EXP_EvalTypeDesc* desc = NULL;
step:
    assert(inStack->length >= inBP);
    if (inBP == inStack->length)
    {
        assert(reduceBP == reduceStack->length);
        assert(1 == retBuf->length - retBP);
        u32 t = retBuf->data[retBP];
        vec_pop(retBuf);
        t = EXP_evalTypeReduct(ctx, varSpace, t);
        return t;
    }

    cur = vec_last(inStack);
    vec_pop(inStack);
    if (EXP_EvalTypeInStackCmd_Reduce == cur)
    {
        assert(reduceStack->length > reduceBP);
        topReduce = &vec_last(reduceStack);
        switch (topReduce->typeType)
        {
        case EXP_EvalTypeType_List:
        {
            assert(retBuf->length >= topReduce->retBase);
            u32* elms = retBuf->data + topReduce->retBase;
            u32 count = retBuf->length - topReduce->retBase;
            u32 t = EXP_evalTypeListOrListVar(ctx, elms, count);
            vec_resize(retBuf, topReduce->retBase);
            vec_push(retBuf, t);
            break;
        }
        case EXP_EvalTypeType_Fun:
        {
            assert(retBuf->length == topReduce->retBase + 2);
            u32 t = EXP_evalTypeFun(ctx, retBuf->data[topReduce->retBase], retBuf->data[topReduce->retBase + 1]);
            vec_resize(retBuf, topReduce->retBase);
            vec_push(retBuf, t);
            break;
        }
        case EXP_EvalTypeType_Array:
        {
            assert(retBuf->length == topReduce->retBase + 1);
            u32 t = EXP_evalTypeArray(ctx, retBuf->data[topReduce->retBase]);
            vec_resize(retBuf, topReduce->retBase);
            vec_push(retBuf, t);
            break;
        }
        default:
            assert(false);
            break;
        }
        vec_pop(reduceStack);
    }
    else
    {
        desc = EXP_evalTypeDescById(ctx, cur);
        switch (desc->type)
        {
        case EXP_EvalTypeType_Atom:
        {
            vec_push(retBuf, cur);
            break;
        }
        case EXP_EvalTypeType_Var:
        case EXP_EvalTypeType_ListVar:
        {
            u32* pV = EXP_evalTypeVarValue(varRenMap, desc->varId);
            if (pV)
            {
                vec_push(retBuf, *pV);
            }
            else
            {
                u32 varId = EXP_evalTypeNewVar(varSpace);
                u32 t;
                if (EXP_EvalTypeType_Var == desc->type)
                {
                    t = EXP_evalTypeVar(ctx, varId);
                }
                else
                {
                    t = EXP_evalTypeListVar(ctx, varId);
                }
                EXP_evalTypeVarBind(varRenMap, desc->varId, t);
                vec_push(retBuf, t);
            }
            break;
        }
        case EXP_EvalTypeType_List:
        {
            topReduce = (reduceStack->length > reduceBP) ? &vec_last(reduceStack) : NULL;
            bool listInList = topReduce && (EXP_EvalTypeType_List == topReduce->typeType);
            if (!listInList)
            {
                EXP_EvalTypeReduce newReduce = { EXP_EvalTypeType_List, retBuf->length };
                vec_push(reduceStack, newReduce);
                vec_push(inStack, EXP_EvalTypeInStackCmd_Reduce);
            }

            const EXP_EvalTypeDescList* list = &desc->list;
            for (u32 i = 0; i < list->count; ++i)
            {
                u32 e = list->elms[list->count - 1 - i];
                vec_push(inStack, e);
            }
            break;
        }
        case EXP_EvalTypeType_Fun:
        {
            EXP_EvalTypeReduce newReduce = { EXP_EvalTypeType_Fun, retBuf->length };
            vec_push(reduceStack, newReduce);
            vec_push(inStack, EXP_EvalTypeInStackCmd_Reduce);

            const EXP_EvalTypeDescFun* fun = &desc->fun;
            vec_push(inStack, fun->outs);
            vec_push(inStack, fun->ins);
            break;
        }
        case EXP_EvalTypeType_Array:
        {
            EXP_EvalTypeReduce newReduce = { EXP_EvalTypeType_Array, retBuf->length };
            vec_push(reduceStack, newReduce);
            vec_push(inStack, EXP_EvalTypeInStackCmd_Reduce);

            const EXP_EvalTypeDescArray* ary = &desc->ary;
            vec_push(inStack, ary->elm);
            break;
        }
        default:
            assert(false);
            break;
        }
    }
    goto step;
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






















































































































