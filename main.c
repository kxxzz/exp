#include "exp.h"



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
    u32 textSize = fileu_readFile("../0.exp", &text);
    assert(textSize != -1);


    EXP_Space* space = EXP_newSpace();
    EXP_NodeSrcInfoTable srcInfoTable = { 0 };
    EXP_Node root = EXP_loadSrcAsList(space, text, &srcInfoTable);
    assert(root.id != EXP_NodeId_Invalid);
    free(text);

    {
        EXP_SaveMLopt saveOpt = { 4, 50 };
        u32 text1BufSize = EXP_saveML(space, root, NULL, 0, &saveOpt) + 1;
        char* text1 = malloc(text1BufSize);
        u32 writen = EXP_saveML(space, root, text1, text1BufSize, &saveOpt) + 1;
        assert(text1BufSize == writen);
        printf("\"\n%s\"\n", text1);
        free(text1);
    }


    vec_free(&srcInfoTable);
    EXP_spaceFree(space);
}



void testExec(void)
{
    EXP_Space* space = EXP_newSpace();
    EXP_Node r = EXP_execFile(space, "../1.exp");
    assert(r.id != EXP_NodeId_Invalid);
    EXP_spaceFree(space);
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






























































