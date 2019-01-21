#include "exp_eval_a.h"



typedef union EXP_EvalValue
{
    void* ptr;
    uintptr_t integer;
    double real;
    EXP_Node node;
} EXP_EvalValue;

typedef vec_t(EXP_EvalValue) EXP_EvalDataStack;


typedef struct EXP_EvalDef
{
    EXP_Node key;
    EXP_Node val;
    bool hasRtVal;
    EXP_EvalValue rtVal;
} EXP_EvalDef;

typedef vec_t(EXP_EvalDef) EXP_EvalDefMap;


typedef struct EXP_EvalBlock
{
    EXP_Node* seq;
    u32 len;
    u32 p;
} EXP_EvalBlock;

typedef vec_t(EXP_EvalBlock) EXP_EvalBlockStack;


typedef struct EXP_EvalAtom
{
    EXP_EvalAtomFun fun;
    u32 numArgs;
    u32 remain;
} EXP_EvalAtom;

typedef vec_t(EXP_EvalAtom) EXP_EvalAtomStack;


typedef struct EXP_EvalContext
{
    EXP_Space* space;
    EXP_NodeSrcInfoTable* srcInfoTable;
    EXP_EvalRet ret;
    bool hasHalt;
    EXP_EvalDefMap defStack;
    vec_u32 scopeStack;
    EXP_EvalBlockStack blockStack;
    EXP_EvalDataStack dataStack;
    EXP_EvalAtomStack atomStack;
} EXP_EvalContext;


static void EXP_evalContextFree(EXP_EvalContext* ctx)
{
    vec_free(&ctx->atomStack);
    vec_free(&ctx->dataStack);
    vec_free(&ctx->blockStack);
    vec_free(&ctx->scopeStack);
    vec_free(&ctx->defStack);
}



typedef enum EXP_EvalPrimType
{
    EXP_EvalPrimType_Blk,
    EXP_EvalPrimType_Def,
    EXP_EvalPrimType_If,
    EXP_EvalPrimType_Add,
    EXP_EvalPrimType_Sub,
    EXP_EvalPrimType_Mul,
    EXP_EvalPrimType_Div,

    EXP_NumEvalPrimTypes
} EXP_EvalPrimType;

static const char* EXP_EvalPrimFunTypeNameTable[EXP_NumEvalPrimTypes] =
{
    "blk",
    "def",
    "if",
    "+",
    "-",
    "*",
    "/",
};

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











static EXP_Node* EXP_evalGetMatched(EXP_EvalContext* ctx, const char* funName)
{
    EXP_Space* space = ctx->space;
    for (u32 i = 0; i < ctx->defStack.length; ++i)
    {
        EXP_EvalDef* def = ctx->defStack.data + ctx->defStack.length - 1 - i;
        const char* str = EXP_tokCstr(space, def->key);
        if (0 == strcmp(str, funName))
        {
            return &def->val;
        }
    }
    return NULL;
}









static void EXP_evalPushScope(EXP_EvalContext* ctx)
{
    vec_push(&ctx->scopeStack, ctx->defStack.length);
}

static void EXP_evalPopScope(EXP_EvalContext* ctx)
{
    u32 defMapSize0 = vec_last(&ctx->scopeStack);
    vec_pop(&ctx->scopeStack);
    vec_resize(&ctx->defStack, defMapSize0);
}


static bool EXP_evalEnterBlock(EXP_EvalContext* ctx, u32 len, EXP_Node* seq)
{
    for (u32 i = 0; i < len; ++i)
    {
        EXP_evalLoadDef(ctx, seq[i]);
        if (ctx->hasHalt)
        {
            return false;;
        }
    }
    EXP_EvalBlock blk = { seq, len, 0 };
    vec_push(&ctx->blockStack, blk);
    return true;
}

static bool EXP_evalLeaveBlock(EXP_EvalContext* ctx)
{
    assert(ctx->blockStack.length > 0);
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
        if (EXP_evalLeaveBlock(ctx))
        {
            EXP_evalPopScope(ctx);
            goto next;
        }
        assert(0 == ctx->scopeStack.length);
        return;
    }
    EXP_Node call = curBlock->seq[curBlock->p++];
    if (!EXP_evalCheckCall(space, call))
    {
        EXP_evalSyntaxErrorAtNode(ctx, call);
        return;
    }
    EXP_Node* elms = EXP_seqElm(space, call);
    u32 len = EXP_seqLen(space, call);
    const char* funName = EXP_tokCstr(space, elms[0]);
    EXP_Node* val = EXP_evalGetMatched(ctx, funName);
    if (val)
    {
        u32 numParms = 0;;
        EXP_Node* parms = NULL;
        EXP_evalDefGetParms(ctx, *val, &numParms, &parms);
        if (numParms != len - 1)
        {
            EXP_evalSyntaxErrorAtNode(ctx, call);
            return;
        }

        EXP_evalPushScope(ctx);
        for (u32 i = 0; i < numParms; ++i)
        {
            EXP_Node k = parms[i];
            EXP_Node v = elms[1 + i];
            EXP_EvalDef def = { k, v };
            vec_push(&ctx->defStack, def);
        }
        u32 bodyLen = 0;
        EXP_Node* body = NULL;
        EXP_evalDefGetBody(ctx, *val, &bodyLen, &body);
        if (EXP_evalEnterBlock(ctx, bodyLen, body))
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
        EXP_evalPushScope(ctx);
        if (EXP_evalEnterBlock(ctx, len - 1, elms + 1))
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
            fun(ctx, len - 1, elms + 1);
            goto next;
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
    EXP_evalEnterBlock(&ctx, len, seq);
    EXP_evalCall(&ctx);
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















static void EXP_primFunHandle_Add(EXP_EvalContext* ctx, u32 numParms, EXP_Node* args)
{
    printf("+\n");
}

static void EXP_primFunHandle_Sub(EXP_EvalContext* ctx, u32 numParms, EXP_Node* args)
{
    printf("-\n");
}

static void EXP_primFunHandle_Mul(EXP_EvalContext* ctx, u32 numParms, EXP_Node* args)
{
    printf("*\n");
}

static void EXP_primFunHandle_Div(EXP_EvalContext* ctx, u32 numParms, EXP_Node* args)
{
    printf("/\n");
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













































































































