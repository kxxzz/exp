#include "a.h"


typedef vec_t(EXP_Node) EXP_NodeVec;

typedef struct EXP_ExecContext
{
    EXP_Space* space;
    bool hasHalt;
    int retValue;
    EXP_NodeVec defStack;
} EXP_ExecContext;


typedef enum EXP_PrimCmdType
{
    EXP_PrimCmdType_Def,
    EXP_PrimCmdType_Read,
    EXP_PrimCmdType_Write,
    EXP_PrimCmdType_For,
    EXP_PrimCmdType_Match,

    EXP_NumPrimCmdTypes
} EXP_PrimCmdType;

static const char* EXP_PrimCmdTypeNameTable[EXP_NumPrimCmdTypes] =
{
    "def",
    "read",
    "write",
    "for",
    "match",
};

typedef void(*EXP_PrimCmdHandler)(EXP_ExecContext* ctx, u32 numArgs, EXP_Node* args);



void EXP_primCmdHandle_Def(EXP_ExecContext* ctx, u32 numArgs, EXP_Node* args)
{
}

void EXP_primCmdHandle_Read(EXP_ExecContext* ctx, u32 numArgs, EXP_Node* args)
{

}

void EXP_primCmdHandle_Write(EXP_ExecContext* ctx, u32 numArgs, EXP_Node* args)
{

}

void EXP_primCmdHandle_For(EXP_ExecContext* ctx, u32 numArgs, EXP_Node* args)
{

}

void EXP_primCmdHandle_Match(EXP_ExecContext* ctx, u32 numArgs, EXP_Node* args)
{

}



static EXP_PrimCmdHandler EXP_PrimCmdHandlerTable[EXP_NumPrimCmdTypes] =
{
    EXP_primCmdHandle_Def,
    EXP_primCmdHandle_Read,
    EXP_primCmdHandle_Write,
    EXP_primCmdHandle_For,
    EXP_primCmdHandle_Match,
};




static EXP_PrimCmdType EXP_getPrimCmdType(EXP_Space* space, EXP_Node cmdHead)
{
    const char* name = EXP_strCstr(space, cmdHead);
    for (u32 i = 0; i < EXP_NumPrimCmdTypes; ++i)
    {
        if (0 == strcmp(name, EXP_PrimCmdTypeNameTable[i]))
        {
            return i;
        }
    }
    return -1;
}




static EXP_Node EXP_getDefCmdDef(EXP_ExecContext* ctx, EXP_Node cmdHead)
{
    EXP_Node node = { EXP_NodeInvalidId };
    return node;
}




void EXP_execCmd(EXP_ExecContext* ctx, EXP_Node cmd)
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
    EXP_Node def = EXP_getDefCmdDef(ctx, cmdHead);
    if (def.id != EXP_NodeInvalidId)
    {
        return;
    }
    EXP_PrimCmdType primType = EXP_getPrimCmdType(space, cmdHead);
    if (primType != -1)
    {
        EXP_PrimCmdHandler handler = EXP_PrimCmdHandlerTable[primType];
        handler(ctx, len - 1, elms + 1);
        return;
    }
}


void EXP_execCmdList(EXP_ExecContext* ctx, EXP_Node cmdList)
{
    EXP_Space* space = ctx->space;
    assert(EXP_isExp(space, cmdList));

    EXP_NodeVec* defStack = &ctx->defStack;
    vec_push(defStack, cmdList);

    u32 len = EXP_expLen(space, cmdList);
    EXP_Node* elms = EXP_expElm(space, cmdList);
    for (u32 i = 0; i < len; ++i)
    {
        EXP_execCmd(ctx, elms[i]);
        if (ctx->hasHalt) return;
    }

    vec_pop(defStack);
}



int EXP_exec(EXP_Space* space, EXP_Node root)
{
    if (!EXP_isExp(space, root))
    {
        return EXIT_FAILURE;
    }
    EXP_ExecContext ctx = { space };
    EXP_execCmdList(&ctx, root);
    return ctx.retValue;
}




int EXP_execFile(const char* srcFile, EXP_NodeSrcInfoTable* srcInfoTable)
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
    EXP_Node root = EXP_loadSrcAsList(space, src, srcInfoTable);
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

























































































































