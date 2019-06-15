#pragma once



#include "inf.h"




#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <ctype.h>

#include <fileu.h>
#include <upool.h>




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



static char* stzncpy(char* dst, char const* src, u32 len)
{
    assert(len > 0);
#ifdef _WIN32
    char* p = _memccpy(dst, src, 0, len - 1);
#else
    char* p = memccpy(dst, src, 0, len - 1);
#endif
    if (p) --p;
    else
    {
        p = dst + len - 1;
        *p = 0;
    }
    return p;
}





static u32 align(u32 x, u32 a)
{
    return (x + a - 1) / a * a;
}










typedef struct INF_NodeInfo
{
    INF_NodeType type;
    u32 offset;
    u32 length;
    u32 quoted;
} INF_NodeInfo;

typedef vec_t(INF_NodeInfo) INF_NodeInfoVec;


typedef struct INF_SeqDefFrame
{
    INF_NodeType seqType;
    u32 p;
} INF_SeqDefFrame;

typedef vec_t(INF_SeqDefFrame) INF_SeqDefFrameVec;


typedef struct INF_Space
{
    INF_NodeInfoVec nodes[1];
    upool_t dataPool;
    INF_NodeVec seqDefStack[1];
    INF_SeqDefFrameVec seqDefFrameStack[1];
    vec_char cstrBuf[1];
} INF_Space;






































