#pragma once


#include "exp.h"


typedef enum EXP_EvalKey
{
    EXP_EvalKey_Def,
    EXP_EvalKey_VarDefBegin,
    EXP_EvalKey_VarDefEnd,
    EXP_EvalKey_If,
    EXP_EvalKey_Drop,

    EXP_NumEvalKeys
} EXP_EvalKey;

const char* EXP_EvalKeyNameTable[EXP_NumEvalKeys];



typedef vec_t(union EXP_EvalValue) EXP_EvalValueVec;

typedef union EXP_EvalValue
{
    bool b;
    f64 f;
    vec_char* s;
    EXP_EvalValueVec* v;
    void* p;
} EXP_EvalValue;



typedef bool(*EXP_EvalNvalCtorBySym)(u32 len, const char* str, EXP_EvalValue* pVal);
typedef void(*EXP_EvalNvalDtor)(EXP_EvalValue* pVal);

typedef struct EXP_EvalNvalTypeInfo
{
    const char* name;
    EXP_EvalNvalCtorBySym ctorBySym;
    EXP_EvalNvalDtor dtor;
} EXP_EvalNvalTypeInfo;



enum
{
    EXP_EvalNfunIns_MAX = 16,
    EXP_EvalNfunOuts_MAX = 16,
};

typedef void(*EXP_EvalNfunCall)(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs);

typedef struct EXP_EvalNfunInfo
{
    const char* name;
    EXP_EvalNfunCall call;
    u32 numIns;
    u32 inType[EXP_EvalNfunIns_MAX];
    u32 numOuts;
    u32 outType[EXP_EvalNfunOuts_MAX];
} EXP_EvalNfunInfo;






typedef enum EXP_EvalPrimValueType
{
    EXP_EvalPrimValueType_BOOL,
    EXP_EvalPrimValueType_FLOAT,
    EXP_EvalPrimValueType_STRING,

    EXP_NumEvalPrimValueTypes
} EXP_EvalPrimValueType;

const EXP_EvalNvalTypeInfo EXP_EvalPrimValueTypeInfoTable[EXP_NumEvalPrimValueTypes];



typedef enum EXP_EvalPrimFun
{
    EXP_EvalPrimFun_Not,

    EXP_EvalPrimFun_Add,
    EXP_EvalPrimFun_Sub,
    EXP_EvalPrimFun_Mul,
    EXP_EvalPrimFun_Div,

    EXP_EvalPrimFun_Neg,

    EXP_EvalPrimFun_EQ,
    EXP_EvalPrimFun_INEQ,

    EXP_EvalPrimFun_GT,
    EXP_EvalPrimFun_LT,
    EXP_EvalPrimFun_GE,
    EXP_EvalPrimFun_LE,

    EXP_NumEvalPrimFuns
} EXP_EvalPrimFun;

const EXP_EvalNfunInfo EXP_EvalPrimFunInfoTable[EXP_NumEvalPrimFuns];



typedef struct EXP_EvalNativeEnv
{
    u32 numValueTypes;
    EXP_EvalNvalTypeInfo* valueTypes;
    u32 numFuns;
    EXP_EvalNfunInfo* funs;
} EXP_EvalNativeEnv;




typedef enum EXP_EvalErrCode
{
    EXP_EvalErrCode_NONE,

    EXP_EvalErrCode_SrcFile,
    EXP_EvalErrCode_ExpSyntax,
    EXP_EvalErrCode_EvalSyntax,
    EXP_EvalErrCode_EvalUnable,
    EXP_EvalErrCode_EvalUndefined,
    EXP_EvalErrCode_EvalArgs,
    EXP_EvalErrCode_EvalBranchUneq,
    EXP_EvalErrCode_EvalRecurNoBaseCase,

    EXP_NumEvalErrorCodes
} EXP_EvalErrCode;

typedef struct EXP_EvalError
{
    EXP_EvalErrCode code;
    u32 file;
    u32 line;
    u32 column;
} EXP_EvalError;





typedef struct EXP_EvalContext EXP_EvalContext;


EXP_EvalContext* EXP_newEvalContext(const EXP_EvalNativeEnv* nenv);
void EXP_evalContextFree(EXP_EvalContext* ctx);




EXP_EvalError EXP_evalLastError(EXP_EvalContext* ctx);
vec_u32* EXP_evalDataTypeStack(EXP_EvalContext* ctx);
EXP_EvalValueVec* EXP_evalDataStack(EXP_EvalContext* ctx);




void EXP_evalPushValue(EXP_EvalContext* ctx, u32 type, EXP_EvalValue* val);
void EXP_evalDrop(EXP_EvalContext* ctx);




void EXP_evalBlock(EXP_EvalContext* ctx, EXP_Node block);
bool EXP_evalCode(EXP_EvalContext* ctx, const char* code, bool enableSrcInfo);
bool EXP_evalFile(EXP_EvalContext* ctx, const char* fileName, bool enableSrcInfo);

























































































