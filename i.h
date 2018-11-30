#pragma once




#include "vec.h"




enum
{
    saga_NameStr_MAX = 255,
};





typedef enum saga_CellType
{
    saga_CellType_Num,
    saga_CellType_Str,
    saga_CellType_Vec,

    saga_NumCellTypes
} saga_CellType;

static const char* saga_CellTypeNameTable[saga_NumCellTypes] =
{
    "Num",
    "Str",
    "Vec",
};




typedef vec_t(struct saga_Cell) saga_Vec;




typedef struct saga_CellSrcInfo
{
    u32 offset;
    u32 line;
    u32 column;
    bool isStringTok;
    char numStr[saga_NameStr_MAX];
} saga_CellSrcInfo;

typedef struct saga_Cell
{
    saga_CellType type;
    union
    {
        double number;
        char* string;
        saga_Vec vec;
    };
    u32 stringLen;
    bool hasSrcInfo;
    saga_CellSrcInfo srcInfo;
} saga_Cell;


void saga_cellFree(saga_Cell* cell);
void saga_cellDup(saga_Cell* a, const saga_Cell* b);

void saga_vecFree(saga_Vec* vec);
void saga_vecAdd(saga_Vec* vec, const saga_Cell* cell);
void saga_vecConcat(saga_Vec* vec, const saga_Vec* a);




static saga_Cell saga_number(double n)
{
    saga_Cell cell = { saga_CellType_Num };
    cell.number = n;
    return cell;
}
static saga_Cell saga_string(const char* s)
{
    saga_Cell cell = { saga_CellType_Str };
    cell.stringLen = (u32)strlen(s);
    cell.string = (char*)malloc(cell.stringLen + 1);
    strncpy(cell.string, s, cell.stringLen + 1);
    return cell;
}





bool saga_load(saga_Vec* vec, const char* str, u32 strSize);

u32 saga_ps(const saga_Cell* cell, char* buf, u32 bufSize, bool withSrcInfo);
u32 saga_psVec(const saga_Vec* vec, char* buf, u32 bufSize, bool withSrcInfo);





typedef struct saga_PPopt
{
    u32 indent;
    u32 width;
    bool withSrcInfo;
} saga_PPopt;

u32 saga_ppVec(const saga_Vec* vec, char* buf, u32 bufSize, const saga_PPopt* opt);
u32 saga_pp(const saga_Cell* cell, char* buf, u32 bufSize, const saga_PPopt* opt);

char* saga_ppVecAc(const saga_Vec* vec, const saga_PPopt* opt);
char* saga_ppAc(const saga_Cell* cell, const saga_PPopt* opt);





























