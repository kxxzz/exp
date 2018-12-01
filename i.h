#pragma once



#include <stdbool.h>
#include <stdint.h>


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




#include "vec.h"



typedef enum saga_CellType
{
    saga_CellType_Str,
    saga_CellType_Vec,

    saga_NumCellTypes
} saga_CellType;

static const char* saga_CellTypeNameTable[saga_NumCellTypes] =
{
    "Str",
    "Vec",
};



typedef vec_t(char) saga_Str;
typedef vec_t(struct saga_Cell) saga_Vec;



typedef struct saga_CellSrcInfo
{
    u32 offset;
    u32 line;
    u32 column;
    bool isStrTok;
} saga_CellSrcInfo;

typedef struct saga_Cell
{
    saga_CellType type;
    union
    {
        saga_Str str;
        saga_Vec vec;
    };
    bool hasSrcInfo;
    saga_CellSrcInfo srcInfo;
} saga_Cell;


void saga_cellFree(saga_Cell* cell);
void saga_cellDup(saga_Cell* a, const saga_Cell* b);

void saga_vecFree(saga_Vec* vec);
void saga_vecDup(saga_Vec* vec, const saga_Vec* a);
void saga_vecConcat(saga_Vec* vec, const saga_Vec* a);




saga_Cell saga_cellStr(const char* s);
u32 saga_strLen(saga_Cell* a);



bool saga_load(saga_Cell* cell, const char* str);

u32 saga_saveSL(const saga_Cell* cell, char* buf, u32 bufSize, bool withSrcInfo);





typedef struct saga_SaveMLopt
{
    u32 indent;
    u32 width;
    bool withSrcInfo;
} saga_SaveMLopt;

u32 saga_saveML(const saga_Cell* cell, char* buf, u32 bufSize, const saga_SaveMLopt* opt);































