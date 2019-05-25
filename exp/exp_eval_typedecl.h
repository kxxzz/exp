#pragma once



#include "exp_eval_type.h"





typedef struct EXP_EvalTypeDeclContext EXP_EvalTypeDeclContext;

EXP_EvalTypeDeclContext* EXP_newEvalTypeDeclContext
(
    EXP_Space* space, EXP_EvalAtypeInfoVec* atypeTable, EXP_EvalTypeContext* typeContext
);
void EXP_evalTypeDeclContextFree(EXP_EvalTypeDeclContext* ctx);




u32 EXP_evalTypeDecl
(
    EXP_EvalTypeDeclContext* ctx, EXP_Node node, const EXP_SpaceSrcInfo* srcInfo, EXP_EvalError* outError
);





































































