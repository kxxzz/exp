#include "a.h"



typedef struct EXP_ExecContext
{
    EXP_Space* space;
    bool hasHalt;
    int retValue;
} EXP_ExecContext;



void EXP_execCmd(EXP_ExecContext* ctx, EXP_Node cmd)
{
    EXP_Space* space = ctx->space;
    u32 len = EXP_expLen(space, cmd);
}


void EXP_execCmdList(EXP_ExecContext* ctx, EXP_Node cmdList)
{
    EXP_Space* space = ctx->space;
    assert(EXP_isExp(space, cmdList));
    u32 len = EXP_expLen(space, cmdList);
    EXP_Node* elms = EXP_expElm(space, cmdList);
    for (u32 i = 0; i < len; ++i)
    {
        EXP_execCmd(ctx, elms[i]);
        if (ctx->hasHalt) return;
    }
}



int EXP_exec(EXP_Space* space, EXP_Node root)
{
    if (!EXP_isExp(space, root))
    {
        return -1;
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
        return -1;
    }
    if (0 == srcSize)
    {
        return 0;
    }

    EXP_Space* space = EXP_newSpace();
    EXP_Node root = EXP_loadSrcAsList(space, src, srcInfoTable);
    free(src);
    if (EXP_InvalidNodeId == root.id)
    {
        EXP_spaceFree(space);
        return -1;
    }
    int r = EXP_exec(space, root);
    EXP_spaceFree(space);
    return r;
}

























































































































