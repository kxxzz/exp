#pragma once


#include "exp.h"



typedef enum EXP_EvalPrimType
{
    EXP_EvalPrimType_Blk,
    EXP_EvalPrimType_Def,
    EXP_EvalPrimType_If,
    EXP_EvalPrimType_Add,
    EXP_EvalPrimType_Sub,
    EXP_EvalPrimType_Mul,
    EXP_EvalPrimType_Div,

    EXP_NumEvalPrimTypes
} EXP_EvalPrimType;

static const char* EXP_EvalPrimFunTypeNameTable[EXP_NumEvalPrimTypes] =
{
    "blk",
    "def",
    "if",
    "+",
    "-",
    "*",
    "/",
};

static u32 EXP_EvalPrimFunTypeNumParmsTable[EXP_NumEvalPrimTypes] =
{
    -1,
    -1,
    -1,
    2,
    2,
    2,
    2,
};



typedef union EXP_EvalValue
{
    void* ptr;
    uintptr_t integer;
    double real;
    EXP_Node node;
} EXP_EvalValue;

typedef EXP_EvalValue(*EXP_EvalAtomFun)(u32 numParms, EXP_EvalValue* args);



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
    EXP_EvalValue retVal;
} EXP_EvalRet;


EXP_EvalRet EXP_eval(EXP_Space* space, EXP_Node root, EXP_NodeSrcInfoTable* srcInfoTable);

EXP_EvalRet EXP_evalFile(EXP_Space* space, const char* entrySrcFile, bool debug);

























































































