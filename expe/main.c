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

#include <sys/stat.h>
#include <signal.h>

#include <argparse.h>

#include <fileu.h>






static void execFile(const char* filename)
{
    char timeBuf[EXP_EvalTimeStrBuf_MAX];
    printf("[EXEC] \"%s\" [%s]\n", filename, EXP_evalGetNowStr(timeBuf));
    EXP_EvalContext* ctx = EXP_newEvalContext(NULL);
    bool r = EXP_evalFile(ctx, filename, true);
    EXP_EvalError err = EXP_evalLastError(ctx);
    if (r)
    {
        assert(EXP_EvalErrCode_NONE == err.code);
        printf("[DONE] \"%s\" [%s]\n", filename, EXP_evalGetNowStr(timeBuf));
    }
    else
    {
        const EXP_EvalFileInfoTable* fiTable = EXP_evalFileInfoTable(ctx);
        EXP_evalErrorFprint(stderr, fiTable, &err);
    }
    printf("[DataStack] :\n");
    EXP_evalDataStackFprint(stdout, ctx);
    EXP_evalContextFree(ctx);
}


static bool quitFlag = false;
void intHandler(int dummy)
{
    quitFlag = true;
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
        time_t lastMtime;
        struct stat st;
        {
            stat(entryFile, &st);
            lastMtime = st.st_mtime;
        }
        execFile(entryFile);
        if (watchFlag)
        {
            signal(SIGINT, intHandler);
            while (!quitFlag)
            {
                stat(entryFile, &st);
                if (lastMtime != st.st_mtime)
                {
                    execFile(entryFile);
                }
                lastMtime = st.st_mtime;
            }
        }
    }
    else
    {
        argparse_usage(&argparse);
        return mainReturn(EXIT_FAILURE);
    }

    return mainReturn(EXIT_SUCCESS);
}






























































