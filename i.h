#pragma once


#include "vec.h"


enum
{
    saga_NameStr_MAX = 255,
};



typedef enum saga_ItemType
{
    saga_ItemType_Number,
    saga_ItemType_String,
    saga_ItemType_List,

    saga_NumItemTypes
} saga_ItemType;

static const char* saga_ItemTypeNameTable[saga_NumItemTypes] =
{
    "Number",
    "String",
    "List",
};


typedef vec_t(struct saga_Item) saga_List;


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
        saga_List list;
    };
    u32 stringLen;
    bool hasSrcInfo;
    saga_ItemSrcInfo srcInfo;
} saga_Item;

void saga_itemFree(saga_Item* item);
void saga_itemDup(saga_Item* a, const saga_Item* b);

void saga_listFree(saga_List* list);
void saga_listAdd(saga_List* list, const saga_Item* item);
void saga_listConcat(saga_List* list, const saga_List* a);

static saga_Item saga_number(double n)
{
    saga_Item item = { saga_ItemType_Number };
    item.number = n;
    return item;
}
static saga_Item saga_string(const char* s)
{
    saga_Item item = { saga_ItemType_String };
    item.stringLen = (u32)strlen(s);
    item.string = (char*)malloc(item.stringLen + 1);
    strncpy(item.string, s, item.stringLen + 1);
    return item;
}


bool saga_load(saga_List* list, const char* str, u32 strSize);

u32 saga_ps(const saga_Item* item, char* buf, u32 bufSize, bool withSrcInfo);
u32 saga_psList(const saga_List* list, char* buf, u32 bufSize, bool withSrcInfo);


typedef struct saga_PPopt
{
    u32 indent;
    u32 width;
    bool withSrcInfo;
} saga_PPopt;

u32 saga_ppList(const saga_List* list, char* buf, u32 bufSize, const saga_PPopt* opt);
u32 saga_pp(const saga_Item* item, char* buf, u32 bufSize, const saga_PPopt* opt);

char* saga_ppListAc(const saga_List* list, const saga_PPopt* opt);
char* saga_ppAc(const saga_Item* item, const saga_PPopt* opt);





























