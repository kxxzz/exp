#pragma once



#include "exp_eval_type.h"





typedef struct EXP_EvalCompileTypeDeclContext EXP_EvalCompileTypeDeclContext;

EXP_EvalCompileTypeDeclContext* EXP_evalCompileTypeDeclNewContext
(
    EXP_Space* space, EXP_EvalAtypeInfoVec* atypeTable, EXP_EvalTypeContext* typeContext
);
void EXP_evalCompileTypeDeclContextFree(EXP_EvalCompileTypeDeclContext* ctx);




u32 EXP_evalCompileTypeDecl
(
    EXP_EvalCompileTypeDeclContext* ctx, EXP_Node node, const EXP_SpaceSrcInfo* srcInfo, EXP_EvalError* outError
);





































































