#include "exp_eval_a.h"



typedef vec_t(EXP_EvalValue) EXP_EvalDataStack;


typedef struct EXP_EvalDef
{
    EXP_Node key;
    EXP_Node val;
    bool hasRtVal;
    EXP_EvalValue rtVal;
} EXP_EvalDef;

typedef vec_t(EXP_EvalDef) EXP_EvalDefStack;


typedef struct EXP_EvalBlock
{
    EXP_EvalAtomFun fun;
    u32 defStackP;
    EXP_Node* seq;
    u32 len;
    u32 p;
} EXP_EvalBlock;

typedef vec_t(EXP_EvalBlock) EXP_EvalBlockStack;



typedef struct EXP_EvalContext
{
    EXP_Space* space;
    EXP_NodeSrcInfoTable* srcInfoTable;
    EXP_EvalRet ret;
    bool hasHalt;
    EXP_EvalDefStack defStack;
    EXP_EvalBlockStack blockStack;
    EXP_EvalDataStack dataStack;
} EXP_EvalContext;


static void EXP_evalContextFree(EXP_EvalContext* ctx)
{
    vec_free(&ctx->dataStack);
    vec_free(&ctx->blockStack);
    vec_free(&ctx->defStack);
}




static EXP_EvalPrimType EXP_evalGetPrimFunType(EXP_Space* space, const char* funName)
{
    for (u32 i = 0; i < EXP_NumEvalPrimTypes; ++i)
    {
        if (0 == strcmp(funName, EXP_EvalPrimFunTypeNameTable[i]))
        {
            return i;
        }
    }
    return -1;
}

static EXP_EvalAtomFun EXP_EvalPrimAtomFunTable[EXP_NumEvalPrimTypes];









static bool EXP_evalCheckCall(EXP_Space* space, EXP_Node node)
{
    if (!EXP_isSeq(space, node))
    {
        return false;
    }
    u32 len = EXP_seqLen(space, node);
    if (!len)
    {
        return false;
    }
    EXP_Node* elms = EXP_seqElm(space, node);
    if (!EXP_isTok(space, elms[0]))
    {
        return false;
    }
    return true;
}

static bool EXP_evalCheckDefPat(EXP_Space* space, EXP_Node node)
{
    if (!EXP_isSeq(space, node))
    {
        return false;
    }
    u32 len = EXP_seqLen(space, node);
    if (!len)
    {
        return false;
    }
    EXP_Node* elms = EXP_seqElm(space, node);
    if (!EXP_isTok(space, elms[0]))
    {
        return false;
    }
    return true;
}




static void EXP_evalSyntaxErrorAtNode(EXP_EvalContext* ctx, EXP_Node node)
{
    ctx->hasHalt = true;
    EXP_NodeSrcInfoTable* srcInfoTable = ctx->srcInfoTable;
    if (srcInfoTable)
    {
        assert(node.id < srcInfoTable->length);
        ctx->ret.errCode = EXP_EvalErrCode_EvalSyntax;
        ctx->ret.errSrcFile = NULL;// todo
        ctx->ret.errSrcFileLine = srcInfoTable->data[node.id].line;
        ctx->ret.errSrcFileColumn = srcInfoTable->data[node.id].column;
    }
}





static void EXP_evalLoadDef(EXP_EvalContext* ctx, EXP_Node node)
{
    EXP_Space* space = ctx->space;
    if (EXP_isTok(space, node))
    {
        return;
    }
    if (!EXP_evalCheckCall(space, node))
    {
        EXP_evalSyntaxErrorAtNode(ctx, node);
        return;
    }
    EXP_Node* defCall = EXP_seqElm(space, node);
    const char* kDef = EXP_tokCstr(space, defCall[0]);
    EXP_EvalPrimType primType = EXP_evalGetPrimFunType(space, kDef);
    if (primType != EXP_EvalPrimType_Def)
    {
        return;
    }
    EXP_Node name;
    if (EXP_isTok(space, defCall[1]))
    {
        name = defCall[1];
    }
    else if (EXP_evalCheckDefPat(space, defCall[1]))
    {
        EXP_Node* pat = EXP_seqElm(space, defCall[1]);
        name = pat[0];
    }
    else
    {
        EXP_evalSyntaxErrorAtNode(ctx, defCall[1]);
        return;
    }
    EXP_EvalDef def = { name, node };
    vec_push(&ctx->defStack, def);
}




static void EXP_evalDefGetParms(EXP_EvalContext* ctx, EXP_Node node, u32* pNumParms, EXP_Node** pParms)
{
    EXP_Space* space = ctx->space;
    EXP_Node* defCall = EXP_seqElm(space, node);
    if (EXP_isTok(space, defCall[1]))
    {
        *pNumParms = 0;
        *pParms = NULL;
    }
    else
    {
        assert(EXP_evalCheckDefPat(space, defCall[1]));
        EXP_Node* pat = EXP_seqElm(space, defCall[1]);
        *pNumParms = EXP_seqLen(space, defCall[1]) - 1;
        *pParms = pat;
    }
}


static void EXP_evalDefGetBody(EXP_EvalContext* ctx, EXP_Node node, u32* pLen, EXP_Node** pSeq)
{
    EXP_Space* space = ctx->space;
    assert(EXP_seqLen(space, node) >= 2);
    *pLen = EXP_seqLen(space, node) - 2;
    EXP_Node* defCall = EXP_seqElm(space, node);
    *pSeq = defCall + 2;
}











static EXP_EvalDef* EXP_evalGetMatched(EXP_EvalContext* ctx, const char* funName)
{
    EXP_Space* space = ctx->space;
    for (u32 i = 0; i < ctx->defStack.length; ++i)
    {
        EXP_EvalDef* def = ctx->defStack.data + ctx->defStack.length - 1 - i;
        const char* str = EXP_tokCstr(space, def->key);
        if (0 == strcmp(str, funName))
        {
            return def;
        }
    }
    return NULL;
}












static bool EXP_evalEnterBlock
(
    EXP_EvalContext* ctx, u32 len, EXP_Node* seq,
    u32 numParms, EXP_Node* parms, EXP_Node* args,
    EXP_EvalAtomFun fun
)
{
    u32 defStackP = ctx->defStack.length;
    for (u32 i = 0; i < numParms; ++i)
    {
        EXP_Node k = parms[i];
        EXP_Node v = args[i];
        EXP_EvalDef def = { k, v };
        vec_push(&ctx->defStack, def);
    }
    if (!fun)
    {
        for (u32 i = 0; i < len; ++i)
        {
            EXP_evalLoadDef(ctx, seq[i]);
            if (ctx->hasHalt)
            {
                return false;;
            }
        }
    }
    EXP_EvalBlock blk = { fun, defStackP, seq, len, 0 };
    vec_push(&ctx->blockStack, blk);
    return true;
}

static bool EXP_evalLeaveBlock(EXP_EvalContext* ctx)
{
    u32 defStackP = vec_last(&ctx->blockStack).defStackP;
    vec_resize(&ctx->defStack, defStackP);
    vec_pop(&ctx->blockStack);
    return ctx->blockStack.length > 0;
}





static void EXP_evalCall(EXP_EvalContext* ctx)
{
    EXP_Space* space = ctx->space;
    EXP_EvalBlock* curBlock;
next:
    curBlock = &vec_last(&ctx->blockStack);
    if (curBlock->p == curBlock->len)
    {
        if (curBlock->fun)
        {
            EXP_EvalAtomFun fun = curBlock->fun;
            u32 numArgs = curBlock->len;
            assert(numArgs <= ctx->dataStack.length);
            u32 argsOffset = ctx->dataStack.length - numArgs;
            EXP_EvalValue v = fun(numArgs, ctx->dataStack.data + argsOffset);
            vec_resize(&ctx->dataStack, argsOffset);
            vec_push(&ctx->dataStack, v);
        }
        if (EXP_evalLeaveBlock(ctx))
        {
            goto next;
        }
        return;
    }
    EXP_Node node = curBlock->seq[curBlock->p++];
    if (EXP_isTok(space, node))
    {
        const char* funName = EXP_tokCstr(space, node);
        EXP_EvalDef* def = EXP_evalGetMatched(ctx, funName);
        if (def)
        {
            u32 numParms = 0;;
            EXP_Node* parms = NULL;
            EXP_evalDefGetParms(ctx, def->val, &numParms, &parms);
            if (numParms > 0)
            {
                EXP_evalSyntaxErrorAtNode(ctx, node);
                return;
            }
            if (def->hasRtVal)
            {
                vec_push(&ctx->dataStack, def->rtVal);
                goto next;
            }
            else
            {
                u32 bodyLen = 0;
                EXP_Node* body = NULL;
                EXP_evalDefGetBody(ctx, def->val, &bodyLen, &body);
                if (EXP_evalEnterBlock(ctx, bodyLen, body, 0, NULL, NULL, NULL))
                {
                    goto next;
                }
                else
                {
                    return;
                }
            }
        }
        else
        {
            EXP_EvalValue v = { 0 };
            vec_push(&ctx->dataStack, v);
            goto next;
        }
    }
    else if (!EXP_evalCheckCall(space, node))
    {
        EXP_evalSyntaxErrorAtNode(ctx, node);
        return;
    }
    EXP_Node call = node;
    EXP_Node* elms = EXP_seqElm(space, call);
    u32 len = EXP_seqLen(space, call);
    const char* funName = EXP_tokCstr(space, elms[0]);
    EXP_EvalDef* def = EXP_evalGetMatched(ctx, funName);
    if (def)
    {
        u32 numParms = 0;;
        EXP_Node* parms = NULL;
        EXP_evalDefGetParms(ctx, def->val, &numParms, &parms);
        if (numParms != len - 1)
        {
            EXP_evalSyntaxErrorAtNode(ctx, call);
            return;
        }
        if (numParms > 0)
        {
            assert(!def->hasRtVal);
        }
        else if (def->hasRtVal)
        {
            vec_push(&ctx->dataStack, def->rtVal);
            goto next;
        }

        u32 bodyLen = 0;
        EXP_Node* body = NULL;
        EXP_evalDefGetBody(ctx, def->val, &bodyLen, &body);
        if (EXP_evalEnterBlock(ctx, bodyLen, body, numParms, parms, elms + 1, NULL))
        {
            goto next;
        }
        else
        {
            return;
        }
    }
    EXP_EvalPrimType primType = EXP_evalGetPrimFunType(space, funName);
    switch (primType)
    {
    case EXP_EvalPrimType_Def:
    {
        goto next;
    }
    case EXP_EvalPrimType_Blk:
    {
        if (EXP_evalEnterBlock(ctx, len - 1, elms + 1, 0, NULL, NULL, NULL))
        {
            goto next;
        }
        else
        {
            return;
        }
        break;
    }
    case EXP_EvalPrimType_If:
    {
        goto next;
    }
    default:
    {
        if (primType != -1)
        {
            EXP_EvalAtomFun fun = EXP_EvalPrimAtomFunTable[primType];
            assert(fun);
            u32 numParms = EXP_EvalPrimFunTypeNumParmsTable[primType];
            u32 numArgs = len - 1;
            if (numParms != (u32)-1)
            {
                if (numParms != numArgs)
                {
                    EXP_evalSyntaxErrorAtNode(ctx, node);
                    return;
                }
            }
            if (EXP_evalEnterBlock(ctx, numArgs, elms + 1, 0, NULL, NULL, fun))
            {
                goto next;
            }
            else
            {
                return;
            }
        }
        EXP_evalSyntaxErrorAtNode(ctx, call);
        break;
    }
    }
}
















EXP_EvalRet EXP_eval(EXP_Space* space, EXP_Node root, EXP_NodeSrcInfoTable* srcInfoTable)
{
    EXP_EvalRet ret = { 0 };
    if (!EXP_isSeq(space, root))
    {
        return ret;
    }
    EXP_EvalContext ctx = { space };
    u32 len = EXP_seqLen(space, root);
    EXP_Node* seq = EXP_seqElm(space, root);
    EXP_evalEnterBlock(&ctx, len, seq, 0, NULL, NULL, NULL);
    EXP_evalCall(&ctx);
    if (!ctx.hasHalt)
    {
        assert(1 == ctx.dataStack.length);
        ctx.ret.value = ctx.dataStack.data[0];
    }
    ret = ctx.ret;
    EXP_evalContextFree(&ctx);
    return ret;
}












EXP_EvalRet EXP_evalFile(EXP_Space* space, const char* entrySrcFile, bool debug)
{
    EXP_EvalRet ret = { EXP_EvalErrCode_NONE };
    char* src = NULL;
    u32 srcSize = fileu_readFile(entrySrcFile, &src);
    if (-1 == srcSize)
    {
        ret.errCode = EXP_EvalErrCode_SrcFile;
        ret.errSrcFile = entrySrcFile;
        return ret;
    }
    if (0 == srcSize)
    {
        return ret;
    }

    EXP_NodeSrcInfoTable* srcInfoTable = NULL;
    EXP_NodeSrcInfoTable _srcInfoTable = { 0 };
    if (debug)
    {
        srcInfoTable = &_srcInfoTable;
    }
    EXP_Node root = EXP_loadSrcAsList(space, src, srcInfoTable);
    free(src);
    if (EXP_NodeId_Invalid == root.id)
    {
        ret.errCode = EXP_EvalErrCode_ExpSyntax;
        ret.errSrcFile = entrySrcFile;
        if (srcInfoTable)
        {
#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable : 6011)
#endif
            ret.errSrcFileLine = vec_last(srcInfoTable).line;
            ret.errSrcFileColumn = vec_last(srcInfoTable).column;
#ifdef _MSC_VER
# pragma warning(pop)
#endif
        }
        else
        {
            ret.errSrcFileLine = -1;
            ret.errSrcFileColumn = -1;
        }
        return ret;
    }
    ret = EXP_eval(space, root, srcInfoTable);
    if (srcInfoTable)
    {
        vec_free(srcInfoTable);
    }
    return ret;
}















static EXP_EvalValue EXP_primFunHandle_Add(u32 numParms, EXP_EvalValue* args)
{
    assert(2 == numParms);
    EXP_EvalValue v;
    v.real = args[0].real + args[1].real;
    return v;
}
static EXP_EvalValue EXP_primFunHandle_Sub(u32 numParms, EXP_EvalValue* args)
{
    assert(2 == numParms);
    EXP_EvalValue v;
    v.real = args[0].real - args[1].real;
    return v;
}
static EXP_EvalValue EXP_primFunHandle_Mul(u32 numParms, EXP_EvalValue* args)
{
    assert(2 == numParms);
    EXP_EvalValue v;
    v.real = args[0].real * args[1].real;
    return v;
}
static EXP_EvalValue EXP_primFunHandle_Div(u32 numParms, EXP_EvalValue* args)
{
    assert(2 == numParms);
    EXP_EvalValue v;
    v.real = args[0].real / args[1].real;
    return v;
}




static EXP_EvalAtomFun EXP_EvalPrimAtomFunTable[EXP_NumEvalPrimTypes] =
{
    NULL,
    NULL,
    NULL,
    EXP_primFunHandle_Add,
    EXP_primFunHandle_Sub,
    EXP_primFunHandle_Mul,
    EXP_primFunHandle_Div,
};













































































































