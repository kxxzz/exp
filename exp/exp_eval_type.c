#include "exp_eval_a.h"
#include "exp_eval_type_a.h"




typedef enum EXP_EvalTypeBuildCmdType
{
    EXP_EvalTypeBuildCmdType_Source,
    EXP_EvalTypeBuildCmdType_Reduce,
} EXP_EvalTypeBuildCmdType;

typedef struct EXP_EvalTypeBuildCmd
{
    EXP_EvalTypeBuildCmdType type;
    union
    {
        u32 src;
        struct 
        {
            EXP_EvalTypeType reduceType;
            u32 baseOffset;
        };
    };
} EXP_EvalTypeBuildCmd;

static EXP_EvalTypeBuildCmd EXP_evalTypeBuildCmdSource(u32 src)
{
    EXP_EvalTypeBuildCmd a = { EXP_EvalTypeBuildCmdType_Source, src };
    return a;
}
static EXP_EvalTypeBuildCmd EXP_evalTypeBuildCmdReduce(EXP_EvalTypeType reduceType, u32 offset)
{
    assert
    (
        (EXP_EvalTypeType_List == reduceType) ||
        (EXP_EvalTypeType_Fun == reduceType) ||
        (EXP_EvalTypeType_Array == reduceType)
    );
    EXP_EvalTypeBuildCmd a = { EXP_EvalTypeBuildCmdType_Reduce, .reduceType = reduceType, .baseOffset = offset };
    return a;
}



typedef vec_t(EXP_EvalTypeBuildCmd) EXP_EvalTypeBuildStack;


typedef struct EXP_EvalTypeContext
{
    Upool* dataPool;
    vec_char descBuffer;
    EXP_EvalTypeBuildStack remain;
    EXP_EvalTypeBuildStack remain1;
    vec_u32 result;
} EXP_EvalTypeContext;


EXP_EvalTypeContext* EXP_newEvalTypeContext(void)
{
    EXP_EvalTypeContext* ctx = zalloc(sizeof(EXP_EvalTypeContext));
    ctx->dataPool = newUpool(256);
    return ctx;
}

void EXP_evalTypeContextFree(EXP_EvalTypeContext* ctx)
{
    vec_free(&ctx->result);
    vec_free(&ctx->remain1);
    vec_free(&ctx->remain);
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
    EXP_EvalTypeBuildStack* remain = &ctx->remain;
    vec_u32* result = &ctx->result;
    const u32 remainBP = remain->length;
    const u32 resultBP = result->length;
    EXP_EvalTypeBuildCmd root = EXP_evalTypeBuildCmdSource(x);
    vec_push(remain, root);
    EXP_EvalTypeBuildCmd cmd;
    EXP_EvalTypeBuildCmd* top = NULL;
    const EXP_EvalTypeDesc* desc = NULL;
step:
    assert(remain->length >= remainBP);
    if (remainBP == remain->length)
    {
        assert(1 == result->length - resultBP);
        return result->data[resultBP];
    }
    top = &vec_last(remain);
    if (EXP_EvalTypeBuildCmdType_Source == top->type)
    {
        desc = EXP_evalTypeDescById(ctx, top->src);
        switch (desc->type)
        {
        case EXP_EvalTypeType_Atom:
        {
            vec_push(result, top->src);
            vec_pop(remain);
            break;
        }
        case EXP_EvalTypeType_Var:
        case EXP_EvalTypeType_ListVar:
        {
            u32* pV = EXP_evalTypeVarValue(varSpace, desc->varId);
            if (pV)
            {
                vec_pop(remain);
                cmd = EXP_evalTypeBuildCmdSource(*pV);
                vec_push(remain, cmd);
            }
            else
            {
                vec_push(result, top->src);
                vec_pop(remain);
            }
            break;
        }
        case EXP_EvalTypeType_List:
        {
            vec_pop(remain);

            bool listInList = false;
            for (u32 i = 0; i < remain->length; ++i)
            {
                cmd = remain->data[remain->length - 1 - i];
                if (EXP_EvalTypeBuildCmdType_Reduce == cmd.type)
                {
                    if (EXP_EvalTypeType_List == cmd.reduceType)
                    {
                        listInList = true;
                    }
                    break;
                }
            }
            if (!listInList)
            {
                cmd = EXP_evalTypeBuildCmdReduce(EXP_EvalTypeType_List, result->length);
                vec_push(remain, cmd);
            }

            const EXP_EvalTypeDescList* list = &desc->list;
            for (u32 i = 0; i < list->count; ++i)
            {
                u32 e = list->elms[list->count - 1 - i];
                cmd = EXP_evalTypeBuildCmdSource(e);
                vec_push(remain, cmd);
            }
            break;
        }
        case EXP_EvalTypeType_Fun:
        {
            vec_pop(remain);
            EXP_EvalTypeBuildCmd cmd = EXP_evalTypeBuildCmdReduce(EXP_EvalTypeType_Fun, result->length);
            vec_push(remain, cmd);

            const EXP_EvalTypeDescFun* fun = &desc->fun;
            cmd = EXP_evalTypeBuildCmdSource(fun->outs);
            vec_push(remain, cmd);
            cmd = EXP_evalTypeBuildCmdSource(fun->ins);
            vec_push(remain, cmd);
            break;
        }
        case EXP_EvalTypeType_Array:
        {
            vec_pop(remain);
            EXP_EvalTypeBuildCmd cmd = EXP_evalTypeBuildCmdReduce(EXP_EvalTypeType_Array, result->length);
            vec_push(remain, cmd);

            const EXP_EvalTypeDescArray* ary = &desc->ary;
            cmd = EXP_evalTypeBuildCmdSource(ary->elm);
            vec_push(remain, cmd);
            break;
        }
        default:
            assert(false);
            break;
        }
    }
    else
    {
        switch (top->reduceType)
        {
        case EXP_EvalTypeType_List:
        {
            assert(result->length >= top->baseOffset);
            u32 t = EXP_evalTypeList(ctx, result->data + top->baseOffset, result->length - top->baseOffset);
            vec_resize(result, top->baseOffset);
            vec_push(result, t);
            break;
        }
        case EXP_EvalTypeType_Fun:
        {
            assert(result->length == top->baseOffset + 2);
            u32 t = EXP_evalTypeFun(ctx, result->data[top->baseOffset], result->data[top->baseOffset + 1]);
            vec_resize(result, top->baseOffset);
            vec_push(result, t);
            break;
        }
        case EXP_EvalTypeType_Array:
        {
            assert(result->length == top->baseOffset + 1);
            u32 t = EXP_evalTypeArray(ctx, result->data[top->baseOffset]);
            vec_resize(result, top->baseOffset);
            vec_push(result, t);
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






















































































































