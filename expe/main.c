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




static void execCode(const char* filename, const char* code)
{
    char timeBuf[EXP_EvalTimeStrBuf_MAX];
    printf("[EXEC] \"%s\" [%s]\n", filename, EXP_evalGetNowStr(timeBuf));
    EXP_EvalContext* ctx = EXP_newEvalContext(NULL);
    bool r;
    if (code)
    {
        r = EXP_evalCode(ctx, filename, code, true);
    }
    else
    {
        r = EXP_evalFile(ctx, filename, true);
    }
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
    if (r)
    {
        printf("<DataStack>\n");
        printf("-------------\n");
        EXP_evalDataStackFprint(stdout, ctx);
        printf("-------------\n");
    }
    EXP_evalContextFree(ctx);
}




static bool quitFlag = false;
void intHandler(int dummy)
{
    quitFlag = true;
}




int main(int argc, char* argv[])
{
#if !defined(NDEBUG) && defined(_WIN32)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    char timeBuf[EXP_EvalTimeStrBuf_MAX];

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
        execCode(entryFile, NULL);
        if (watchFlag)
        {
            signal(SIGINT, intHandler);
            while (!quitFlag)
            {
                stat(entryFile, &st);
                if (lastMtime != st.st_mtime)
                {
                    printf("[CHANGE] \"%s\" [%s]\n", entryFile, EXP_evalGetNowStr(timeBuf));
                    execCode(entryFile, NULL);
                }
                lastMtime = st.st_mtime;
            }
        }
    }
    else
    {
        vec_char code = { 0 };
        for (;;)
        {
            char c = getchar();
            if (EOF == c) break;
            vec_push(&code, c);
        }
        vec_push(&code, 0);
        execCode("stdin", code.data);
        vec_free(&code);
    }

    return EXIT_SUCCESS;
}






























































