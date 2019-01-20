#include "a.h"



typedef struct EXP_EvalDef
{
    EXP_Node key;
    EXP_Node val;
} EXP_EvalDef;

typedef vec_t(EXP_EvalDef) EXP_EvalDefMap;


typedef struct EXP_EvalContext
{
    EXP_Space* space;
    bool hasHalt;
    EXP_EvalDefMap defStack;
} EXP_EvalContext;


static void EXP_evalContextFree(EXP_EvalContext* ctx)
{
    vec_free(&ctx->defStack);
}



typedef enum EXP_PrimFunType
{
    EXP_PrimFunType_Block,
    EXP_PrimFunType_Def,
    EXP_PrimFunType_Write,
    EXP_PrimFunType_For,
    EXP_PrimFunType_Match,
    EXP_PrimFunType_Add,
    EXP_PrimFunType_Sub,
    EXP_PrimFunType_Mul,
    EXP_PrimFunType_Div,

    EXP_NumPrimFunTypes
} EXP_PrimFunType;

static const char* EXP_PrimFunTypeNameTable[EXP_NumPrimFunTypes] =
{
    "block",
    "def",
    "write",
    "for",
    "match",
    "+",
    "-",
    "*",
    "/",
};
static const bool EXP_PrimFunTypeSideEffectTable[EXP_NumPrimFunTypes] =
{
    false,
    false,
    true,
    false,
    false,
    false,
    false,
    false,
    false,
};


typedef void(*EXP_PrimFunHandler)(EXP_EvalContext* ctx, u32 numParms, EXP_Node* args);

static EXP_PrimFunHandler EXP_PrimFunHandlerTable[EXP_NumPrimFunTypes];

static EXP_PrimFunType EXP_getPrimFunType(EXP_Space* space, const char* funName)
{
    for (u32 i = 0; i < EXP_NumPrimFunTypes; ++i)
    {
        if (0 == strcmp(funName, EXP_PrimFunTypeNameTable[i]))
        {
            return i;
        }
    }
    return -1;
}









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





static void EXP_evalLoadDef(EXP_EvalContext* ctx, EXP_Node node)
{
    EXP_Space* space = ctx->space;
    if (!EXP_evalCheckCall(space, node))
    {
        ctx->hasHalt = true;
        return;
    }
    EXP_Node* defCall = EXP_seqElm(space, node);
    const char* kDef = EXP_tokCstr(space, defCall[0]);
    EXP_PrimFunType primType = EXP_getPrimFunType(space, kDef);
    if (primType != EXP_PrimFunType_Def)
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
        ctx->hasHalt = true;
        return;
    }
    EXP_EvalDef def = { name, node };
    vec_push(&ctx->defStack, def);
}




static void EXP_evalDefGetParms(EXP_EvalContext* ctx, EXP_Node node, u32* numParms, EXP_Node** pParms)
{
    EXP_Space* space = ctx->space;
    EXP_Node* defCall = EXP_seqElm(space, node);
    if (EXP_isTok(space, defCall[1]))
    {
        *numParms = 0;
        *pParms = NULL;
    }
    else
    {
        assert(EXP_evalCheckDefPat(space, defCall[1]));
        EXP_Node* pat = EXP_seqElm(space, defCall[1]);
        *numParms = EXP_seqLen(space, defCall[1]) - 1;
        *pParms = pat;
    }
}


static EXP_Node EXP_evalDefGetBody(EXP_EvalContext* ctx, EXP_Node node)
{
    EXP_Space* space = ctx->space;
    assert(3 == EXP_seqLen(space, node));
    EXP_Node* defCall = EXP_seqElm(space, node);
    return defCall[2];
}











static EXP_Node EXP_getMatched(EXP_EvalContext* ctx, const char* funName)
{
    EXP_Space* space = ctx->space;
    for (u32 i = 0; i < ctx->defStack.length; ++i)
    {
        EXP_EvalDef* def = ctx->defStack.data + ctx->defStack.length - 1 - i;
        const char* str = EXP_tokCstr(space, def->key);
        if (0 == strcmp(str, funName))
        {
            return def->val;
        }
    }
    EXP_Node node = { EXP_NodeId_Invalid };
    return node;
}





static void EXP_evalCall(EXP_EvalContext* ctx, EXP_Node fun);

static void EXP_evalApply(EXP_EvalContext* ctx, EXP_Node body, u32 numParms, EXP_Node* parms, EXP_Node* args)
{
    EXP_Space* space = ctx->space;

    u32 defMapSize0 = ctx->defStack.length;

    for (u32 i = 0; i < numParms; ++i)
    {
        EXP_Node k = parms[i];
        EXP_Node v = args[i];
        EXP_EvalDef def = { k, v };
        vec_push(&ctx->defStack, def);
    }
    EXP_evalCall(ctx, body);

    vec_resize(&ctx->defStack, defMapSize0);
}

static void EXP_evalCall(EXP_EvalContext* ctx, EXP_Node call)
{
    EXP_Space* space = ctx->space;
    if (!EXP_evalCheckCall(space, call))
    {
        ctx->hasHalt = true;
        return;
    }
    EXP_Node* elms = EXP_seqElm(space, call);
    u32 len = EXP_seqLen(space, call);
    const char* funName = EXP_tokCstr(space, elms[0]);
    EXP_Node body = EXP_getMatched(ctx, funName);
    if (body.id != EXP_NodeId_Invalid)
    {
        u32 numParms = 0;;
        EXP_Node* parms = NULL;
        EXP_evalDefGetParms(ctx, body, &numParms, &parms);
        if (numParms != len - 1)
        {
            ctx->hasHalt = true;
            return;
        }
        EXP_evalApply(ctx, body, numParms, parms, elms + 1);
        return;
    }
    EXP_PrimFunType primType = EXP_getPrimFunType(space, funName);
    if (primType != -1)
    {
        EXP_PrimFunHandler handler = EXP_PrimFunHandlerTable[primType];
        handler(ctx, len - 1, elms + 1);
        return;
    }
    ctx->hasHalt = true;
    return;
}






static void EXP_evalBlock(EXP_EvalContext* ctx, u32 len, EXP_Node* seq)
{
    u32 defMapSize0 = ctx->defStack.length;

    for (u32 i = 0; i < len; ++i)
    {
        EXP_evalLoadDef(ctx, seq[i]);
        if (ctx->hasHalt)
        {
            vec_resize(&ctx->defStack, defMapSize0);
            return;
        }
    }
    for (u32 i = 0; i < len; ++i)
    {
        EXP_evalCall(ctx, seq[i]);
        if (ctx->hasHalt)
        {
            vec_resize(&ctx->defStack, defMapSize0);
            return;
        }
    }

    vec_resize(&ctx->defStack, defMapSize0);
}






EXP_Node EXP_eval(EXP_Space* space, EXP_Node root)
{
    EXP_Node node = { EXP_NodeId_Invalid };
    if (!EXP_isSeq(space, root))
    {
        return node;
    }
    EXP_EvalContext ctx = { space };
    u32 len = EXP_seqLen(space, root);
    EXP_Node* seq = EXP_seqElm(space, root);
    EXP_evalBlock(&ctx, len, seq);
    EXP_evalContextFree(&ctx);
    return node;
}




EXP_Node EXP_evalFile(EXP_Space* space, const char* srcFile)
{
    EXP_Node node = { EXP_NodeId_Invalid };
    char* src = NULL;
    u32 srcSize = fileu_readFile(srcFile, &src);
    if (-1 == srcSize)
    {
        return node;
    }
    if (0 == srcSize)
    {
        return node;
    }

    EXP_Node root = EXP_loadSrcAsList(space, src, NULL);
    free(src);
    if (EXP_NodeId_Invalid == root.id)
    {
        EXP_spaceFree(space);
        return node;
    }
    node = EXP_eval(space, root);
    return node;
}

























static void EXP_primFunHandle_Block(EXP_EvalContext* ctx, u32 numParms, EXP_Node* args)
{
    EXP_evalBlock(ctx, numParms, args);
}


static void EXP_primFunHandle_Def(EXP_EvalContext* ctx, u32 numParms, EXP_Node* args)
{
}


static void EXP_primFunHandle_Write(EXP_EvalContext* ctx, u32 numParms, EXP_Node* args)
{

}


static void EXP_primFunHandle_For(EXP_EvalContext* ctx, u32 numParms, EXP_Node* args)
{

}


static void EXP_primFunHandle_Match(EXP_EvalContext* ctx, u32 numParms, EXP_Node* args)
{

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

static EXP_PrimFunHandler EXP_PrimFunHandlerTable[EXP_NumPrimFunTypes] =
{
    EXP_primFunHandle_Block,
    EXP_primFunHandle_Def,
    EXP_primFunHandle_Write,
    EXP_primFunHandle_For,
    EXP_primFunHandle_Match,
    EXP_primFunHandle_Add,
    EXP_primFunHandle_Sub,
    EXP_primFunHandle_Mul,
    EXP_primFunHandle_Div,
};













































































































