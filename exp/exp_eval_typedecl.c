#include "exp_eval_a.h"
#include "exp_eval_typedecl.h"







typedef struct EXP_EvalCompileTypeDeclReduce
{
    EXP_EvalTypeType typeType;
    u32 retBase;
} EXP_EvalCompileTypeDeclReduce;

typedef vec_t(EXP_EvalCompileTypeDeclReduce) EXP_EvalCompileTypeDeclReduceStack;




typedef struct EXP_EvalCompileTypeDeclVar
{
    EXP_Node name;
    u32 type;
} EXP_EvalCompileTypeDeclVar;

typedef vec_t(EXP_EvalCompileTypeDeclVar) EXP_EvalCompileTypeDeclVarStack;




typedef struct EXP_EvalCompileTypeDeclContext
{
    EXP_Space* space;
    EXP_EvalAtypeInfoVec* atypeTable;
    EXP_EvalTypeContext* typeContext;

    vec_u32 inStack;
    EXP_EvalCompileTypeDeclReduceStack reduceStack;
    EXP_EvalCompileTypeDeclVarStack varStack;
    vec_u32 retBuf;
} EXP_EvalCompileTypeDeclContext;




EXP_EvalCompileTypeDeclContext* EXP_evalCompileTypeDeclNewContext
(
    EXP_Space* space, EXP_EvalAtypeInfoVec* atypeTable, EXP_EvalTypeContext* typeContext
)
{
    EXP_EvalCompileTypeDeclContext* a = zalloc(sizeof(EXP_EvalCompileTypeDeclContext));
    a->space = space;
    a->atypeTable = atypeTable;
    a->typeContext = typeContext;
    return a;
}

void EXP_evalCompileTypeDeclContextFree(EXP_EvalCompileTypeDeclContext* ctx)
{
    vec_free(&ctx->retBuf);
    vec_free(&ctx->varStack);
    vec_free(&ctx->reduceStack);
    vec_free(&ctx->inStack);
    free(ctx);
}











static u32 EXP_evalCompileTypeDeclLoop
(
    EXP_EvalCompileTypeDeclContext* ctx,
    const EXP_SpaceSrcInfo* srcInfo,
    EXP_EvalError* outError
)
{
    EXP_Space* space = ctx->space;
    EXP_EvalAtypeInfoVec* atypeTable = ctx->atypeTable;
    EXP_EvalTypeContext* typeContext = ctx->typeContext;
    vec_u32* inStack = &ctx->inStack;
    EXP_EvalCompileTypeDeclReduceStack* reduceStack = &ctx->reduceStack;
    EXP_EvalCompileTypeDeclVarStack* varStack = &ctx->varStack;
    vec_u32* retBuf = &ctx->retBuf;

next:
    if (!typeDeclStack->length)
    {
        assert(r != -1);
        return r;
    }
    EXP_EvalCompileTypeDeclLevel* top = &vec_last(typeDeclStack);
    EXP_Node node = top->src;

    if (EXP_isTok(space, node))
    {
        const char* cstr = EXP_tokCstr(space, node);
        for (u32 i = 0; i + 1 < typeDeclStack->length; ++i)
        {
            EXP_EvalCompileTypeDeclLevel* fun = typeDeclStack->data + typeDeclStack->length - 2 - i;
            if (EXP_isSeqRound(space, fun->src) || EXP_isSeqNaked(space, fun->src))
            {
                for (u32 i = 0; i < fun->varSpace.length; ++i)
                {
                    EXP_EvalCompileTypeDeclVar* var = fun->varSpace.data + i;
                    if (EXP_nodeDataEq(space, node, var->name))
                    {
                        r = var->type;
                        EXP_evalCompileTypeDeclStackPop(typeDeclStack);
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
                EXP_evalCompileTypeDeclStackPop(typeDeclStack);
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
                    EXP_EvalCompileTypeDeclVar v = { name, t };
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
            EXP_EvalCompileTypeDeclLevel l = { elms[p] };
            vec_push(typeDeclStack, l);
        }
        else
        {
            u32 ins = EXP_evalTypeList(typeContext, top->elms.data, numIns);
            u32 outs = EXP_evalTypeList(typeContext, top->elms.data + numIns, numOuts);
            r = EXP_evalTypeFun(typeContext, ins, outs);
            EXP_evalCompileTypeDeclStackPop(typeDeclStack);
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
            EXP_EvalCompileTypeDeclLevel l = { elms[p] };
            vec_push(typeDeclStack, l);
        }
        else
        {
            u32 elm = EXP_evalTypeList(typeContext, top->elms.data, len);
            r = EXP_evalTypeArray(typeContext, elm);
            EXP_evalCompileTypeDeclStackPop(typeDeclStack);
        }
        goto next;
    }
    else
    {
        EXP_evalErrorFound(outError, srcInfo, EXP_EvalErrCode_EvalSyntax, node);
        return -1;
    }
}








u32 EXP_evalCompileTypeDecl
(
    EXP_EvalCompileTypeDeclContext* ctx, EXP_Node node, const EXP_SpaceSrcInfo* srcInfo, EXP_EvalError* outError
)
{
    EXP_Space* space = ctx->space;
    EXP_EvalAtypeInfoVec* atypeTable = ctx->atypeTable;
    EXP_EvalTypeContext* typeContext = ctx->typeContext;
    vec_u32* inStack = &ctx->inStack;
    EXP_EvalCompileTypeDeclReduceStack* reduceStack = &ctx->reduceStack;
    EXP_EvalCompileTypeDeclVarStack* varStack = &ctx->varStack;
    vec_u32* retBuf = &ctx->retBuf;

    if (!EXP_isSeqRound(space, node) && !EXP_isSeqNaked(space, node))
    {
        EXP_evalErrorFound(outError, srcInfo, EXP_EvalErrCode_EvalSyntax, node);
        return -1;
    }
    const EXP_Node* elms = EXP_seqElm(space, node);
    u32 len = EXP_seqLen(space, node);

    assert(0 == typeDeclStack->length);
    EXP_EvalCompileTypeDeclLevel l = { 0 };
    if (1 == len)
    {
        l.src = elms[0];
    }
    else
    {
        l.src = node;
    }
    vec_push(typeDeclStack, l);
    u32 t = EXP_evalCompileTypeDeclLoop(ctx, srcInfo, outError);
    return t;
}



















































































