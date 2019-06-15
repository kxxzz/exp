#pragma warning(disable: 4101)

#include "inf/inf.h"



#include <stdlib.h>
#ifdef _WIN32
# include <crtdbg.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <fileu.h>




static int mainReturn(int r)
{
#if !defined(NDEBUG) && defined(_WIN32)
    system("pause");
#endif
    return r;
}






static void pp_test(void)
{
    u32 n;
    INF_Node root;
    char* text;

    u32 textSize = FILEU_readFile("../1.inf", &text);
    assert(textSize != -1);

    INF_Space* space = INF_newSpace();
    INF_SpaceSrcInfo srcInfo[1] = { 0 };

    root = INF_parseAsList(space, text, srcInfo);
    assert(root.id != INF_Node_Invalid.id);
    assert(0 == srcInfo->baseNodeId);
    assert(1 == srcInfo->fileCount);
    n = srcInfo->nodes->length;

    root = INF_parseAsList(space, text, srcInfo);
    assert(root.id != INF_Node_Invalid.id);
    assert(n == srcInfo->baseNodeId);
    assert(2 == srcInfo->fileCount);
    assert(n * 2 == srcInfo->nodes->length);

    free(text);

    {
        u32 text1BufSize = INF_printSL(space, root, NULL, 0, srcInfo) + 1;
        char* text1 = malloc(text1BufSize);
        u32 writen = INF_printSL(space, root, text1, text1BufSize, srcInfo) + 1;
        assert(text1BufSize == writen);
        printf("\"\n%s\"\n", text1);
        free(text1);
    }

    {
        INF_PrintMlOpt opt[1] = { 4, 50, srcInfo };
        u32 text1BufSize = INF_printML(space, root, NULL, 0, opt) + 1;
        char* text1 = malloc(text1BufSize);
        u32 writen = INF_printML(space, root, text1, text1BufSize, opt) + 1;
        assert(text1BufSize == writen);
        printf("\"\n%s\"\n", text1);
        free(text1);
    }

    INF_spaceSrcInfoFree(srcInfo);
    INF_spaceFree(space);
}







int main(int argc, char* argv[])
{
#if !defined(NDEBUG) && defined(_WIN32)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    pp_test();
    return mainReturn(EXIT_SUCCESS);
}






























































