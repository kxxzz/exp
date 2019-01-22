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

typedef struct EXP_EvalNativeFunInfo
{
    const char* name;
    EXP_EvalNativeFunCall call;
    u32 retType;
    u32 numParms;
    u32 parmType[EXP_EvalNativeFunParms_MAX];
} EXP_EvalNativeFunInfo;




typedef enum EXP_EvalPrimValueType
{
    EXP_EvalPrimValueType_Num,
    EXP_EvalPrimValueType_Tok,
    EXP_EvalPrimValueType_Seq,

    EXP_NumEvalPrimValueTypes
} EXP_EvalPrimValueType;

const EXP_EvalValueTypeInfo EXP_EvalPrimValueTypeInfoTable[EXP_NumEvalPrimValueTypes];



typedef enum EXP_EvalPrimFun
{
    EXP_EvalPrimFun_Blk,
    EXP_EvalPrimFun_Def,
    EXP_EvalPrimFun_If,
    EXP_EvalPrimFun_Add,
    EXP_EvalPrimFun_Sub,
    EXP_EvalPrimFun_Mul,
    EXP_EvalPrimFun_Div,

    EXP_NumEvalPrimFuns
} EXP_EvalPrimFun;

const EXP_EvalNativeFunInfo EXP_EvalPrimFunInfoTable[EXP_NumEvalPrimFuns];



typedef struct EXP_EvalNativeEnv
{
    u32 numValueTypes;
    EXP_EvalValueTypeInfo* valueTypes;
    u32 numFuns;
    EXP_EvalNativeFunInfo* funs;
} EXP_EvalNativeEnv;




typedef enum EXP_EvalErrCode
{
    EXP_EvalErrCode_NONE,

    EXP_EvalErrCode_SrcFile,
    EXP_EvalErrCode_ExpSyntax,
    EXP_EvalErrCode_EvalSyntax,
    EXP_EvalErrCode_EvalArgs,

    EXP_NumEvalErrCodes
} EXP_EvalErrCode;

typedef struct EXP_EvalRet
{
    EXP_EvalErrCode errCode;
    const char* errSrcFile;
    u32 errSrcFileLine;
    u32 errSrcFileColumn;
} EXP_EvalRet;


typedef vec_t(EXP_EvalValue) EXP_EvalDataStack;


EXP_EvalRet EXP_eval
(
    EXP_Space* space, EXP_EvalDataStack* dataStack, EXP_Node root, const EXP_EvalNativeEnv* nativeEnv,
    EXP_NodeSrcInfoTable* srcInfoTable
);

EXP_EvalRet EXP_evalFile
(
    EXP_Space* space, EXP_EvalDataStack* dataStack, const char* srcFile, const EXP_EvalNativeEnv* nativeEnv,
    bool traceSrcInfo
);

























































































