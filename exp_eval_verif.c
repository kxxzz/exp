#include "exp_eval_a.h"



//typedef struct EXP_EvalValueTypeDef
//{
//    int x;
//} EXP_EvalValueTypeDef;



typedef struct EXP_EvalBlockInfo
{
    bool hasInited;
    u32 numIn;
    u32 numOut;
    u32* insType;
    u32* outsType;
    u32 parent;
} EXP_EvalBlockInfo;

typedef vec_t(EXP_EvalBlockInfo) EXP_EvalBlockInfoTable;



typedef struct EXP_EvalVerifBlock
{
    EXP_Node srcNode;
    u32 dataStackP;
    EXP_Node* seq;
    u32 seqLen;
    u32 p;
    EXP_EvalBlockCallback cb;
} EXP_EvalVerifBlock;

typedef vec_t(EXP_EvalVerifBlock) EXP_EvalVerifBlockStack;



typedef struct EXP_EvalVerifContext
{
    EXP_Space* space;
    EXP_EvalValueTypeInfoTable* valueTypeTable;
    EXP_EvalNativeFunInfoTable* nativeFunTable;
    EXP_NodeSrcInfoTable* srcInfoTable;
    EXP_EvalBlockInfoTable blockTable;
    vec_u32 dataStack;
    EXP_EvalVerifBlockStack blockStack;
    EXP_EvalError error;
} EXP_EvalVerifContext;





static EXP_EvalVerifContext EXP_newEvalVerifContext
(
    EXP_Space* space, EXP_EvalValueTypeInfoTable* valueTypeTable, EXP_EvalNativeFunInfoTable* nativeFunTable,
    EXP_NodeSrcInfoTable* srcInfoTable
)
{
    EXP_EvalVerifContext _ctx = { 0 };
    EXP_EvalVerifContext* ctx = &_ctx;
    ctx->space = space;
    ctx->valueTypeTable = valueTypeTable;
    ctx->nativeFunTable = nativeFunTable;
    ctx->srcInfoTable = srcInfoTable;
    return *ctx;
}
static void EXP_evalVerifContextFree(EXP_EvalVerifContext* ctx)
{
    vec_free(&ctx->dataStack);
    vec_free(&ctx->blockTable);
}






static void EXP_evalVerifErrorAtNode(EXP_EvalVerifContext* ctx, EXP_Node node, EXP_EvalErrCode errCode)
{
    ctx->error.code = errCode;
    EXP_NodeSrcInfoTable* srcInfoTable = ctx->srcInfoTable;
    if (srcInfoTable)
    {
        assert(node.id < srcInfoTable->length);
        ctx->error.file = NULL;// todo
        ctx->error.line = srcInfoTable->data[node.id].line;
        ctx->error.column = srcInfoTable->data[node.id].column;
    }
}







static EXP_Node EXP_evalVerifGetMatched(EXP_EvalVerifContext* ctx, const char* funName)
{
    EXP_Node node = { EXP_NodeId_Invalid };
    return node;
}

static u32 EXP_evalVerifGetNativeFun(EXP_EvalVerifContext* ctx, const char* funName)
{
    EXP_Space* space = ctx->space;
    for (u32 i = 0; i < ctx->nativeFunTable->length; ++i)
    {
        u32 idx = ctx->nativeFunTable->length - 1 - i;
        const char* name = ctx->nativeFunTable->data[idx].name;
        if (0 == strcmp(funName, name))
        {
            return idx;
        }
    }
    return -1;
}








static void EXP_evalVerifEnterBlock(EXP_EvalVerifContext* ctx, u32 len, EXP_Node* seq, EXP_Node srcNode)
{


}

static void EXP_evalVerifEnterBlockWithCB
(
    EXP_EvalVerifContext* ctx, u32 len, EXP_Node* seq, EXP_Node srcNode, EXP_EvalBlockCallback cb
)
{

}

static bool EXP_evalVerifLeaveBlock(EXP_EvalVerifContext* ctx)
{
    vec_pop(&ctx->blockStack);
    return ctx->blockStack.length > 0;
}






static void EXP_evalVerifCall(EXP_EvalVerifContext* ctx)
{
    EXP_Space* space = ctx->space;
    EXP_EvalVerifBlock* curBlock;
    vec_u32* dataStack = &ctx->dataStack;
next:
    if (ctx->error.code)
    {
        return;
    }
    curBlock = &vec_last(&ctx->blockStack);
    if (curBlock->p == curBlock->seqLen)
    {
        EXP_EvalBlockCallback* cb = &curBlock->cb;
        switch (cb->type)
        {
        case EXP_EvalBlockCallbackType_NONE:
        {
            break;
        }
        case EXP_EvalBlockCallbackType_NativeFun:
        {
            u32 numIns = dataStack->length - curBlock->dataStackP;
            EXP_EvalNativeFunInfo* nativeFunInfo = ctx->nativeFunTable->data + cb->nativeFun;
            if (numIns != nativeFunInfo->numIns)
            {
                EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
                return;
            }
            //EXP_evalNativeFunCall(ctx, nativeFunInfo, curBlock->dataStackP, curBlock->srcNode);
            break;
        }
        case EXP_EvalBlockCallbackType_Fun:
        {
            if (curBlock->dataStackP > dataStack->length)
            {
                EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
                return;
            }
            //EXP_Node fun = cb->fun;
            //if (curBlock->dataStackP != (dataStack->length - numIns))
            //{
            //    EXP_evalErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
            //    return;
            //}
            // todo
            //if (!EXP_evalLeaveBlock(ctx))
            //{
            //    return;
            //}
            //if (EXP_evalEnterBlock(ctx, bodyLen, body, curBlock->srcNode))
            //{
            //    goto next;
            //}
            return;
        }
        case EXP_EvalBlockCallbackType_Branch:
        {
            if (curBlock->dataStackP + 1 != dataStack->length)
            {
                EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
                return;
            }
            u32 v = dataStack->data[curBlock->dataStackP];
            vec_pop(dataStack);
            //if (!EXP_evalValueTypeConvert(ctx, &v, EXP_EvalPrimValueType_Bool, curBlock->srcNode))
            //{
            //    return;
            //}
            if (!EXP_evalVerifLeaveBlock(ctx))
            {
                return;
            }
            //if (EXP_evalEnterBlock(ctx, 1, cb->branch[0], curBlock->srcNode))
            //{
            //    goto next;
            //}
            //if (cb->branch[1])
            //{
            //    if (EXP_evalEnterBlock(ctx, 1, cb->branch[1], curBlock->srcNode))
            //    {
            //        goto next;
            //    }
            //}
            goto next;
        }
        default:
            assert(false);
            return;
        }
        if (EXP_evalVerifLeaveBlock(ctx))
        {
            goto next;
        }
        return;
    }
    EXP_Node node = curBlock->seq[curBlock->p++];
    if (EXP_isTok(space, node))
    {

    }
    else if (!EXP_evalCheckCall(space, node))
    {
        EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalSyntax);
        return;
    }
    EXP_Node call = node;
    EXP_Node* elms = EXP_seqElm(space, call);
    u32 len = EXP_seqLen(space, call);
    const char* funName = EXP_tokCstr(space, elms[0]);
    EXP_Node blk = EXP_evalVerifGetMatched(ctx, funName);
    if (blk.id != EXP_NodeId_Invalid)
    {
        // todo
    }
    u32 nativeFun = EXP_evalVerifGetNativeFun(ctx, funName);
    switch (nativeFun)
    {
    case EXP_EvalPrimFun_Def:
    {
        goto next;
    }
    case EXP_EvalPrimFun_If:
    {
        if ((len != 3) && (len != 4))
        {
            EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
            return;
        }
        EXP_EvalBlockCallback cb = { EXP_EvalBlockCallbackType_Branch };
        cb.branch[0] = elms + 2;
        if (3 == len)
        {
            cb.branch[1] = NULL;
        }
        else if (4 == len)
        {
            cb.branch[1] = elms + 3;
        }
        EXP_evalVerifEnterBlockWithCB(ctx, 1, elms + 1, node, cb);
        goto next;
    }
    case EXP_EvalPrimFun_Drop:
    {
        if (!dataStack->length)
        {
            EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalStack);
            return;
        }
        vec_pop(dataStack);
        goto next;
    }
    case EXP_EvalPrimFun_Blk:
    {
        // todo
        goto next;
    }
    default:
    {
        if (nativeFun != -1)
        {
            EXP_EvalNativeFunInfo* nativeFunInfo = ctx->nativeFunTable->data + nativeFun;
            assert(nativeFunInfo->call);
            EXP_EvalBlockCallback cb = { EXP_EvalBlockCallbackType_NativeFun, .nativeFun = nativeFun };
            EXP_evalVerifEnterBlockWithCB(ctx, len - 1, elms + 1, node, cb);
            goto next;
        }
        EXP_evalVerifErrorAtNode(ctx, call, EXP_EvalErrCode_EvalSyntax);
        break;
    }
    }
}







EXP_EvalError EXP_evalVerif
(
    EXP_Space* space, EXP_Node root,
    EXP_EvalValueTypeInfoTable* valueTypeTable, EXP_EvalNativeFunInfoTable* nativeFunTable,
    EXP_NodeSrcInfoTable* srcInfoTable
)
{
    EXP_EvalError error = { 0 };
    if (!EXP_isSeq(space, root))
    {
        return error;
    }
    EXP_EvalVerifContext ctx = EXP_newEvalVerifContext(space, valueTypeTable, nativeFunTable, srcInfoTable);
    u32 len = EXP_seqLen(space, root);
    EXP_Node* seq = EXP_seqElm(space, root);
    EXP_evalVerifEnterBlock(&ctx, len, seq, root);
    EXP_evalVerifCall(&ctx);
    error = ctx.error;
    EXP_evalVerifContextFree(&ctx);
    return error;
}

















































































































































