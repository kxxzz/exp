#include "a.h"



typedef struct EXP_ExecContext
{
    EXP_Space* code;
    EXP_Space* rt;
} EXP_ExecContext;











int EXP_exec(EXP_Space* space, EXP_Node root)
{



    return 0;
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

























































































































