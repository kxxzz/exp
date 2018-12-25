#pragma once



#include "i.h"




#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>
#include <stdio.h>




#ifdef ARYLEN
# undef ARYLEN
#endif
#define ARYLEN(a) (sizeof(a) / sizeof((a)[0]))




#ifdef max
# undef max
#endif
#ifdef min
# undef min
#endif
#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))




#define zalloc(sz) calloc(1, sz)



static char* stzncpy(char* dst, char const* src, size_t len)
{
    assert(len > 0);
    char* p = memccpy(dst, src, 0, len - 1);
    if (p) --p;
    else
    {
        p = dst + len - 1;
        *p = 0;
    }
    return p;
}







#include "vec.h"



typedef vec_t(struct saga_Node*) saga_NodeVec;

typedef struct saga_Node
{
    saga_NodeType type;
    union
    {
        vec_t(char) str;
        saga_NodeVec vec;
    };
    bool hasSrcInfo;
    saga_NodeSrcInfo srcInfo;
} saga_Node;



void saga_nodeVecFree(saga_NodeVec* vec);
void saga_nodeVecDup(saga_NodeVec* vec, const saga_NodeVec* a);
void saga_nodeVecConcat(saga_NodeVec* vec, const saga_NodeVec* a);













































