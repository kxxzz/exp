#pragma warning(disable: 4101)

#include "exp_eval.h"



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
    u32 textSize = FILEU_readFile("../0.exp", &text);
    assert(textSize != -1);


    EXP_Space* space = EXP_newSpace();
    EXP_SpaceSrcInfo srcInfo = { 0 };
    EXP_Node root = EXP_loadSrcAsList(space, text, &srcInfo);
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


    EXP_spaceSrcInfoFree(&srcInfo);
    EXP_spaceFree(space);
}



void testEval(void)
{
    EXP_Space* space = EXP_newSpace();
    EXP_EvalDataStack dataStack = { 0 };
    vec_u32 typeStack = { 0 };
    EXP_EvalError r = EXP_evalFile(space, &dataStack, "../1.exp", NULL, &typeStack, true);
    assert(EXP_EvalErrCode_NONE == r.code);
    for (u32 i = 0; i < dataStack.length; ++i)
    {
        EXP_EvalValue v = dataStack.data[i];
        u32 vt = typeStack.data[i];
        switch (vt)
        {
        case EXP_EvalPrimValueType_Bool:
        {
            printf("%s\n", v.truth ? "true" : "false");
            break;
        }
        case EXP_EvalPrimValueType_Num:
        {
            printf("%f\n", v.num);
            break;
        }
        case EXP_EvalPrimValueType_Str:
        {
            const char* s = v.str->data;
            printf("%s\n", s);
            break;
        }
        default:
        {
            const char* s = EXP_EvalPrimValueTypeInfoTable[vt].name;
            printf("%s\n", s);
            break;
        }
        }
    }
    vec_free(&dataStack);
    vec_free(&typeStack);
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
    testEval();

    return mainReturn(EXIT_SUCCESS);
}






























































