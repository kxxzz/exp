#include "a.h"


typedef vec_t(EXP_Node) EXP_NodeVec;

typedef struct EXP_ExecContext
{
    EXP_Space* space;
    bool hasHalt;
    int retValue;
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
    EXP_Space* space = ctx->space;
    if (!numArgs)
    {
        ctx->hasHalt = true;
        ctx->retValue = EXIT_FAILURE;
        return;
    }
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




static EXP_PrimExpType EXP_getPrimExpType(EXP_Space* space, EXP_Node cmdHead)
{
    const char* name = EXP_strCstr(space, cmdHead);
    for (u32 i = 0; i < EXP_NumPrimExpTypes; ++i)
    {
        if (0 == strcmp(name, EXP_PrimExpTypeNameTable[i]))
        {
            return i;
        }
    }
    return -1;
}




static EXP_Node EXP_getDefExpDef(EXP_ExecContext* ctx, EXP_Node cmdHead)
{
    EXP_Node node = { EXP_NodeInvalidId };
    return node;
}


static void EXP_execExp(EXP_ExecContext* ctx, EXP_Node cmd);

static void EXP_execExpList(EXP_ExecContext* ctx, u32 len, EXP_Node* cmds)
{
    for (u32 i = 0; i < len; ++i)
    {
        EXP_execExp(ctx, cmds[i]);
        if (ctx->hasHalt) return;
    }
}


static void EXP_execCallDef(EXP_ExecContext* ctx, EXP_Node def, u32 numArgs, EXP_Node* args)
{
    EXP_Space* space = ctx->space;
    u32 defLen = EXP_expLen(space, def);
    assert(defLen >= 2);
    EXP_Node* defElms = EXP_expElm(space, def);
    EXP_Node* bodyElms = defElms + 2;
    EXP_execExpList(ctx, defLen - 2, bodyElms);
}




static void EXP_execExp(EXP_ExecContext* ctx, EXP_Node cmd)
{
    EXP_Space* space = ctx->space;
    u32 len = EXP_expLen(space, cmd);
    if (!len)
    {
        ctx->hasHalt = true;
        ctx->retValue = EXIT_FAILURE;
        return;
    }
    EXP_Node* elms = EXP_expElm(space, cmd);
    EXP_Node cmdHead = elms[0];
    if (!EXP_isStr(space, cmdHead))
    {
        ctx->hasHalt = true;
        ctx->retValue = EXIT_FAILURE;
        return;
    }
    EXP_Node def = EXP_getDefExpDef(ctx, cmdHead);
    if (def.id != EXP_NodeInvalidId)
    {
        EXP_execCallDef(ctx, def, len - 1, elms + 1);
        return;
    }
    EXP_PrimExpType primType = EXP_getPrimExpType(space, cmdHead);
    if (primType != -1)
    {
        EXP_PrimExpHandler handler = EXP_PrimExpHandlerTable[primType];
        handler(ctx, len - 1, elms + 1);
        return;
    }
}


static void EXP_execExpListBlock(EXP_ExecContext* ctx, EXP_Node cmdList)
{
    EXP_Space* space = ctx->space;
    assert(EXP_isExp(space, cmdList));

    u32 len = EXP_expLen(space, cmdList);
    EXP_Node* elms = EXP_expElm(space, cmdList);
    EXP_execExpList(ctx, len, elms);
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

























































































































