#pragma once


#include "exp_eval.h"
#include "exp_a.h"

#include <nstr.h>




typedef vec_t(EXP_EvalAtypeInfo) EXP_EvalAtypeInfoVec;
typedef vec_t(EXP_EvalAfunInfo) EXP_EvalAfunInfoVec;




typedef enum EXP_EvalNodeType
{
    EXP_EvalNodeType_None = 0,

    EXP_EvalNodeType_VarDefBegin,
    EXP_EvalNodeType_VarDefEnd,
    EXP_EvalNodeType_GC,
    EXP_EvalNodeType_Afun,
    EXP_EvalNodeType_Apply,
    EXP_EvalNodeType_BlockExe,

    EXP_EvalNodeType_Var,
    EXP_EvalNodeType_Word,
    EXP_EvalNodeType_Atom,
    EXP_EvalNodeType_Block,

    EXP_EvalNodeType_CallAfun,
    EXP_EvalNodeType_CallWord,
    EXP_EvalNodeType_CallVar,

    EXP_EvalNodeType_Def,
    EXP_EvalNodeType_If,
} EXP_EvalNodeType;

typedef struct EXP_EvalNodeVar
{
    EXP_Node block;
    u32 id;
} EXP_EvalNodeVar;

typedef struct EXP_EvalNode
{
    EXP_EvalNodeType type;
    union
    {
        u32 afun;
        u32 atype;
        EXP_Node blkSrc;
        EXP_EvalNodeVar var;
    };
    u32 numIns;
    u32 numOuts;
    u32 varsCount;
} EXP_EvalNode;

typedef vec_t(EXP_EvalNode) EXP_EvalNodeTable;













static bool EXP_evalCheckCall(EXP_Space* space, EXP_Node node)
{
    if (!EXP_isSeqRound(space, node))
    {
        return false;
    }
    u32 len = EXP_seqLen(space, node);
    if (!len)
    {
        return false;
    }
    const EXP_Node* elms = EXP_seqElm(space, node);
    if (!EXP_isTok(space, elms[0]))
    {
        return false;
    }
    if (EXP_tokQuoted(space, elms[0]))
    {
        return false;
    }
    return true;
}





static const EXP_Node* EXP_evalIfBranch0(EXP_Space* space, EXP_Node node)
{
    const EXP_Node* elms = EXP_seqElm(space, node);
    return elms + 2;
}
static const EXP_Node* EXP_evalIfBranch1(EXP_Space* space, EXP_Node node)
{
    const EXP_Node* elms = EXP_seqElm(space, node);
    return elms + 3;
}








static void EXP_evalErrorFound
(
    EXP_EvalError* err, const EXP_SpaceSrcInfo* srcInfo, EXP_EvalErrCode errCode, EXP_Node node
)
{
    err->code = errCode;
    if (srcInfo)
    {
        const EXP_NodeSrcInfo* nodeSrcInfo = EXP_nodeSrcInfo(srcInfo, node);
        err->file = nodeSrcInfo->file;
        err->line = nodeSrcInfo->line;
        err->column = nodeSrcInfo->column;
    }
}













#include "exp_eval_typedecl.h"














typedef struct EXP_EvalObject
{
    bool gcFlag;
    char a[1];
} EXP_EvalObject;




typedef enum EXP_EvalBlockCallbackType
{
    EXP_EvalBlockCallbackType_NONE,
    EXP_EvalBlockCallbackType_Ncall,
    EXP_EvalBlockCallbackType_CallWord,
    EXP_EvalBlockCallbackType_CallVar,
    EXP_EvalBlockCallbackType_Cond,
} EXP_EvalBlockCallbackType;

typedef struct EXP_EvalBlockCallback
{
    EXP_EvalBlockCallbackType type;
    union
    {
        u32 afun;
        EXP_Node blkSrc;
    };
} EXP_EvalBlockCallback;





typedef struct EXP_EvalCall
{
    EXP_Node srcNode;
    const EXP_Node* p;
    const EXP_Node* end;
    u32 varBase;
    EXP_EvalBlockCallback cb;
} EXP_EvalCall;

typedef vec_t(EXP_EvalCall) EXP_EvalCallStack;






typedef vec_t(EXP_EvalObject*) EXP_EvalObjectPtrVec;
typedef vec_t(EXP_EvalObjectPtrVec) EXP_EvalObjectTable;






typedef struct EXP_EvalContext
{
    EXP_Space* space;
    EXP_SpaceSrcInfo srcInfo;
    EXP_EvalTypeContext* typeContext;
    EXP_EvalAtypeInfoVec atypeTable;
    EXP_EvalAfunInfoVec afunTable;
    vec_u32 afunTypeTable;
    EXP_EvalNodeTable nodeTable;
    EXP_EvalObjectTable objectTable;

    vec_u32 typeStack;
    EXP_EvalCallStack callStack;
    EXP_EvalValueVec varStack;
    EXP_EvalValueVec dataStack;

    EXP_EvalError error;
    EXP_EvalValue ncallOutBuf[EXP_EvalAfunOuts_MAX];
    bool gcFlag;
    EXP_EvalCompileTypeDeclStack typeDeclStack;

    EXP_EvalFileInfoTable fileInfoTable;
} EXP_EvalContext;





























































