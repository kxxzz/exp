#include "exp_eval_a.h"
#include "exp_eval_typedecl.h"







typedef struct EXP_EvalTypeDeclReduce
{
    EXP_EvalTypeType typeType;
    u32 varBase;
    u32 retBase;
    union
    {
        struct 
        {
            u32 numIns;
            u32 numOuts;
        };
    };
} EXP_EvalTypeDeclReduce;

typedef vec_t(EXP_EvalTypeDeclReduce) EXP_EvalTypeDeclReduceStack;




typedef struct EXP_EvalTypeDeclVar
{
    EXP_Node name;
    u32 type;
} EXP_EvalTypeDeclVar;

typedef vec_t(EXP_EvalTypeDeclVar) EXP_EvalTypeDeclVarStack;




typedef struct EXP_EvalTypeDeclContext
{
    EXP_Space* space;
    EXP_EvalAtypeInfoVec* atypeTable;
    EXP_EvalTypeContext* typeContext;

    EXP_NodeVec inStack;
    EXP_EvalTypeDeclReduceStack reduceStack;
    EXP_EvalTypeDeclVarStack varStack;
    vec_u32 retBuf;
} EXP_EvalTypeDeclContext;




EXP_EvalTypeDeclContext* EXP_newEvalTypeDeclContext
(
    EXP_Space* space, EXP_EvalAtypeInfoVec* atypeTable, EXP_EvalTypeContext* typeContext
)
{
    EXP_EvalTypeDeclContext* a = zalloc(sizeof(EXP_EvalTypeDeclContext));
    a->space = space;
    a->atypeTable = atypeTable;
    a->typeContext = typeContext;
    return a;
}

void EXP_evalTypeDeclContextFree(EXP_EvalTypeDeclContext* ctx)
{
    vec_free(&ctx->retBuf);
    vec_free(&ctx->varStack);
    vec_free(&ctx->reduceStack);
    vec_free(&ctx->inStack);
    free(ctx);
}











static u32 EXP_evalTypeDeclLoop
(
    EXP_EvalTypeDeclContext* ctx,
    const EXP_SpaceSrcInfo* srcInfo,
    EXP_EvalError* outError
)
{
    EXP_Space* space = ctx->space;
    EXP_EvalAtypeInfoVec* atypeTable = ctx->atypeTable;
    EXP_EvalTypeContext* typeContext = ctx->typeContext;
    EXP_NodeVec* inStack = &ctx->inStack;
    EXP_EvalTypeDeclReduceStack* reduceStack = &ctx->reduceStack;
    EXP_EvalTypeDeclVarStack* varStack = &ctx->varStack;
    vec_u32* retBuf = &ctx->retBuf;

    EXP_Node cur;
    const EXP_EvalTypeDeclReduce* topReduce = NULL;
next:
    if (!inStack->length)
    {
        assert(0 == reduceStack->length);
        assert(1 == retBuf->length);
        u32 t = retBuf->data[0];
        vec_pop(retBuf);
        return t;
    }
    cur = vec_last(inStack);
    vec_pop(inStack);

    if (EXP_Node_Invalid.id == cur.id)
    {
        assert(reduceStack->length > 0);
        topReduce = &vec_last(reduceStack);

        switch (topReduce->typeType)
        {
        case EXP_EvalTypeType_Fun:
        {
            assert(retBuf->length == topReduce->retBase + topReduce->numIns + topReduce->numOuts);
            u32 funIns = EXP_evalTypeListOrListVar(typeContext, retBuf->data + topReduce->retBase, topReduce->numIns);
            u32 funOuts = EXP_evalTypeListOrListVar(typeContext, retBuf->data + topReduce->retBase + topReduce->numIns, topReduce->numOuts);
            u32 fun = EXP_evalTypeFun(typeContext, funIns, funOuts);
            vec_resize(retBuf, topReduce->retBase);
            vec_push(retBuf, fun);
            break;
        }
        case EXP_EvalTypeType_Array:
        {
            assert(retBuf->length >= topReduce->retBase);
            u32 elmLen = retBuf->length - topReduce->retBase;
            u32 elm = EXP_evalTypeListOrListVar(typeContext, retBuf->data + topReduce->retBase, elmLen);
            u32 ary = EXP_evalTypeArray(typeContext, elm);
            vec_resize(retBuf, topReduce->retBase);
            vec_push(retBuf, ary);
            break;
        }
        default:
            assert(false);
            break;
        }
        vec_resize(varStack, topReduce->varBase);
        vec_pop(reduceStack);
    }
    else if (EXP_isTok(space, cur))
    {
        const char* cstr = EXP_tokCstr(space, cur);
        for (u32 i = 0; i < varStack->length; ++i)
        {
            EXP_EvalTypeDeclVar* var = varStack->data + i;
            if (EXP_nodeDataEq(space, cur, var->name))
            {
                vec_push(retBuf, var->type);
                goto next;
            }
        }
        for (u32 i = 0; i < atypeTable->length; ++i)
        {
            if (0 == strcmp(cstr, atypeTable->data[i].name))
            {
                u32 t = EXP_evalTypeAtom(typeContext, i);
                vec_push(retBuf, t);
                goto next;
            }
        }
        EXP_evalErrorFound(outError, srcInfo, EXP_EvalErrCode_EvalSyntax, cur);
        return -1;
    }
    else if (EXP_isSeqRound(space, cur) || EXP_isSeqNaked(space, cur))
    {
        vec_push(inStack, EXP_Node_Invalid);
        EXP_EvalTypeDeclReduce newReduce = { EXP_EvalTypeType_Fun, varStack->length, retBuf->length };

        const EXP_Node* elms = EXP_seqElm(space, cur);
        u32 len = EXP_seqLen(space, cur);
        if (0 == len)
        {
            EXP_evalErrorFound(outError, srcInfo, EXP_EvalErrCode_EvalSyntax, cur);
            return -1;
        }
        u32 elmsOffset = EXP_isSeqCurly(space, elms[0]) ? 1 : 0;

        if (EXP_isSeqCurly(space, elms[0]))
        {
            const EXP_Node* names = EXP_seqElm(space, elms[0]);
            u32 len = EXP_seqLen(space, elms[0]);
            for (u32 i = 0; i < len; ++i)
            {
                EXP_Node name = names[i];
                if (!EXP_isTok(space, name))
                {
                    EXP_evalErrorFound(outError, srcInfo, EXP_EvalErrCode_EvalSyntax, cur);
                    return -1;
                }
                for (u32 i = 0; i < varStack->length; ++i)
                {
                    EXP_Node name0 = varStack->data[i].name;
                    if (EXP_Node_Invalid.id == name0.id)
                    {
                        break;
                    }
                    if (EXP_nodeDataEq(space, name0, name))
                    {
                        EXP_evalErrorFound(outError, srcInfo, EXP_EvalErrCode_EvalSyntax, cur);
                        return -1;
                    }
                }
                    
                const char* cstr = EXP_tokCstr(space, name);
                u32 cstrLen = EXP_tokSize(space, name);
                u32 t;
                if ('*' == cstr[cstrLen - 1])
                {
                    t = EXP_evalTypeListVar(typeContext, i);
                }
                else
                {
                    t = EXP_evalTypeVar(typeContext, i);
                }
                EXP_EvalTypeDeclVar v = { name, t };
                vec_push(varStack, v);
            }
        }

        u32 arrowPos = -1;
        for (u32 i = elmsOffset; i < len; ++i)
        {
            EXP_Node e = elms[i];
            if (EXP_isTok(space, e))
            {
                const char* cstr = EXP_tokCstr(space, e);
                u32 cstrLen = EXP_tokSize(space, e);
                if (0 == strcmp("->", cstr))
                {
                    if (arrowPos != -1)
                    {
                        EXP_evalErrorFound(outError, srcInfo, EXP_EvalErrCode_EvalSyntax, cur);
                        return -1;
                    }
                    arrowPos = i;
                    continue;
                }
            }
            if (-1 == arrowPos)
            {
                ++newReduce.numIns;
            }
            else
            {
                ++newReduce.numOuts;
            }
        }
        if (-1 == arrowPos)
        {
            EXP_evalErrorFound(outError, srcInfo, EXP_EvalErrCode_EvalSyntax, cur);
            return -1;
        }
        assert(elmsOffset + newReduce.numIns == arrowPos);

        vec_push(reduceStack, newReduce);
        for (u32 i = 0; i < len - elmsOffset; ++i)
        {
            const u32 p = len - 1 - i;
            if (p == arrowPos) continue;
            EXP_Node e = elms[p];
            vec_push(inStack, e);
        }
    }
    else if (EXP_isSeqSquare(space, cur))
    {
        vec_push(inStack, EXP_Node_Invalid);
        EXP_EvalTypeDeclReduce newReduce = { EXP_EvalTypeType_Array, varStack->length, retBuf->length };
        vec_push(reduceStack, newReduce);

        const EXP_Node* elms = EXP_seqElm(space, cur);
        u32 len = EXP_seqLen(space, cur);
        if (0 == len)
        {
            EXP_evalErrorFound(outError, srcInfo, EXP_EvalErrCode_EvalSyntax, cur);
            return -1;
        }

        for (u32 i = 0; i < len; ++i)
        {
            EXP_Node e = elms[len - 1 - i];
            vec_push(inStack, e);
        }
    }
    else
    {
        EXP_evalErrorFound(outError, srcInfo, EXP_EvalErrCode_EvalSyntax, cur);
        return -1;
    }
    goto next;
}








u32 EXP_evalTypeDecl
(
    EXP_EvalTypeDeclContext* ctx, EXP_Node node, const EXP_SpaceSrcInfo* srcInfo, EXP_EvalError* outError
)
{
    EXP_Space* space = ctx->space;
    EXP_EvalAtypeInfoVec* atypeTable = ctx->atypeTable;
    EXP_EvalTypeContext* typeContext = ctx->typeContext;
    EXP_NodeVec* inStack = &ctx->inStack;
    EXP_EvalTypeDeclReduceStack* reduceStack = &ctx->reduceStack;
    EXP_EvalTypeDeclVarStack* varStack = &ctx->varStack;
    vec_u32* retBuf = &ctx->retBuf;

    if (!EXP_isSeqRound(space, node) && !EXP_isSeqNaked(space, node))
    {
        EXP_evalErrorFound(outError, srcInfo, EXP_EvalErrCode_EvalSyntax, node);
        return -1;
    }
    const EXP_Node* elms = EXP_seqElm(space, node);
    u32 len = EXP_seqLen(space, node);

    assert(0 == inStack->length);
    EXP_Node root;
    if (1 == len)
    {
        root = elms[0];
    }
    else
    {
        root = node;
    }
    vec_push(inStack, root);
    u32 t = EXP_evalTypeDeclLoop(ctx, srcInfo, outError);
    return t;
}



















































































