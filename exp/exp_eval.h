#pragma once




#include "exp_eval_type.h"




typedef enum EXP_EvalKey
{
    EXP_EvalKey_Def,
    EXP_EvalKey_VarDefBegin,
    EXP_EvalKey_VarDefEnd,
    EXP_EvalKey_If,
    EXP_EvalKey_Drop,

    EXP_NumEvalKeys
} EXP_EvalKey;

const char** EXP_EvalKeyNameTable(void);



typedef vec_t(struct EXP_EvalValue) EXP_EvalValueVec;

typedef struct EXP_EvalValue
{
    union
    {
        bool b;
        f64 f;
        void* a;
    };
    bool allocated;
} EXP_EvalValue;


typedef bool(*EXP_EvalAtomCtorByStr)(u32 len, const char* str, EXP_EvalValue* pVal);
typedef void(*EXP_EvalAtomADtor)(void* ptr);

typedef struct EXP_EvalAtypeInfo
{
    const char* name;
    EXP_EvalAtomCtorByStr ctorByStr;
    bool fromSymAble;
    u32 allocMemSize;
    EXP_EvalAtomADtor aDtor;
} EXP_EvalAtypeInfo;



enum
{
    EXP_EvalAfunIns_MAX = 16,
    EXP_EvalAfunOuts_MAX = 16,
};

typedef void(*EXP_EvalAfunCall)(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs);

typedef struct EXP_EvalAfunInfo
{
    const char* name;
    EXP_EvalAfunCall call;
    u32 numIns;
    u32 inAtype[EXP_EvalAfunIns_MAX];
    u32 numOuts;
    u32 outAtype[EXP_EvalAfunOuts_MAX];
} EXP_EvalAfunInfo;




typedef enum EXP_EvalPrimType
{
    EXP_EvalPrimType_BOOL,
    EXP_EvalPrimType_FLOAT,
    EXP_EvalPrimType_STRING,

    EXP_NumEvalPrimTypes
} EXP_EvalPrimType;

const EXP_EvalAtypeInfo* EXP_EvalPrimTypeInfoTable(void);



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

const EXP_EvalAfunInfo* EXP_EvalPrimFunInfoTable(void);




typedef struct EXP_EvalAtomTable
{
    u32 numTypes;
    EXP_EvalAtypeInfo* types;
    u32 numFuns;
    EXP_EvalAfunInfo* funs;
} EXP_EvalAtomTable;




typedef enum EXP_EvalErrCode
{
    EXP_EvalErrCode_NONE,

    EXP_EvalErrCode_SrcFile,
    EXP_EvalErrCode_ExpSyntax,
    EXP_EvalErrCode_EvalSyntax,
    EXP_EvalErrCode_EvalUnkWord,
    EXP_EvalErrCode_EvalUnkCall,
    EXP_EvalErrCode_EvalArgs,
    EXP_EvalErrCode_EvalBranchUneq,
    EXP_EvalErrCode_EvalRecurNoBaseCase,
    EXP_EvalErrCode_EvalUnification,
    EXP_EvalErrCode_EvalAtomCtorByStr,

    EXP_NumEvalErrorCodes
} EXP_EvalErrCode;

const char** EXP_EvalErrCodeNameTable(void);


typedef struct EXP_EvalError
{
    EXP_EvalErrCode code;
    u32 file;
    u32 line;
    u32 column;
} EXP_EvalError;





typedef struct EXP_EvalContext EXP_EvalContext;


EXP_EvalContext* EXP_newEvalContext(const EXP_EvalAtomTable* addAtomTable);
void EXP_evalContextFree(EXP_EvalContext* ctx);




EXP_EvalError EXP_evalLastError(EXP_EvalContext* ctx);
EXP_EvalTypeContext* EXP_evalDataTypeContext(EXP_EvalContext* ctx);
vec_u32* EXP_evalDataTypeStack(EXP_EvalContext* ctx);
EXP_EvalValueVec* EXP_evalDataStack(EXP_EvalContext* ctx);




void EXP_evalPushValue(EXP_EvalContext* ctx, u32 type, EXP_EvalValue val);
void EXP_evalDrop(EXP_EvalContext* ctx);




void EXP_evalBlock(EXP_EvalContext* ctx, EXP_Node block);
bool EXP_evalCode(EXP_EvalContext* ctx, const char* filename, const char* code, bool enableSrcInfo);
bool EXP_evalFile(EXP_EvalContext* ctx, const char* fileName, bool enableSrcInfo);






enum
{
    EXP_EvalFileName_MAX = 255,
};

typedef struct EXP_EvalFileInfo
{
    char name[EXP_EvalFileName_MAX];
} EXP_EvalFileInfo;

typedef vec_t(EXP_EvalFileInfo) EXP_EvalFileInfoTable;

const EXP_EvalFileInfoTable* EXP_evalFileInfoTable(EXP_EvalContext* ctx);






















































































