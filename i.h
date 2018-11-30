#pragma once




#include "vec.h"




enum
{
    saga_NameStr_MAX = 255,
};





typedef enum saga_ItemType
{
    saga_ItemType_Num,
    saga_ItemType_Str,
    saga_ItemType_Vec,

    saga_NumItemTypes
} saga_ItemType;

static const char* saga_ItemTypeNameTable[saga_NumItemTypes] =
{
    "Num",
    "Str",
    "Vec",
};




typedef vec_t(struct saga_Item) saga_Vec;




typedef struct saga_ItemSrcInfo
{
    u32 offset;
    u32 line;
    u32 column;
    bool isStringTok;
    char numStr[saga_NameStr_MAX];
} saga_ItemSrcInfo;

typedef struct saga_Item
{
    saga_ItemType type;
    union
    {
        double number;
        char* string;
        saga_Vec vec;
    };
    u32 stringLen;
    bool hasSrcInfo;
    saga_ItemSrcInfo srcInfo;
} saga_Item;


void saga_itemFree(saga_Item* item);
void saga_itemDup(saga_Item* a, const saga_Item* b);

void saga_vecFree(saga_Vec* vec);
void saga_vecAdd(saga_Vec* vec, const saga_Item* item);
void saga_vecConcat(saga_Vec* vec, const saga_Vec* a);




static saga_Item saga_number(double n)
{
    saga_Item item = { saga_ItemType_Num };
    item.number = n;
    return item;
}
static saga_Item saga_string(const char* s)
{
    saga_Item item = { saga_ItemType_Str };
    item.stringLen = (u32)strlen(s);
    item.string = (char*)malloc(item.stringLen + 1);
    strncpy(item.string, s, item.stringLen + 1);
    return item;
}





bool saga_load(saga_Vec* vec, const char* str, u32 strSize);

u32 saga_ps(const saga_Item* item, char* buf, u32 bufSize, bool withSrcInfo);
u32 saga_psVec(const saga_Vec* vec, char* buf, u32 bufSize, bool withSrcInfo);





typedef struct saga_PPopt
{
    u32 indent;
    u32 width;
    bool withSrcInfo;
} saga_PPopt;

u32 saga_ppVec(const saga_Vec* vec, char* buf, u32 bufSize, const saga_PPopt* opt);
u32 saga_pp(const saga_Item* item, char* buf, u32 bufSize, const saga_PPopt* opt);

char* saga_ppVecAc(const saga_Vec* vec, const saga_PPopt* opt);
char* saga_ppAc(const saga_Item* item, const saga_PPopt* opt);





























