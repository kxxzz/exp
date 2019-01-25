#include "exp_eval_a.h"



//typedef struct EXP_EvalValueTypeDef
//{
//    int x;
//} EXP_EvalValueTypeDef;


typedef enum EXP_EvalBlockInfoState
{
    EXP_EvalBlockInfoState_Uninited = 0,
    EXP_EvalBlockInfoState_Initing,
    EXP_EvalBlockInfoState_Inited,
} EXP_EvalBlockInfoState;

typedef struct EXP_EvalBlockInfo
{
    EXP_EvalBlockInfoState state;
    u32 numIns;
    u32 numOuts;
    u32* inType;
    u32* outType;
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



typedef struct EXP_EvalVerifValue
{
    u32 type;
    EXP_Node lit;
} EXP_EvalVerifValue;

typedef vec_t(EXP_EvalVerifValue) EXP_EvalVerifDataStack;



typedef struct EXP_EvalVerifContext
{
    EXP_Space* space;
    EXP_EvalValueTypeInfoTable* valueTypeTable;
    EXP_EvalNativeFunInfoTable* nativeFunTable;
    EXP_NodeSrcInfoTable* srcInfoTable;
    EXP_EvalBlockInfoTable blockTable;
    EXP_EvalVerifDataStack dataStack;
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







typedef struct EXP_EvalVerifDef
{
    EXP_Node key;
    bool isVal;
    union
    {
        EXP_Node fun;
        EXP_EvalVerifValue val;
    };
} EXP_EvalVerifDef;



static bool EXP_evalVerifGetMatched(EXP_EvalVerifContext* ctx, const char* funName, EXP_EvalVerifDef* def)
{
    return false;
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








static bool EXP_evalVerifEnterBlock(EXP_EvalVerifContext* ctx, u32 len, EXP_Node* seq, EXP_Node srcNode)
{
    return true;
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






static bool EXP_evalVerifValueTypeConvert(EXP_EvalVerifContext* ctx, EXP_EvalVerifValue* v, u32 vt, EXP_Node srcNode)
{
    EXP_Space* space = ctx->space;
    if (v->type != vt)
    {
        EXP_EvalValFromStr fromStr = ctx->valueTypeTable->data[vt].fromStr;
        if (fromStr && (EXP_EvalPrimValueType_Tok == v->type))
        {
            u32 l = EXP_tokSize(space, v->lit);
            const char* s = EXP_tokCstr(space, v->lit);
            EXP_EvalValueData data;
            if (!fromStr(l, s, &data))
            {
                EXP_evalVerifErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
                return false;
            }
            v->type = vt;
        }
        else
        {
            EXP_evalVerifErrorAtNode(ctx, srcNode, EXP_EvalErrCode_EvalArgs);
            return false;
        }
    }
    return true;
}



static void EXP_evalVerifNativeFunCall
(
    EXP_EvalVerifContext* ctx, EXP_EvalNativeFunInfo* nativeFunInfo, EXP_Node srcNode
)
{
    EXP_Space* space = ctx->space;
    EXP_EvalVerifDataStack* dataStack = &ctx->dataStack;
    u32 argsOffset = dataStack->length - nativeFunInfo->numIns;
    for (u32 i = 0; i < nativeFunInfo->numIns; ++i)
    {
        EXP_EvalVerifValue* v = dataStack->data + argsOffset + i;
        u32 vt = nativeFunInfo->inType[i];
        if (!EXP_evalVerifValueTypeConvert(ctx, v, vt, srcNode))
        {
            return;
        }
    }
    vec_resize(dataStack, argsOffset);
    for (u32 i = 0; i < nativeFunInfo->numOuts; ++i)
    {
        EXP_EvalVerifValue v = { nativeFunInfo->outType[i] };
        vec_push(dataStack, v);
    }
}




static void EXP_evalVerifFunCall
(
    EXP_EvalVerifContext* ctx, EXP_EvalBlockInfo* blkInfo, EXP_Node srcNode
)
{
    EXP_Space* space = ctx->space;
    EXP_EvalVerifDataStack* dataStack = &ctx->dataStack;
    u32 argsOffset = dataStack->length - blkInfo->numIns;
    for (u32 i = 0; i < blkInfo->numIns; ++i)
    {
        EXP_EvalVerifValue* v = dataStack->data + argsOffset + i;
        u32 vt = blkInfo->inType[i];
        if (!EXP_evalVerifValueTypeConvert(ctx, v, vt, srcNode))
        {
            return;
        }
    }
    vec_resize(dataStack, argsOffset);
    for (u32 i = 0; i < blkInfo->numOuts; ++i)
    {
        EXP_EvalVerifValue v = { blkInfo->outType[i] };
        vec_push(dataStack, v);
    }
}




static void EXP_evalVerifCall(EXP_EvalVerifContext* ctx)
{
    EXP_Space* space = ctx->space;
    EXP_EvalVerifBlock* curBlock;
    EXP_EvalVerifDataStack* dataStack = &ctx->dataStack;
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
            EXP_evalVerifNativeFunCall(ctx, nativeFunInfo, curBlock->srcNode);
            break;
        }
        case EXP_EvalBlockCallbackType_Fun:
        {
            if (curBlock->dataStackP > dataStack->length)
            {
                EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
                return;
            }
            EXP_Node fun = cb->fun;
            EXP_EvalBlockInfo* blkInfo = ctx->blockTable.data + fun.id;
            assert(blkInfo->state > EXP_EvalBlockInfoState_Uninited);
            if (EXP_EvalBlockInfoState_Inited == blkInfo->state)
            {
                if (curBlock->dataStackP != (dataStack->length - blkInfo->numIns))
                {
                    EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
                    return;
                }
                if (!EXP_evalVerifLeaveBlock(ctx))
                {
                    return;
                }
                EXP_evalVerifFunCall(ctx, blkInfo, curBlock->srcNode);
                goto next;
            }
            else if (EXP_EvalBlockInfoState_Initing == blkInfo->state)
            {
                // todo
                assert(false);
            }
            return;
        }
        case EXP_EvalBlockCallbackType_Branch:
        {
            if (curBlock->dataStackP + 1 != dataStack->length)
            {
                EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
                return;
            }
            EXP_EvalVerifValue v = dataStack->data[curBlock->dataStackP];
            vec_pop(dataStack);
            if (!EXP_evalVerifValueTypeConvert(ctx, &v, EXP_EvalPrimValueType_Bool, curBlock->srcNode))
            {
                return;
            }
            if (!EXP_evalVerifLeaveBlock(ctx))
            {
                return;
            }
            if (EXP_evalVerifEnterBlock(ctx, 1, cb->branch[0], curBlock->srcNode))
            {
                goto next;
            }
            if (cb->branch[1])
            {
                if (EXP_evalVerifEnterBlock(ctx, 1, cb->branch[1], curBlock->srcNode))
                {
                    goto next;
                }
                return;
            }
            else
            {
                // todo
                assert(false);
                return;
            }
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
        const char* funName = EXP_tokCstr(space, node);
        u32 nativeFun = EXP_evalVerifGetNativeFun(ctx, funName);
        if (nativeFun != -1)
        {
            switch (nativeFun)
            {
            case EXP_EvalPrimFun_PopDefBegin:
            {
                for (;;)
                {
                    EXP_Node key = curBlock->seq[curBlock->p++];
                    const char* skey = EXP_tokCstr(space, key);
                    if (!EXP_isTok(space, key))
                    {
                        EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
                        return;
                    }
                    u32 nativeFun = EXP_evalVerifGetNativeFun(ctx, EXP_tokCstr(space, key));
                    if (nativeFun != -1)
                    {
                        if (EXP_EvalPrimFun_PopDefEnd == nativeFun)
                        {
                            goto next;
                        }
                        EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
                        return;
                    }
                    if (!dataStack->length)
                    {
                        EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalStack);
                        return;
                    }
                    // todo
                    //EXP_EvalValue val = vec_last(dataStack);
                    //EXP_EvalDef def = { key, true,.val = val };
                    //vec_push(&ctx->defStack, def);
                    vec_pop(dataStack);
                }
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
            default:
                break;
            }
            EXP_EvalNativeFunInfo* nativeFunInfo = ctx->nativeFunTable->data + nativeFun;
            if (!nativeFunInfo->call)
            {
                EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
                return;
            }
            if (dataStack->length < nativeFunInfo->numIns)
            {
                EXP_evalVerifErrorAtNode(ctx, node, EXP_EvalErrCode_EvalArgs);
                return;
            }
            EXP_evalVerifNativeFunCall(ctx, nativeFunInfo, node);
            goto next;
        }

        EXP_EvalVerifDef def = { 0 };
        if (EXP_evalVerifGetMatched(ctx, funName, &def))
        {
            if (def.isVal)
            {
                vec_push(dataStack, def.val);
                goto next;
            }
            else
            {
                EXP_Node fun = def.fun;
                EXP_EvalBlockInfo* blkInfo = ctx->blockTable.data + fun.id;
                assert(blkInfo->state > EXP_EvalBlockInfoState_Uninited);
                if (EXP_EvalBlockInfoState_Inited == blkInfo->state)
                {
                    if (dataStack->length < blkInfo->numIns)
                    {
                        EXP_evalVerifErrorAtNode(ctx, curBlock->srcNode, EXP_EvalErrCode_EvalArgs);
                        return;
                    }
                    EXP_evalVerifFunCall(ctx, blkInfo, node);
                    goto next;
                }
                else
                {
                    assert(false);
                    return;
                }
            }
        }
        else
        {
            EXP_EvalVerifValue v = { EXP_EvalPrimValueType_Tok, .lit = node };
            vec_push(dataStack, v);
            goto next;
        }
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
    EXP_EvalVerifDef def = { 0 };
    if (EXP_evalVerifGetMatched(ctx, funName, &def))
    {
        if (def.isVal)
        {
            vec_push(dataStack, def.val);
            goto next;
        }
        else
        {
            EXP_EvalBlockCallback cb = { EXP_EvalBlockCallbackType_Fun, .fun = def.fun };
            EXP_evalVerifEnterBlockWithCB(ctx, len - 1, elms + 1, node, cb);
            goto next;
        }
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
    return error;
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

















































































































































