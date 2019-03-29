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

#include <argparse.h>

#include <fileu.h>
#include <filew.h>



static void execFile(const char* filename)
{
    EXP_EvalContext* ctx = EXP_newEvalContext(NULL);
    bool r = EXP_evalFile(ctx, filename, true);
    EXP_EvalError err = EXP_evalLastError(ctx);
    if (r)
    {
        assert(EXP_EvalErrCode_NONE == err.code);
    }
    else
    {
        EXP_evalErrorFprint(stderr, &err);
    }
    EXP_evalDataStackFprint(stdout, ctx);
    EXP_evalContextFree(ctx);
}

static void entryFileCallback(const char* dir, const char* filename, FILEW_Change change)
{
    switch (change)
    {
    case FILEW_Change_Add:
    case FILEW_Change_Delete:
        break;
    case FILEW_Change_Modified:
    {
        printf("refresh %s", filename);
        execFile(filename);
        break;
    }
    default:
        assert(false);
        break;
    }
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

    char* entryFile = NULL;
    int watchFlag = false;
    struct argparse_option options[] =
    {
        OPT_HELP(),
        //OPT_GROUP("Basic options"),
        OPT_STRING('f', "file", &entryFile, "execute entry file"),
        OPT_BOOLEAN('w', "watch", &watchFlag, "watch file and execute it when it changes"),
        OPT_END(),
    };
    struct argparse argparse;
    argparse_init(&argparse, options, NULL, 0);
    argc = argparse_parse(&argparse, argc, argv);

    if (entryFile)
    {
        FILEW_Context* wctx = NULL;
        if (watchFlag)
        {
            wctx = FILEW_newContext();
            FILEW_Callback cb = entryFileCallback;
            FILEW_addFile(wctx, entryFile, cb);
            
        }
        execFile(entryFile);
        if (watchFlag)
        {
            while (true)
            {
                FILEW_poll(wctx);
            }
            FILEW_contextFree(wctx);
        }
    }
    else
    {
        argparse_usage(&argparse);
        return mainReturn(EXIT_FAILURE);
    }

    return mainReturn(EXIT_SUCCESS);
}






























































