#pragma warning(disable: 4101)

#include "txn.h"



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
    TXN_Node root;
    char* text;

    u32 textSize = FILEU_readFile("../1.txn", &text);
    assert(textSize != -1);

    TXN_Space* space = TXN_spaceNew();
    TXN_SpaceSrcInfo srcInfo[1] = { 0 };

    root = TXN_parseAsList(space, text, srcInfo);
    assert(root.id != TXN_Node_Invalid.id);
    assert(0 == vec_last(srcInfo->fileBases));
    assert(1 == srcInfo->fileBases->length);
    n = srcInfo->nodes->length;

    root = TXN_parseAsList(space, text, srcInfo);
    assert(root.id != TXN_Node_Invalid.id);
    assert(n == vec_last(srcInfo->fileBases));
    assert(2 == srcInfo->fileBases->length);
    assert(n * 2 == srcInfo->nodes->length);

    free(text);

    {
        u32 text1BufSize = TXN_printSL(space, root, NULL, 0, srcInfo) + 1;
        char* text1 = malloc(text1BufSize);
        u32 writen = TXN_printSL(space, root, text1, text1BufSize, srcInfo) + 1;
        assert(text1BufSize == writen);
        printf("\"\n%s\"\n", text1);
        free(text1);
    }

    {
        TXN_PrintMlOpt opt[1] = { 4, 50, srcInfo };
        u32 text1BufSize = TXN_printML(space, root, NULL, 0, opt) + 1;
        char* text1 = malloc(text1BufSize);
        u32 writen = TXN_printML(space, root, text1, text1BufSize, opt) + 1;
        assert(text1BufSize == writen);
        printf("\"\n%s\"\n", text1);
        free(text1);
    }

    TXN_spaceSrcInfoFree(srcInfo);
    TXN_spaceFree(space);
}







int main(int argc, char* argv[])
{
#if !defined(NDEBUG) && defined(_WIN32)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    pp_test();
    return mainReturn(EXIT_SUCCESS);
}






























































