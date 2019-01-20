#pragma once


#include "exp.h"


typedef enum EXP_EvalErrCode
{
    EXP_EvalErrCode_NONE,

    EXP_EvalErrCode_Syntax,
    EXP_EvalErrCode_SrcFile,
    EXP_EvalErrCode_User,

    EXP_NumEvalErrCodes
} EXP_EvalErrCode;

typedef struct EXP_EvalRet
{
    EXP_EvalErrCode errCode;
    EXP_Node errVal;
    uintptr_t retVal;
} EXP_EvalRet;

EXP_EvalRet EXP_eval(EXP_Space* space, EXP_Node root);

EXP_EvalRet EXP_evalFile(EXP_Space* space, const char* entrySrcFile);























































































