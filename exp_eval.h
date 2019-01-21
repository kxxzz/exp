#pragma once


#include "exp.h"



typedef void(*EXP_EvalAtomFun)(u32 numParms, EXP_Node* args);



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
    uintptr_t retVal;
} EXP_EvalRet;


EXP_EvalRet EXP_eval(EXP_Space* space, EXP_Node root, EXP_NodeSrcInfoTable* srcInfoTable);

EXP_EvalRet EXP_evalFile(EXP_Space* space, const char* entrySrcFile, bool debug);

























































































