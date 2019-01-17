#include "a.h"



typedef struct PRIM_ExecContext
{
    PRIM_Space* code;
    PRIM_Space* rt;
} PRIM_ExecContext;











int PRIM_exec(PRIM_Space* space, PRIM_Node root)
{



    return 0;
}




int PRIM_execFile(const char* srcFile, PRIM_NodeSrcInfoTable* srcInfoTable)
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

    PRIM_Space* space = PRIM_newSpace();
    PRIM_Node root = PRIM_loadSrcAsList(space, src, srcInfoTable);
    free(src);
    if (PRIM_InvalidNodeId == root.id)
    {
        PRIM_spaceFree(space);
        return -1;
    }
    int r = PRIM_exec(space, root);
    PRIM_spaceFree(space);
    return r;
}

























































































































