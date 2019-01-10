#include "i.h"



#include <stdlib.h>
#ifdef _WIN32
# include <crtdbg.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <string.h>



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
    u32 textSize = readFile("../0.prim", &text);
    assert(textSize != -1);


    PRIM_Space* space = PRIM_newSpace();
    PRIM_NodeSrcInfoTable srcInfoTable = { 0 };
    PRIM_Node src = PRIM_loadList(space, text, &srcInfoTable);
    assert(src);
    free(text);


    PRIM_SaveMLopt saveOpt = { 4, 50 };


    u32 text1BufSize = PRIM_saveML(space, src, NULL, 0, &saveOpt) + 1;
    char* text1 = malloc(text1BufSize);


    u32 writen = PRIM_saveML(space, src, text1, text1BufSize, &saveOpt) + 1;
    assert(text1BufSize == writen);


    u32 text1Size = (u32)strlen(text1);
    assert(text1Size + 1 == text1BufSize);


    printf("\"\n%s\"\n", text1);
    free(text1);


    PRIM_nodeFree(src);

    PRIM_spaceFree(space);
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






























































