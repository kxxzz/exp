#pragma once


#include "exp.h"


typedef enum EXP_EvalPrimValueType
{
    EXP_EvalPrimValueType_Num,
    EXP_EvalPrimValueType_Str,

    EXP_NumEvalPrimValueTypes
} EXP_EvalPrimValueType;

typedef struct EXP_EvalValue
{
    u32 type;
    union
    {
        double num;
        EXP_Node str;
        void* ptr;
    };
} EXP_EvalValue;

typedef EXP_EvalValue(*EXP_EvalNativeFunCall)(EXP_Space* space, u32 numParms, EXP_EvalValue* args);



typedef enum EXP_EvalPrimFunType
{
    EXP_EvalPrimFunType_Blk,
    EXP_EvalPrimFunType_Def,
    EXP_EvalPrimFunType_If,
    EXP_EvalPrimFunType_Add,
    EXP_EvalPrimFunType_Sub,
    EXP_EvalPrimFunType_Mul,
    EXP_EvalPrimFunType_Div,

    EXP_NumEvalPrimFunTypes
} EXP_EvalPrimFunType;

static const char* EXP_EvalPrimFunTypeNameTable[EXP_NumEvalPrimFunTypes] =
{
    "blk",
    "def",
    "if",
    "+",
    "-",
    "*",
    "/",
};

static u32 EXP_EvalPrimFunTypeNumParmsTable[EXP_NumEvalPrimFunTypes] =
{
    -1,
    -1,
    -1,
    2,
    2,
    2,
    2,
};



typedef enum EXP_EvalErrCode
{
    EXP_EvalErrCode_NONE,

    EXP_EvalErrCode_SrcFile,
    EXP_EvalErrCode_ExpSyntax,
    EXP_EvalErrCode_EvalSyntax,

    EXP_NumEvalErrCodes
} EXP_EvalErrCode;

typedef struct EXP_EvalRet
{
    EXP_EvalErrCode errCode;
    const char* errSrcFile;
    u32 errSrcFileLine;
    u32 errSrcFileColumn;
    EXP_EvalValue value;
} EXP_EvalRet;


EXP_EvalRet EXP_eval(EXP_Space* space, EXP_Node root, EXP_NodeSrcInfoTable* srcInfoTable);

EXP_EvalRet EXP_evalFile(EXP_Space* space, const char* entrySrcFile, bool debug);

























































































