#include "a.h"


enum
{
    EXP_ExecDefArgs_MAX = 8,
};



typedef struct EXP_ExecDef
{
    EXP_Node key;
    EXP_Node val;
} EXP_ExecDef;

typedef vec_t(EXP_ExecDef) EXP_ExecDefMap;


typedef struct EXP_ExecContext
{
    EXP_Space* space;
    bool hasHalt;
    int retValue;
    EXP_ExecDefMap defmap;
    vec_u32 frameStack;
} EXP_ExecContext;


typedef enum EXP_PrimExpType
{
    EXP_PrimExpType_Def,
    EXP_PrimExpType_Read,
    EXP_PrimExpType_Write,
    EXP_PrimExpType_For,
    EXP_PrimExpType_Match,

    EXP_NumPrimExpTypes
} EXP_PrimExpType;

static const char* EXP_PrimExpTypeNameTable[EXP_NumPrimExpTypes] =
{
    "def",
    "read",
    "write",
    "for",
    "match",
};
static const bool EXP_PrimExpTypeSideEffectTable[EXP_NumPrimExpTypes] =
{
    false,
    true,
    true,
    false,
    false,
};



typedef void(*EXP_PrimExpHandler)(EXP_ExecContext* ctx, u32 numArgs, EXP_Node* args);



static void EXP_primExpHandle_Def(EXP_ExecContext* ctx, u32 numArgs, EXP_Node* args)
{
}

static void EXP_primExpHandle_Read(EXP_ExecContext* ctx, u32 numArgs, EXP_Node* args)
{

}

static void EXP_primExpHandle_Write(EXP_ExecContext* ctx, u32 numArgs, EXP_Node* args)
{

}

static void EXP_primExpHandle_For(EXP_ExecContext* ctx, u32 numArgs, EXP_Node* args)
{

}

static void EXP_primExpHandle_Match(EXP_ExecContext* ctx, u32 numArgs, EXP_Node* args)
{

}



static EXP_PrimExpHandler EXP_PrimExpHandlerTable[EXP_NumPrimExpTypes] =
{
    EXP_primExpHandle_Def,
    EXP_primExpHandle_Read,
    EXP_primExpHandle_Write,
    EXP_primExpHandle_For,
    EXP_primExpHandle_Match,
};




static EXP_PrimExpType EXP_getPrimExpType(EXP_Space* space, const char* expHead)
{
    for (u32 i = 0; i < EXP_NumPrimExpTypes; ++i)
    {
        if (0 == strcmp(expHead, EXP_PrimExpTypeNameTable[i]))
        {
            return i;
        }
    }
    return -1;
}








static EXP_Node EXP_getMatchedDef(EXP_ExecContext* ctx, const char* expHead)
{
    EXP_Node node = { EXP_NodeInvalidId };

    return node;
}






static void EXP_execExp(EXP_ExecContext* ctx, EXP_Node exp);

static void EXP_execExpList
(
    EXP_ExecContext* ctx, u32 len, EXP_Node* exps,
    u32 numArgs, EXP_Node argKeys[EXP_ExecDefArgs_MAX], EXP_Node argVals[EXP_ExecDefArgs_MAX]
)
{
    //vec_push(frameStack, level);

    for (u32 i = 0; i < len; ++i)
    {
        EXP_execExp(ctx, exps[i]);
        if (ctx->hasHalt) return;
    }

    //vec_pop(frameStack);
}


static void EXP_execCallDef(EXP_ExecContext* ctx, EXP_Node def, u32 numArgs, EXP_Node* argVals)
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
    u32 numParms = EXP_isExp(space, pat) ? EXP_expLen(space, pat) : 0;
    if (numParms != numArgs)
    {
        ctx->hasHalt = true;
        ctx->retValue = EXIT_FAILURE;
        return;
    }
    EXP_Node* argKeys = EXP_expElm(space, pat);
    EXP_Node* bodyElms = defElms + 2;
    EXP_execExpList(ctx, defLen - 2, bodyElms, numArgs, argKeys, argVals);
}




static void EXP_execExp(EXP_ExecContext* ctx, EXP_Node exp)
{
    EXP_Space* space = ctx->space;
    u32 len = EXP_expLen(space, exp);
    if (!len)
    {
        ctx->hasHalt = true;
        ctx->retValue = EXIT_FAILURE;
        return;
    }
    EXP_Node* elms = EXP_expElm(space, exp);
    if (!EXP_isStr(space, elms[0]))
    {
        ctx->hasHalt = true;
        ctx->retValue = EXIT_FAILURE;
        return;
    }
    const char* expHead = EXP_strCstr(space, elms[0]);
    EXP_Node def = EXP_getMatchedDef(ctx, expHead);
    if (def.id != EXP_NodeInvalidId)
    {
        EXP_execCallDef(ctx, def, len - 1, elms + 1);
        return;
    }
    EXP_PrimExpType primType = EXP_getPrimExpType(space, expHead);
    if (primType != -1)
    {
        EXP_PrimExpHandler handler = EXP_PrimExpHandlerTable[primType];
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
    EXP_execExpList(ctx, len, elms, 0, NULL, NULL);
}



int EXP_exec(EXP_Space* space, EXP_Node root)
{
    if (!EXP_isExp(space, root))
    {
        return EXIT_FAILURE;
    }
    EXP_ExecContext ctx = { space };
    EXP_execExpListBlock(&ctx, root);
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

























































































































