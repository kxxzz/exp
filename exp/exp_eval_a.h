#pragma once


#include "exp_eval.h"
#include "exp_a.h"





typedef vec_t(EXP_EvalAtypeInfo) EXP_EvalAtypeInfoVec;
typedef vec_t(EXP_EvalAfunInfo) EXP_EvalAfunInfoVec;





#include "exp_eval_typedecl.h"






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
    if (err && srcInfo)
    {
        err->code = errCode;
        const EXP_NodeSrcInfo* nodeSrcInfo = EXP_nodeSrcInfo(srcInfo, node);
        err->file = nodeSrcInfo->file;
        err->line = nodeSrcInfo->line;
        err->column = nodeSrcInfo->column;
    }
}













typedef enum EXP_EvalBlockCallbackType
{
    EXP_EvalBlockCallbackType_NONE,
    EXP_EvalBlockCallbackType_Ncall,
    EXP_EvalBlockCallbackType_CallWord,
    EXP_EvalBlockCallbackType_CallVar,
    EXP_EvalBlockCallbackType_Cond,
    EXP_EvalBlockCallbackType_ArrayMap,
    EXP_EvalBlockCallbackType_ArrayFilter,
    EXP_EvalBlockCallbackType_ArrayReduce,
} EXP_EvalBlockCallbackType;



typedef struct EXP_EvalBlockCallback
{
    EXP_EvalBlockCallbackType type;
    union
    {
        u32 afun;
        EXP_Node blkSrc;
        u32 pos;
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











typedef struct EXP_EvalArray
{
    EXP_EvalValueVec data;
    u32 elmSize;
    u32 size;
} EXP_EvalArray;



static void EXP_evalArrayInit(EXP_EvalArray* a, u32 size)
{
    a->elmSize = 1;
    a->size = size;
}

static void EXP_evalArrayFree(EXP_EvalArray* a)
{
    vec_free(&a->data);
}





typedef vec_t(EXP_EvalArray*) EXP_EvalArrayPtrVec;









#include "exp_eval_object.h"










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
    APNUM_pool_t numPool;

    vec_u32 typeStack;
    EXP_EvalCallStack callStack;
    EXP_EvalValueVec varStack;
    EXP_EvalValueVec dataStack;

    EXP_EvalError error;
    EXP_EvalValue ncallOutBuf[EXP_EvalAfunOuts_MAX];
    bool gcFlag;
    EXP_EvalTypeDeclContext* typeDeclContext;
    EXP_EvalArrayPtrVec aryStack;

    EXP_EvalFileInfoTable fileInfoTable;
} EXP_EvalContext;





























































