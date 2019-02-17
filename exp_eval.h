#pragma once


#include "exp.h"



typedef vec_t(union EXP_EvalValue) EXP_EvalValueVec;

typedef union EXP_EvalValue
{
    bool truth;
    double num;
    vec_char* str;
    EXP_EvalValueVec* vec;
    void* ptr;
} EXP_EvalValue;



typedef bool(*EXP_EvalValCtorBySym)(u32 len, const char* str, EXP_EvalValue* pVal);
typedef void(*EXP_EvalValDtor)(EXP_EvalValue* pVal);

typedef struct EXP_EvalValueTypeInfo
{
    const char* name;
    EXP_EvalValCtorBySym ctorBySym;
    EXP_EvalValDtor dtor;
} EXP_EvalValueTypeInfo;



enum
{
    EXP_EvalNativeFunIns_MAX = 16,
    EXP_EvalNativeFunOuts_MAX = 16,
};

typedef void(*EXP_EvalNativeFunCall)(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs);

typedef struct EXP_EvalNativeFunInfo
{
    const char* name;
    EXP_EvalNativeFunCall call;
    u32 numIns;
    u32 inType[EXP_EvalNativeFunIns_MAX];
    u32 numOuts;
    u32 outType[EXP_EvalNativeFunOuts_MAX];
} EXP_EvalNativeFunInfo;



enum
{
    EXP_EvalValueType_Any = -1,
};

bool EXP_evalTypeMatch(u32 pat, u32 x);
bool EXP_evalTypeUnify(u32 a, u32 b, u32* out);


typedef enum EXP_EvalPrimValueType
{
    EXP_EvalPrimValueType_Bool,
    EXP_EvalPrimValueType_Num,
    EXP_EvalPrimValueType_Str,

    EXP_NumEvalPrimValueTypes
} EXP_EvalPrimValueType;

const EXP_EvalValueTypeInfo EXP_EvalPrimValueTypeInfoTable[EXP_NumEvalPrimValueTypes];



typedef enum EXP_EvalPrimFun
{
    EXP_EvalPrimFun_Def,
    EXP_EvalPrimFun_VarDefBegin,
    EXP_EvalPrimFun_VarDefEnd,
    EXP_EvalPrimFun_If,
    EXP_EvalPrimFun_Drop,
    EXP_EvalPrimFun_Blk,

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
    EXP_EvalErrCode_EvalUndefined,
    EXP_EvalErrCode_EvalArgs,
    EXP_EvalErrCode_EvalStack,
    EXP_EvalErrCode_EvalBranchUneq,
    EXP_EvalErrCode_EvalRecurNoBaseCase,

    EXP_NumEvalErrorCodes
} EXP_EvalErrCode;

typedef struct EXP_EvalError
{
    EXP_EvalErrCode code;
    const char* file;
    u32 line;
    u32 column;
} EXP_EvalError;


typedef struct EXP_EvalContext EXP_EvalContext;

EXP_EvalContext* EXP_newEvalContext(const EXP_EvalNativeEnv* nativeEnv);
void EXP_evalContextFree(EXP_EvalContext* ctx);

EXP_EvalError EXP_evalLastError(EXP_EvalContext* ctx);
vec_u32* EXP_evalDataTypeStack(EXP_EvalContext* ctx);
EXP_EvalValueVec* EXP_evalDataStack(EXP_EvalContext* ctx);

void EXP_eval(EXP_EvalContext* ctx, EXP_Node root, const char* name);
EXP_EvalContext* EXP_evalFile(const EXP_EvalNativeEnv* nativeEnv, const char* srcFile, bool enableSrcInfo);

























































































