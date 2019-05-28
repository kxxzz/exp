#pragma warning(disable: 4101)

#include "exp/exp.h"



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
    char* text = NULL;
    u32 textSize = FILEU_readFile("../1.exp", &text);
    assert(textSize != -1);


    EXP_Space* space = EXP_newSpace();
    EXP_SpaceSrcInfo srcInfo = { 0 };
    EXP_Node root = EXP_parseAsList(space, text, &srcInfo);
    assert(root.id != EXP_Node_Invalid.id);
    free(text);

    {
        EXP_PrintMlOpt opt = { 4, 50 };
        u32 text1BufSize = EXP_printML(space, root, NULL, 0, &opt) + 1;
        char* text1 = malloc(text1BufSize);
        u32 writen = EXP_printML(space, root, text1, text1BufSize, &opt) + 1;
        assert(text1BufSize == writen);
        printf("\"\n%s\"\n", text1);
        free(text1);
    }


    EXP_spaceSrcInfoFree(&srcInfo);
    EXP_spaceFree(space);
}







int main(int argc, char* argv[])
{
#if !defined(NDEBUG) && defined(_WIN32)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    pp_test();
    return mainReturn(EXIT_SUCCESS);
}






























































