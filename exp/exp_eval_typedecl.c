#include "exp_eval_a.h"
#include "exp_eval_typedecl.h"







typedef struct EXP_EvalTypeDeclReduce
{
    EXP_EvalTypeType typeType;
    u32 retBase;
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




EXP_EvalTypeDeclContext* EXP_evalTypeDeclNewContext
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

next:
    if (!inStack->length)
    {
        assert(r != -1);
        return r;
    }
    EXP_EvalTypeDeclLevel* top = &vec_last(inStack);
    EXP_Node node = top->src;

    if (EXP_isTok(space, node))
    {
        const char* cstr = EXP_tokCstr(space, node);
        for (u32 i = 0; i + 1 < inStack->length; ++i)
        {
            EXP_EvalTypeDeclLevel* fun = inStack->data + inStack->length - 2 - i;
            if (EXP_isSeqRound(space, fun->src) || EXP_isSeqNaked(space, fun->src))
            {
                for (u32 i = 0; i < fun->varSpace.length; ++i)
                {
                    EXP_EvalTypeDeclVar* var = fun->varSpace.data + i;
                    if (EXP_nodeDataEq(space, node, var->name))
                    {
                        r = var->type;
                        EXP_evalTypeDeclStackPop(inStack);
                        goto next;
                    }
                }
            }
        }
        for (u32 i = 0; i < atypeTable->length; ++i)
        {
            if (0 == strcmp(cstr, atypeTable->data[i].name))
            {
                r = EXP_evalTypeAtom(typeContext, i);
                EXP_evalTypeDeclStackPop(inStack);
                goto next;
            }
        }
        EXP_evalErrorFound(outError, srcInfo, EXP_EvalErrCode_EvalSyntax, node);
        return -1;
    }
    else if (EXP_isSeqRound(space, node) || EXP_isSeqNaked(space, node))
    {
        const EXP_Node* elms = EXP_seqElm(space, node);
        u32 len = EXP_seqLen(space, node);
        if (0 == len)
        {
            EXP_evalErrorFound(outError, srcInfo, EXP_EvalErrCode_EvalSyntax, node);
            return -1;
        }
        u32 elmsOffset = EXP_isSeqCurly(space, elms[0]) ? 1 : 0;

        if (top->hasRet)
        {
            vec_push(&top->elms, r);
        }
        else
        {
            top->hasRet = true;

            if (EXP_isSeqCurly(space, elms[0]))
            {
                const EXP_Node* names = EXP_seqElm(space, elms[0]);
                u32 len = EXP_seqLen(space, elms[0]);
                for (u32 i = 0; i < len; ++i)
                {
                    EXP_Node name = names[i];
                    if (!EXP_isTok(space, name))
                    {
                        EXP_evalErrorFound(outError, srcInfo, EXP_EvalErrCode_EvalSyntax, node);
                        return -1;
                    }
                    for (u32 i = 0; i < top->varSpace.length; ++i)
                    {
                        EXP_Node name0 = top->varSpace.data[i].name;
                        if (EXP_nodeDataEq(space, name0, name))
                        {
                            EXP_evalErrorFound(outError, srcInfo, EXP_EvalErrCode_EvalSyntax, node);
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
                    vec_push(&top->varSpace, v);
                }
            }

            u32 arrowPos = -1;
            for (u32 i = elmsOffset; i < len; ++i)
            {
                if (EXP_isTok(space, elms[i]))
                {
                    const char* cstr = EXP_tokCstr(space, elms[i]);
                    u32 cstrLen = EXP_tokSize(space, elms[i]);
                    if (0 == strcmp("->", cstr))
                    {
                        if (arrowPos != -1)
                        {
                            EXP_evalErrorFound(outError, srcInfo, EXP_EvalErrCode_EvalSyntax, node);
                            return -1;
                        }
                        arrowPos = i;
                        continue;
                    }
                }
                if (-1 == arrowPos)
                {
                    ++top->fun.numIns;
                }
                else
                {
                    ++top->fun.numOuts;
                }
            }
            if (-1 == arrowPos)
            {
                EXP_evalErrorFound(outError, srcInfo, EXP_EvalErrCode_EvalSyntax, node);
                return -1;
            }
            assert(elmsOffset + top->fun.numIns == arrowPos);
        }

        u32 p = elmsOffset + top->elms.length;
        u32 numIns = top->fun.numIns;
        u32 numOuts = top->fun.numOuts;
        u32 numAll = numIns + numOuts;
        if (p >= elmsOffset + numIns)
        {
            p += 1;
        }
        if (p < elmsOffset + 1 + numAll)
        {
            EXP_EvalTypeDeclLevel l = { elms[p] };
            vec_push(inStack, l);
        }
        else
        {
            u32 ins = EXP_evalTypeList(typeContext, top->elms.data, numIns);
            u32 outs = EXP_evalTypeList(typeContext, top->elms.data + numIns, numOuts);
            r = EXP_evalTypeFun(typeContext, ins, outs);
            EXP_evalTypeDeclStackPop(inStack);
        }
        goto next;
    }
    else if (EXP_isSeqSquare(space, node))
    {
        const EXP_Node* elms = EXP_seqElm(space, node);
        u32 len = EXP_seqLen(space, node);
        if (0 == len)
        {
            EXP_evalErrorFound(outError, srcInfo, EXP_EvalErrCode_EvalSyntax, node);
            return -1;
        }

        if (top->hasRet)
        {
            vec_push(&top->elms, r);
        }
        else
        {
            top->hasRet = true;
        }
        u32 p = top->elms.length;
        if (p < len)
        {
            EXP_EvalTypeDeclLevel l = { elms[p] };
            vec_push(inStack, l);
        }
        else
        {
            u32 elm = EXP_evalTypeList(typeContext, top->elms.data, len);
            r = EXP_evalTypeArray(typeContext, elm);
            EXP_evalTypeDeclStackPop(inStack);
        }
        goto next;
    }
    else
    {
        EXP_evalErrorFound(outError, srcInfo, EXP_EvalErrCode_EvalSyntax, node);
        return -1;
    }
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



















































































