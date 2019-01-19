#include "a.h"



typedef struct EXP_EvalDef
{
    const char* key;
    EXP_Node val;
    u32 numParms;
    EXP_Node parms[EXP_EvalFunParms_MAX];
} EXP_EvalDef;

typedef vec_t(EXP_EvalDef) EXP_EvalDefMap;


typedef struct EXP_EvalContext
{
    EXP_Space* space;
    bool hasHalt;
    EXP_EvalDefMap defStack;
    vec_u32 frameStack;
} EXP_EvalContext;


static void EXP_evalContextFree(EXP_EvalContext* ctx)
{
    vec_free(&ctx->frameStack);
    vec_free(&ctx->defStack);
}



typedef enum EXP_PrimFunType
{
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
    true,
    false,
    false,
    false,
    false,
    false,
    false,
};


typedef void(*EXP_PrimFunHandler)(EXP_EvalContext* ctx, u32 numArgs, EXP_Node* args);

static EXP_PrimFunHandler EXP_PrimFunHandlerTable[EXP_NumPrimFunTypes];

static EXP_PrimFunType EXP_getPrimExpType(EXP_Space* space, const char* funName)
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








static void EXP_primFunHandle_Def(EXP_EvalContext* ctx, u32 numArgs, EXP_Node* args)
{
}


static void EXP_primFunHandle_Write(EXP_EvalContext* ctx, u32 numArgs, EXP_Node* args)
{

}


static void EXP_primFunHandle_For(EXP_EvalContext* ctx, u32 numArgs, EXP_Node* args)
{

}


static void EXP_primFunHandle_Match(EXP_EvalContext* ctx, u32 numArgs, EXP_Node* args)
{

}


static void EXP_primFunHandle_Add(EXP_EvalContext* ctx, u32 numArgs, EXP_Node* args)
{
    printf("+\n");
}

static void EXP_primFunHandle_Sub(EXP_EvalContext* ctx, u32 numArgs, EXP_Node* args)
{
    printf("-\n");
}

static void EXP_primFunHandle_Mul(EXP_EvalContext* ctx, u32 numArgs, EXP_Node* args)
{
    printf("*\n");
}

static void EXP_primFunHandle_Div(EXP_EvalContext* ctx, u32 numArgs, EXP_Node* args)
{
    printf("/\n");
}








static bool EXP_evalCheckCall(EXP_Space* space, EXP_Node node)
{
    if (!EXP_isExp(space, node))
    {
        return false;
    }
    u32 len = EXP_expLen(space, node);
    if (!len)
    {
        return false;
    }
    EXP_Node* elms = EXP_expElm(space, node);
    if (!EXP_isStr(space, elms[0]))
    {
        return false;
    }
    return true;
}

static bool EXP_evalCheckDefPat(EXP_Space* space, EXP_Node node)
{
    if (!EXP_isExp(space, node))
    {
        return false;
    }
    u32 len = EXP_expLen(space, node);
    if (!len)
    {
        return false;
    }
    EXP_Node* elms = EXP_expElm(space, node);
    if (!EXP_isStr(space, elms[0]))
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
    EXP_Node* defCall = EXP_expElm(space, node);
    const char* defStr = EXP_strCstr(space, defCall[0]);
    EXP_PrimFunType primType = EXP_getPrimExpType(space, defStr);
    if (primType != EXP_PrimFunType_Def)
    {
        return;
    }
    const char* name = NULL;
    u32 numArgs = 0;
    EXP_Node* args = NULL;
    if (EXP_isStr(space, defCall[1]))
    {
        name = EXP_strCstr(space, defCall[1]);
    }
    else if (EXP_evalCheckDefPat(space, defCall[1]))
    {
        EXP_Node* pat = EXP_expElm(space, defCall[1]);
        name = EXP_strCstr(space, pat[0]);
        numArgs = EXP_expLen(space, defCall[1]) - 1;
        args = pat + 1;
    }
    else
    {
        ctx->hasHalt = true;
        return;
    }
    EXP_EvalDef def = { name, numArgs };
    for (u32 i = 0; i < numArgs; ++i)
    {
        def.parms[i] = args[i];
    }
    vec_push(&ctx->defStack, def);
}





static void EXP_evalCall(EXP_EvalContext* ctx, EXP_Node fun);

static void EXP_evalSeq
(
    EXP_EvalContext* ctx, u32 len, EXP_Node* seq, u32 numArgs, EXP_EvalDef args[EXP_EvalFunParms_MAX]
)
{
    u32 defMapSize0 = ctx->defStack.length;
    vec_push(&ctx->frameStack, defMapSize0);

    for (u32 i = 0; i < len; ++i)
    {
        EXP_evalLoadDef(ctx, seq[i]);
        if (ctx->hasHalt)
        {
            vec_resize(&ctx->defStack, defMapSize0);
            vec_pop(&ctx->frameStack);
            return;
        }
    }

    for (u32 i = 0; i < len; ++i)
    {
        EXP_evalCall(ctx, seq[i]);
        if (ctx->hasHalt)
        {
            vec_resize(&ctx->defStack, defMapSize0);
            vec_pop(&ctx->frameStack);
            return;
        }
    }

    vec_resize(&ctx->defStack, defMapSize0);
    vec_pop(&ctx->frameStack);
}





static EXP_EvalDef* EXP_getMatchedDef(EXP_EvalContext* ctx, const char* funName)
{
    for (u32 i = 0; i < ctx->defStack.length; ++i)
    {
        EXP_EvalDef* def = ctx->defStack.data + ctx->defStack.length - 1 - i;
        if (0 == strcmp(def->key, funName))
        {
            return def;
        }
    }
    return NULL;
}




static void EXP_evalCallDef(EXP_EvalContext* ctx, EXP_EvalDef* def, EXP_Node* argVals)
{
    EXP_Space* space = ctx->space;
    u32 len = EXP_expLen(space, def->val);
    EXP_Node* seq = EXP_expElm(space, def->val);
    EXP_EvalDef args[EXP_EvalFunParms_MAX];
    for (u32 i = 0; i < def->numParms; ++i)
    {
        args[i].key = EXP_strCstr(space, def->parms[i]);
        args[i].val = argVals[i];
    }
    EXP_evalSeq(ctx, len, seq, def->numParms, args);
}






static void EXP_evalCall(EXP_EvalContext* ctx, EXP_Node call)
{
    EXP_Space* space = ctx->space;
    if (!EXP_evalCheckCall(space, call))
    {
        ctx->hasHalt = true;
        return;
    }
    EXP_Node* elms = EXP_expElm(space, call);
    u32 len = EXP_expLen(space, call);
    const char* funName = EXP_strCstr(space, elms[0]);
    EXP_EvalDef* def = EXP_getMatchedDef(ctx, funName);
    if (def)
    {
        if (def->numParms != len - 1)
        {
            ctx->hasHalt = true;
            return;
        }
        EXP_evalCallDef(ctx, def, elms + 1);
        return;
    }
    EXP_PrimFunType primType = EXP_getPrimExpType(space, funName);
    if (primType != -1)
    {
        EXP_PrimFunHandler handler = EXP_PrimFunHandlerTable[primType];
        handler(ctx, len - 1, elms + 1);
        return;
    }
    ctx->hasHalt = true;
    return;
}




static void EXP_evalSeqBlock(EXP_EvalContext* ctx, EXP_Node expList)
{
    EXP_Space* space = ctx->space;
    assert(EXP_isExp(space, expList));

    u32 len = EXP_expLen(space, expList);
    EXP_Node* elms = EXP_expElm(space, expList);
    EXP_evalSeq(ctx, len, elms, 0, NULL);
}



EXP_Node EXP_eval(EXP_Space* space, EXP_Node root)
{
    EXP_Node node = { EXP_NodeId_Invalid };
    if (!EXP_isExp(space, root))
    {
        return node;
    }
    EXP_EvalContext ctx = { space };
    EXP_evalSeqBlock(&ctx, root);
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


















static EXP_PrimFunHandler EXP_PrimFunHandlerTable[EXP_NumPrimFunTypes] =
{
    EXP_primFunHandle_Def,
    EXP_primFunHandle_Write,
    EXP_primFunHandle_For,
    EXP_primFunHandle_Match,
    EXP_primFunHandle_Add,
    EXP_primFunHandle_Sub,
    EXP_primFunHandle_Mul,
    EXP_primFunHandle_Div,
};













































































































