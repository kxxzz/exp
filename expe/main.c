#pragma warning(disable: 4101)

#include "exp/exp_eval.h"
#include "exp/exp_eval_utils.h"



#include <stdlib.h>
#ifdef _WIN32
# include <crtdbg.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <fileu.h>

#include <argparse.h>






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

    char* execFile = NULL;
    char* watchFile = NULL;
    struct argparse_option options[] =
    {
        OPT_HELP(),
        //OPT_GROUP("Basic options"),
        OPT_STRING('e', "exec", &execFile, "execute file"),
        OPT_STRING('w', "watch", &watchFile, "watch file and execute it when it changes"),
        OPT_END(),
    };
    struct argparse argparse;
    argparse_init(&argparse, options, NULL, 0);
    argc = argparse_parse(&argparse, argc, argv);

    if (execFile)
    {
        EXP_EvalContext* ctx = EXP_newEvalContext(NULL);
        bool r = EXP_evalFile(ctx, execFile, true);
        EXP_EvalError err = EXP_evalLastError(ctx);
        if (r)
        {
            assert(EXP_EvalErrCode_NONE == err.code);
        }
        else
        {
        }
        EXP_evalContextDataStackPrint(ctx);
        EXP_evalContextFree(ctx);
    }

    return mainReturn(EXIT_SUCCESS);
}






























































