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
    char* p = _memccpy(dst, src, 0, len - 1);
    if (p) --p;
    else
    {
        p = dst + len - 1;
        *p = 0;
    }
    return p;
}







#include "vec.h"



typedef vec_t(struct PRIM_Node*) PRIM_NodeVec;

typedef struct PRIM_Node
{
    PRIM_NodeType type;
    union
    {
        vec_t(char) str;
        PRIM_NodeVec vec;
    };
    bool hasSrcInfo;
    PRIM_NodeSrcInfo srcInfo;
} PRIM_Node;



void PRIM_nodeVecFree(PRIM_NodeVec* vec);
void PRIM_nodeVecDup(PRIM_NodeVec* vec, const PRIM_NodeVec* a);
void PRIM_nodeVecConcat(PRIM_NodeVec* vec, const PRIM_NodeVec* a);













































