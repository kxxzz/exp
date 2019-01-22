#pragma once


#include "exp.h"



typedef union EXP_EvalValueData
{
    double num;
    EXP_Node node;
    void* ptr;
} EXP_EvalValueData;

typedef struct EXP_EvalValue
{
    u32 type;
    EXP_EvalValueData data;
} EXP_EvalValue;



typedef bool(*EXP_EvalValFromStr)(u32 len, const char* str, EXP_EvalValueData* pData);

typedef struct EXP_EvalValueTypeInfo
{
    const char* name;
    EXP_EvalValFromStr fromStr;
} EXP_EvalValueTypeInfo;



enum
{
    EXP_EvalNativeFunParms_MAX = 16,
};

typedef EXP_EvalValueData(*EXP_EvalNativeFunCall)(EXP_Space* space, u32 numParms, EXP_EvalValue* args);

typedef struct EXP_EvalNativeFunTypeInfo
{
    const char* name;
    EXP_EvalNativeFunCall call;
    u32 retType;
    u32 numParms;
    u32 parmType[EXP_EvalNativeFunParms_MAX];
} EXP_EvalNativeFunTypeInfo;




typedef enum EXP_EvalPrimValueType
{
    EXP_EvalPrimValueType_Num,
    EXP_EvalPrimValueType_Tok,
    EXP_EvalPrimValueType_Seq,

    EXP_NumEvalPrimValueTypes
} EXP_EvalPrimValueType;

const EXP_EvalValueTypeInfo EXP_EvalPrimValueTypeInfoTable[EXP_NumEvalPrimValueTypes];




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

const EXP_EvalNativeFunTypeInfo EXP_EvalPrimFunTypeInfoTable[EXP_NumEvalPrimFunTypes];





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

























































































