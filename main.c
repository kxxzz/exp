#include "prim.h"



#include <stdlib.h>
#ifdef _WIN32
# include <crtdbg.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <fileu.h>




void testLoadSave(void)
{
    char* text = NULL;
    u32 textSize = fileu_readFile("../0.prim", &text);
    assert(textSize != -1);


    PRIM_Space* space = PRIM_newSpace();
    PRIM_NodeSrcInfoTable srcInfoTable = { 0 };
    PRIM_Node root = PRIM_loadSrcAsList(space, text, &srcInfoTable);
    assert(root.id != PRIM_InvalidNodeId);
    free(text);

    {
        PRIM_SaveMLopt saveOpt = { 4, 50 };
        u32 text1BufSize = PRIM_saveML(space, root, NULL, 0, &saveOpt) + 1;
        char* text1 = malloc(text1BufSize);
        u32 writen = PRIM_saveML(space, root, text1, text1BufSize, &saveOpt) + 1;
        assert(text1BufSize == writen);
        printf("\"\n%s\"\n", text1);
        free(text1);
    }


    vec_free(&srcInfoTable);
    PRIM_spaceFree(space);
}



void testExec(void)
{
    PRIM_NodeSrcInfoTable srcInfoTable = { 0 };
    int r = PRIM_execFile("../1.prim", &srcInfoTable);
    assert(0 == r);
    vec_free(&srcInfoTable);
}





static int mainReturn(int r)
{
#if !defined(NDEBUG) && defined(_WIN32)
    system("pause");
#endif
    return r;
}


int main(int argc, char* argv[])
{
#if !defined(NDEBUG) && defined(_WIN32)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    //testLoadSave();
    testExec();

    return mainReturn(EXIT_SUCCESS);
}






























































