#pragma once


#include <stdbool.h>
#include <stdint.h>
#include <malloc.h>
#include <string.h>


typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;

typedef float f32;
typedef double f64;



typedef enum PRIM_NodeType
{
    PRIM_NodeType_Term,
    PRIM_NodeType_Char,

    PRIM_NumNodeTypes
} PRIM_NodeType;

typedef struct PRIM_Node
{
    PRIM_NodeType type;
    const PRIM_Node* term;
    uintptr_t ch;
} PRIM_Node;




























































































