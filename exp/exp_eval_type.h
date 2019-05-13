#pragma once



#include "exp.h"



typedef enum EXP_EvalTypeType
{
    EXP_EvalTypeType_Atom,
    EXP_EvalTypeType_List,
    EXP_EvalTypeType_Var,
    EXP_EvalTypeType_ListVar,
    EXP_EvalTypeType_Fun,
    EXP_EvalTypeType_Array,

    EXP_NumEvalTypeTypes
} EXP_EvalTypeType;

static const char** EXP_EvalTypeTypeNameTable(void)
{
    static const char* a[EXP_NumEvalTypeTypes] =
    {
        "Atom",
        "List",
        "Var",
        "ListVar",
        "Fun",
        "Array",
    };
    return a;
}


typedef struct EXP_EvalTypeDescList
{
    bool hasListElm;
    u32 count;
    u32 elms[1];
    // nothing could here
} EXP_EvalTypeDescList;

typedef struct EXP_EvalTypeDescFun
{
    u32 ins;
    u32 outs;
} EXP_EvalTypeDescFun;

typedef struct EXP_EvalTypeDescArray
{
    u32 elm;
} EXP_EvalTypeDescArray;

typedef struct EXP_EvalTypeDesc
{
    EXP_EvalTypeType type;
    union
    {
        u32 atype;
        EXP_EvalTypeDescList list;
        u32 varId;
        EXP_EvalTypeDescFun fun;
        EXP_EvalTypeDescArray ary;
    };
} EXP_EvalTypeDesc;




typedef struct EXP_EvalTypeContext EXP_EvalTypeContext;

EXP_EvalTypeContext* EXP_newEvalTypeContext(void);
void EXP_evalTypeContextFree(EXP_EvalTypeContext* ctx);



u32 EXP_evalTypeAtom(EXP_EvalTypeContext* ctx, u32 atype);
u32 EXP_evalTypeList(EXP_EvalTypeContext* ctx, const u32* elms, u32 count);
u32 EXP_evalTypeVar(EXP_EvalTypeContext* ctx, u32 varId);
u32 EXP_evalTypeListVar(EXP_EvalTypeContext* ctx, u32 varId);
u32 EXP_evalTypeFun(EXP_EvalTypeContext* ctx, u32 ins, u32 outs);
u32 EXP_evalTypeArray(EXP_EvalTypeContext* ctx, u32 elm);




const EXP_EvalTypeDesc* EXP_evalTypeDescById(EXP_EvalTypeContext* ctx, u32 typeId);



































































































