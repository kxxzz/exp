#include "i.h"



#include <stdlib.h>
#ifdef _WIN32
# include <crtdbg.h>
#endif

#include <assert.h>
#include <stdio.h>



static u32 fileSize(FILE* f)
{
    u32 pos = ftell(f);
    fseek(f, 0, SEEK_END);
    u32 end = ftell(f);
    fseek(f, pos, SEEK_SET);
    return end;
}
static u32 readFile(const char* path, char** buf)
{
    FILE* f = fopen(path, "rb");
    if (!f)
    {
        return -1;
    }
    u32 size = fileSize(f);
    if (-1 == size)
    {
        return -1;
    }
    if (0 == size)
    {
        return 0;
    }
    *buf = (char*)malloc(size + 1);
    (*buf)[size] = 0;
    size_t r = fread(*buf, 1, size, f);
    if (r != (size_t)size)
    {
        free(*buf);
        *buf = NULL;
        fclose(f);
        return -1;
    }
    fclose(f);
    return size;
}


void test(void)
{
    char* text = NULL;
    u32 textSize = readFile("../0.saga", &text);
    assert(textSize != -1);


    saga_List src = { 0 };


    bool ok = saga_load(&src, text, textSize);
    assert(ok);
    free(text);


    saga_PPopt ppOpt = { 4, 50 };


    u32 ppBufSize = saga_ppList(&src, NULL, 0, &ppOpt) + 1;
    char* ppText = malloc(ppBufSize);


    u32 writen = saga_ppList(&src, ppText, ppBufSize, &ppOpt) + 1;
    assert(ppBufSize == writen);


    u32 ppTextSize = (u32)strlen(ppText);
    assert(ppTextSize + 1 == ppBufSize);


    printf("\"\n%s\"\n", ppText);
    free(ppText);


    saga_listFree(&src);
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

    test();

    return mainReturn(EXIT_SUCCESS);
}






























































