#pragma once



#include "exp_eval_type.h"




typedef struct EXP_EvalCompileTypeDeclLevel EXP_EvalCompileTypeDeclLevel;

typedef vec_t(EXP_EvalCompileTypeDeclLevel) EXP_EvalCompileTypeDeclStack;

EXP_evalCompileTypeDeclStackPop(EXP_EvalCompileTypeDeclStack* stack);
void EXP_evalCompileTypeDeclStackResize(EXP_EvalCompileTypeDeclStack* stack, u32 n);
void EXP_evalCompileTypeDeclStackFree(EXP_EvalCompileTypeDeclStack* stack);




u32 EXP_evalCompileTypeDecl
(
    EXP_Space* space,
    EXP_EvalAtypeInfoVec* atypeTable,
    EXP_EvalTypeContext* typeContext,
    EXP_EvalCompileTypeDeclStack* typeDeclStack,
    EXP_Node node,
    const EXP_SpaceSrcInfo* srcInfo,
    EXP_EvalError* outError
);





































































