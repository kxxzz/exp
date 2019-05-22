#include "exp_eval_a.h"
#include "exp_eval_type_a.h"




typedef struct EXP_EvalTypeReduce
{
    EXP_EvalTypeType typeType;
    u32 inBase;
    u32 retBase;
} EXP_EvalTypeReduce;

typedef vec_t(EXP_EvalTypeReduce) EXP_EvalTypeReduceStack;


typedef struct EXP_EvalTypeContext
{
    Upool* dataPool;
    vec_char descBuffer;
    vec_u32 inStack[2];
    EXP_EvalTypeReduceStack reduceStack[2];
    vec_u32 retBuf;
} EXP_EvalTypeContext;


EXP_EvalTypeContext* EXP_newEvalTypeContext(void)
{
    EXP_EvalTypeContext* ctx = zalloc(sizeof(EXP_EvalTypeContext));
    ctx->dataPool = newUpool(256);
    return ctx;
}

void EXP_evalTypeContextFree(EXP_EvalTypeContext* ctx)
{
    vec_free(&ctx->retBuf);
    vec_free(&ctx->reduceStack[1]);
    vec_free(&ctx->reduceStack[0]);
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



















u32 EXP_evalTypeReduct(EXP_EvalTypeContext* ctx, EXP_EvalTypeVarSpace* varSpace, u32 x)
{
    vec_u32* inStack = &ctx->inStack[0];
    EXP_EvalTypeReduceStack* reduceStack = &ctx->reduceStack[0];
    vec_u32* retBuf = &ctx->retBuf;
    const u32 inBP = inStack->length;
    const u32 retBP = retBuf->length;
    vec_push(inStack, x);
    u32 cur = -1;
    const EXP_EvalTypeReduce* topReduce = NULL;
    const EXP_EvalTypeDesc* desc = NULL;
step:
    assert(inStack->length >= inBP);
    if (inBP == inStack->length)
    {
        assert(1 == retBuf->length - retBP);
        return retBuf->data[retBP];
    }
    cur = vec_last(inStack);
    topReduce = (reduceStack->length > 0) ? &vec_last(reduceStack) : NULL;
    if (topReduce && (topReduce->inBase == inStack->length))
    {
        switch (topReduce->typeType)
        {
        case EXP_EvalTypeType_List:
        {
            assert(retBuf->length >= topReduce->retBase);
            u32 t = EXP_evalTypeList(ctx, retBuf->data + topReduce->retBase, retBuf->length - topReduce->retBase);
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
    }
    else
    {
        desc = EXP_evalTypeDescById(ctx, cur);
        switch (desc->type)
        {
        case EXP_EvalTypeType_Atom:
        {
            vec_push(retBuf, cur);
            vec_pop(inStack);
            break;
        }
        case EXP_EvalTypeType_Var:
        case EXP_EvalTypeType_ListVar:
        {
            u32* pV = EXP_evalTypeVarValue(varSpace, desc->varId);
            if (pV)
            {
                vec_pop(inStack);
                vec_push(inStack, *pV);
            }
            else
            {
                vec_push(retBuf, cur);
                vec_pop(inStack);
            }
            break;
        }
        case EXP_EvalTypeType_List:
        {
            vec_pop(inStack);

            bool listInList = topReduce && (EXP_EvalTypeType_List == topReduce->typeType);
            if (!listInList)
            {
                EXP_EvalTypeReduce newReduce = { EXP_EvalTypeType_List, retBuf->length };
                vec_push(reduceStack, newReduce);
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
            vec_pop(inStack);
            EXP_EvalTypeReduce newReduce = { EXP_EvalTypeType_Fun, retBuf->length };
            vec_push(reduceStack, newReduce);

            const EXP_EvalTypeDescFun* fun = &desc->fun;
            vec_push(inStack, fun->outs);
            vec_push(inStack, fun->ins);
            break;
        }
        case EXP_EvalTypeType_Array:
        {
            vec_pop(inStack);
            EXP_EvalTypeReduce newReduce = { EXP_EvalTypeType_Array, retBuf->length };
            vec_push(reduceStack, newReduce);

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
    u32 r = -1;
    return r;
}


















u32 EXP_evalTypeFromPat
(
    EXP_EvalTypeContext* ctx, EXP_EvalTypeVarSpace* varSpace, EXP_EvalTypeVarSpace* varRenMap, u32 pat
)
{
    u32 r = -1;
    return r;
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






















































































































