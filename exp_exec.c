#include "a.h"


enum
{
    EXP_ExecDefVars_MAX = 8,
};



typedef struct EXP_ExecDef
{
    const char* key;
    EXP_Node val;
    u32 numVars;
    EXP_Node vars[EXP_ExecDefVars_MAX];
} EXP_ExecDef;

typedef vec_t(EXP_ExecDef) EXP_ExecDefMap;


typedef struct EXP_ExecContext
{
    EXP_Space* space;
    bool hasHalt;
    int retValue;
    EXP_ExecDefMap defMap;
    vec_u32 frameStack;
} EXP_ExecContext;


static void EXP_execContextFree(EXP_ExecContext* ctx)
{
    vec_free(&ctx->frameStack);
    vec_free(&ctx->defMap);
}



typedef enum EXP_PrimFunType
{
    EXP_PrimFunType_Def,
    EXP_PrimFunType_Read,
    EXP_PrimFunType_Write,
    EXP_PrimFunType_For,
    EXP_PrimFunType_Match,

    EXP_NumPrimFunTypes
} EXP_PrimFunType;

static const char* EXP_PrimFunTypeNameTable[EXP_NumPrimFunTypes] =
{
    "def",
    "read",
    "write",
    "for",
    "match",
};
static const bool EXP_PrimFunTypeSideEffectTable[EXP_NumPrimFunTypes] =
{
    false,
    true,
    true,
    false,
    false,
};



typedef void(*EXP_PrimFunHandler)(EXP_ExecContext* ctx, u32 numArgs, EXP_Node* args);



static void EXP_primFunHandle_Def(EXP_ExecContext* ctx, u32 numArgs, EXP_Node* args)
{
}

static void EXP_primFunHandle_Read(EXP_ExecContext* ctx, u32 numArgs, EXP_Node* args)
{

}

static void EXP_primFunHandle_Write(EXP_ExecContext* ctx, u32 numArgs, EXP_Node* args)
{

}

static void EXP_primFunHandle_For(EXP_ExecContext* ctx, u32 numArgs, EXP_Node* args)
{

}

static void EXP_primFunHandle_Match(EXP_ExecContext* ctx, u32 numArgs, EXP_Node* args)
{

}

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






static bool EXP_execCheckCall(EXP_Space* space, EXP_Node node)
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

static bool EXP_execCheckDefPat(EXP_Space* space, EXP_Node node)
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





static void EXP_execLoadDef(EXP_ExecContext* ctx, EXP_Node node)
{
    EXP_Space* space = ctx->space;
    if (!EXP_execCheckCall(space, node))
    {
        ctx->hasHalt = true;
        ctx->retValue = EXIT_FAILURE;
        return;
    }
    EXP_Node* defStmt = EXP_expElm(space, node);
    const char* defStr = EXP_strCstr(space, defStmt[0]);
    EXP_PrimFunType primType = EXP_getPrimExpType(space, defStr);
    if (primType != EXP_PrimFunType_Def)
    {
        return;
    }
    const char* name = NULL;
    u32 numArgs = 0;
    EXP_Node* args = NULL;
    if (EXP_isStr(space, defStmt[1]))
    {
        name = EXP_strCstr(space, defStmt[1]);
    }
    else if (EXP_execCheckDefPat(space, defStmt[1]))
    {
        EXP_Node* pat = EXP_expElm(space, defStmt[1]);
        name = EXP_strCstr(space, pat[0]);
        numArgs = EXP_expLen(space, defStmt[1]) - 1;
        args = pat + 1;
    }
    else
    {
        ctx->hasHalt = true;
        ctx->retValue = EXIT_FAILURE;
        return;
    }
    EXP_ExecDef def = { name, numArgs };
    for (u32 i = 0; i < numArgs; ++i)
    {
        def.vars[i] = args[i];
    }
    vec_push(&ctx->defMap, def);
}





static void EXP_execCall(EXP_ExecContext* ctx, EXP_Node fun);

static void EXP_execCallList
(
    EXP_ExecContext* ctx, u32 numCalls, EXP_Node* calls, u32 numVars, EXP_ExecDef vars[EXP_ExecDefVars_MAX]
)
{
    u32 defMapSize0 = ctx->defMap.length;
    vec_push(&ctx->frameStack, defMapSize0);

    for (u32 i = 0; i < numCalls; ++i)
    {
        EXP_execLoadDef(ctx, calls[i]);
        if (ctx->hasHalt)
        {
            vec_resize(&ctx->defMap, defMapSize0);
            vec_pop(&ctx->frameStack);
            return;
        }
    }

    for (u32 i = 0; i < numCalls; ++i)
    {
        EXP_execCall(ctx, calls[i]);
        if (ctx->hasHalt)
        {
            vec_resize(&ctx->defMap, defMapSize0);
            vec_pop(&ctx->frameStack);
            return;
        }
    }

    vec_resize(&ctx->defMap, defMapSize0);
    vec_pop(&ctx->frameStack);
}


static void EXP_execCallDef(EXP_ExecContext* ctx, EXP_Node def, u32 numArgs, EXP_Node* argsVal)
{
    EXP_Space* space = ctx->space;
    u32 defLen = EXP_expLen(space, def);
    if (defLen < 2)
    {
        ctx->hasHalt = true;
        ctx->retValue = EXIT_FAILURE;
        return;
    }
    EXP_Node* defElms = EXP_expElm(space, def);
    EXP_Node pat = defElms[1];
    u32 numVars = EXP_isExp(space, pat) ? EXP_expLen(space, pat) : 0;
    if (numVars != numArgs)
    {
        ctx->hasHalt = true;
        ctx->retValue = EXIT_FAILURE;
        return;
    }
    EXP_Node* varsKey = EXP_expElm(space, pat);
    EXP_Node* bodyElms = defElms + 2;
    EXP_ExecDef vars[EXP_ExecDefVars_MAX];
    for (u32 i = 0; i < numVars; ++i)
    {
        vars[i].key = EXP_strCstr(space, varsKey[i]);
        vars[i].val = argsVal[i];
    }
    EXP_execCallList(ctx, defLen - 2, bodyElms, numVars, vars);
}





static EXP_Node EXP_getMatchedDef(EXP_ExecContext* ctx, const char* funName)
{
    EXP_Node node = { EXP_NodeInvalidId };

    return node;
}







static void EXP_execCall(EXP_ExecContext* ctx, EXP_Node call)
{
    EXP_Space* space = ctx->space;
    if (!EXP_execCheckCall(space, call))
    {
        ctx->hasHalt = true;
        ctx->retValue = EXIT_FAILURE;
        return;
    }
    EXP_Node* elms = EXP_expElm(space, call);
    u32 len = EXP_expLen(space, call);
    const char* funName = EXP_strCstr(space, elms[0]);
    EXP_Node def = EXP_getMatchedDef(ctx, funName);
    if (def.id != EXP_NodeInvalidId)
    {
        EXP_execCallDef(ctx, def, len - 1, elms + 1);
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
    ctx->retValue = EXIT_FAILURE;
    return;
}




static void EXP_execExpListBlock(EXP_ExecContext* ctx, EXP_Node expList)
{
    EXP_Space* space = ctx->space;
    assert(EXP_isExp(space, expList));

    u32 len = EXP_expLen(space, expList);
    EXP_Node* elms = EXP_expElm(space, expList);
    EXP_execCallList(ctx, len, elms, 0, NULL);
}



int EXP_exec(EXP_Space* space, EXP_Node root)
{
    if (!EXP_isExp(space, root))
    {
        return EXIT_FAILURE;
    }
    EXP_ExecContext ctx = { space };
    EXP_execExpListBlock(&ctx, root);
    EXP_execContextFree(&ctx);
    return ctx.retValue;
}




int EXP_execFile(const char* srcFile)
{
    char* src = NULL;
    u32 srcSize = fileu_readFile(srcFile, &src);
    if (-1 == srcSize)
    {
        return EXIT_FAILURE;
    }
    if (0 == srcSize)
    {
        return EXIT_SUCCESS;
    }

    EXP_Space* space = EXP_newSpace();
    EXP_Node root = EXP_loadSrcAsList(space, src, NULL);
    free(src);
    if (EXP_NodeInvalidId == root.id)
    {
        EXP_spaceFree(space);
        return EXIT_FAILURE;
    }
    int r = EXP_exec(space, root);
    EXP_spaceFree(space);
    return r;
}


















static EXP_PrimFunHandler EXP_PrimFunHandlerTable[EXP_NumPrimFunTypes] =
{
    EXP_primFunHandle_Def,
    EXP_primFunHandle_Read,
    EXP_primFunHandle_Write,
    EXP_primFunHandle_For,
    EXP_primFunHandle_Match,
};













































































































