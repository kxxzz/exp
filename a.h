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






static void* zalloc(size_t size)
{
    void* p = malloc(size);
    memset(p, 0, size);
    return p;
}







#include "vec.h"



typedef vec_t(char) saga_Str;
typedef vec_t(struct saga_Node*) saga_Vec;

typedef struct saga_Node
{
    saga_Node* parent;
    saga_NodeType type;
    union
    {
        saga_Str str;
        saga_Vec vec;
    };
    bool hasSrcInfo;
    saga_NodeSrcInfo srcInfo;
} saga_Node;



void saga_vecFree(saga_Vec* vec);
void saga_vecDup(saga_Vec* vec, const saga_Vec* a);
void saga_vecConcat(saga_Vec* vec, const saga_Vec* a);













































