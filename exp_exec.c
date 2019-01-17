#include "a.h"


enum
{
    EXP_ExecDefArgs_MAX = 8,
};



typedef struct EXP_ExecDefLevel
{
    u32 numElms;
    EXP_Node* elms;
    u32 numArgs;
    EXP_Node argKeys[EXP_ExecDefArgs_MAX];
    EXP_Node argVals[EXP_ExecDefArgs_MAX];
} EXP_ExecDefLevel;

typedef vec_t(EXP_ExecDefLevel) EXP_ExecDefLevelStack;


typedef struct EXP_ExecContext
{
    EXP_Space* space;
    bool hasHalt;
    int retValue;
    EXP_ExecDefLevelStack defLevelStack;
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








static EXP_Node EXP_getMatchedDefInLevel(EXP_ExecContext* ctx, EXP_ExecDefLevel* level, const char* expHead)
{
    EXP_Node node = { EXP_NodeInvalidId };
    EXP_Space* space = ctx->space;
    for (u32 i = 0; i < level->numElms; ++i)
    {
        EXP_Node e = level->elms[i];
        if (EXP_isExp(space, e) && (EXP_expLen(space, e) >= 2))
        {
            EXP_Node* exp = EXP_expElm(space, e);
            if (!EXP_isStr(space, exp[0]))
            {
                continue;
            }
            EXP_PrimExpType primType = EXP_getPrimExpType(space, EXP_strCstr(space, exp[0]));
            if (EXP_PrimExpType_Def != primType)
            {
                continue;
            }
            const char* name = NULL;
            if (EXP_isStr(space, exp[1]))
            {
                name = EXP_strCstr(space, exp[1]);
            }
            else
            {
                assert(EXP_isExp(space, exp[1]));
                if (!EXP_expLen(space, exp[1]))
                {
                    ctx->hasHalt = true;
                    ctx->retValue = EXIT_FAILURE;
                    return node;
                }
                name = EXP_strCstr(space, EXP_expElm(space, exp[1])[0]);
            }
            if (0 == strcmp(expHead, name))
            {
                return e;
            }
        }
    }
    return node;
}

static EXP_Node EXP_getMatchedDef(EXP_ExecContext* ctx, const char* expHead)
{
    EXP_Node node = { EXP_NodeInvalidId };
    EXP_ExecDefLevelStack* defLevelStack = &ctx->defLevelStack;
    for (u32 i = 0; i < defLevelStack->length; ++i)
    {
        u32 li = defLevelStack->length - 1 - i;
        EXP_ExecDefLevel* level = defLevelStack->data + li;
        node = EXP_getMatchedDefInLevel(ctx, level, expHead);
        if (node.id != EXP_NodeInvalidId)
        {
            return node;
        }
    }
    return node;
}






static void EXP_execExp(EXP_ExecContext* ctx, EXP_Node exp);

static void EXP_execExpList
(
    EXP_ExecContext* ctx, u32 len, EXP_Node* exps,
    u32 numArgs, EXP_Node argKeys[EXP_ExecDefArgs_MAX], EXP_Node argVals[EXP_ExecDefArgs_MAX]
)
{
    EXP_ExecDefLevelStack* defLevelStack = &ctx->defLevelStack;
    EXP_ExecDefLevel level = { len, exps, numArgs };
    for (u32 i = 0; i < numArgs; ++i)
    {
        level.argKeys[i] = argKeys[i];
        level.argVals[i] = argVals[i];
    }
    vec_push(defLevelStack, level);

    for (u32 i = 0; i < len; ++i)
    {
        EXP_execExp(ctx, exps[i]);
        if (ctx->hasHalt) return;
    }

    vec_pop(defLevelStack);
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
    // todo
    EXP_NodeSrcInfoTable srcInfoTable = { 0 };
    EXP_Node root = EXP_loadSrcAsList(space, src, &srcInfoTable);
    free(src);
    if (EXP_NodeInvalidId == root.id)
    {
        vec_free(&srcInfoTable);
        EXP_spaceFree(space);
        return EXIT_FAILURE;
    }
    int r = EXP_exec(space, root);
    vec_free(&srcInfoTable);
    EXP_spaceFree(space);
    return r;
}

























































































































