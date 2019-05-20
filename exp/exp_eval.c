#include "exp_eval_a.h"





EXP_EvalContext* EXP_newEvalContext(const EXP_EvalAtomTable* addAtomTable)
{
    EXP_EvalContext* ctx = zalloc(sizeof(*ctx));
    EXP_Space* space = ctx->space = EXP_newSpace();
    EXP_EvalAtypeInfoVec* atypeTable = &ctx->atypeTable;
    EXP_EvalAfunInfoVec* afunTable = &ctx->afunTable;
    vec_u32* afunTypeTable = &ctx->afunTypeTable;
    EXP_EvalTypeContext* typeContext = ctx->typeContext = EXP_newEvalTypeContext();
    EXP_EvalObjectTable* objectTable = &ctx->objectTable;
    EXP_EvalCompileTypeDeclStack* typeDeclStack = &ctx->typeDeclStack;

    for (u32 i = 0; i < EXP_NumEvalPrimTypes; ++i)
    {
        vec_push(atypeTable, EXP_EvalPrimTypeInfoTable()[i]);
    }
    for (u32 i = 0; i < EXP_NumEvalPrimFuns; ++i)
    {
        const EXP_EvalAfunInfo* info = EXP_EvalPrimFunInfoTable() + i;
        vec_push(afunTable, *info);
        EXP_Node node = EXP_loadSrcAsList(space, info->typeDecl, NULL);
        assert(node.id != EXP_NodeId_Invalid);
        u32 t = EXP_evalCompileTypeDecl(space, atypeTable, typeContext, typeDeclStack, node, NULL, NULL);
        assert(t != -1);
        vec_push(afunTypeTable, t);
    }
    if (addAtomTable)
    {
        for (u32 i = 0; i < addAtomTable->numTypes; ++i)
        {
            vec_push(atypeTable, addAtomTable->types[i]);
        }
        for (u32 i = 0; i < addAtomTable->numFuns; ++i)
        {
            const EXP_EvalAfunInfo* info = addAtomTable->funs + i;
            vec_push(afunTable, *info);
            EXP_Node node = EXP_loadSrcAsList(space, info->typeDecl, NULL);
            assert(node.id != EXP_NodeId_Invalid);
            u32 t = EXP_evalCompileTypeDecl(space, atypeTable, typeContext, typeDeclStack, node, NULL, NULL);
            assert(t != -1);
            vec_push(afunTypeTable, t);
        }
    }

    ctx->objectTable = EXP_newEvalObjectTable(atypeTable);
    return ctx;
}




void EXP_evalContextFree(EXP_EvalContext* ctx)
{
    vec_free(&ctx->fileInfoTable);

    vec_free(&ctx->aryStack);
    EXP_evalCompileTypeDeclStackFree(&ctx->typeDeclStack);

    vec_free(&ctx->dataStack);
    vec_free(&ctx->varStack);
    vec_free(&ctx->callStack);
    vec_free(&ctx->typeStack);

    EXP_evalObjectTableFree(&ctx->objectTable, &ctx->atypeTable);
    vec_free(&ctx->nodeTable);

    vec_free(&ctx->afunTypeTable);
    vec_free(&ctx->afunTable);
    vec_free(&ctx->atypeTable);

    EXP_evalTypeContextFree(ctx->typeContext);
    EXP_spaceSrcInfoFree(&ctx->srcInfo);
    EXP_spaceFree(ctx->space);
    free(ctx);
}








EXP_EvalError EXP_evalLastError(EXP_EvalContext* ctx)
{
    return ctx->error;
}

EXP_EvalTypeContext* EXP_evalDataTypeContext(EXP_EvalContext* ctx)
{
    return ctx->typeContext;
}

vec_u32* EXP_evalDataTypeStack(EXP_EvalContext* ctx)
{
    return &ctx->typeStack;
}

EXP_EvalValueVec* EXP_evalDataStack(EXP_EvalContext* ctx)
{
    return &ctx->dataStack;
}




















void EXP_evalPushValue(EXP_EvalContext* ctx, u32 type, EXP_EvalValue val)
{
    vec_push(&ctx->typeStack, type);
    vec_push(&ctx->dataStack, val);
}

void EXP_evalDrop(EXP_EvalContext* ctx)
{
    assert(ctx->typeStack.length > 0);
    assert(ctx->dataStack.length > 0);
    assert(ctx->typeStack.length == ctx->dataStack.length);
    vec_pop(&ctx->typeStack);
    vec_pop(&ctx->dataStack);
}


















static void EXP_evalWordDefGetBody(EXP_EvalContext* ctx, EXP_Node node, u32* pLen, const EXP_Node** pSeq)
{
    EXP_Space* space = ctx->space;
    assert(EXP_seqLen(space, node) >= 2);
    *pLen = EXP_seqLen(space, node) - 2;
    const EXP_Node* exp = EXP_seqElm(space, node);
    *pSeq = exp + 2;
}













static EXP_EvalValue* EXP_evalGetVar(EXP_EvalContext* ctx, const EXP_EvalNodeVar* evar)
{
    EXP_Space* space = ctx->space;
    EXP_EvalNodeTable* nodeTable = &ctx->nodeTable;
    EXP_EvalCallStack* callStack = &ctx->callStack;
    EXP_EvalValueVec* varStack = &ctx->varStack;
    for (u32 i = 0; i < callStack->length; ++i)
    {
        EXP_EvalCall* call = callStack->data + callStack->length - 1 - i;
        EXP_EvalNode* blkEnode = nodeTable->data + call->srcNode.id;
        if (evar->block.id == call->srcNode.id)
        {
            u32 offset = call->varBase + evar->id;
            return &varStack->data[offset];
        }
    }
    return NULL;
}













static void EXP_evalEnterBlock(EXP_EvalContext* ctx, const EXP_Node* seq, u32 len, EXP_Node srcNode)
{
    EXP_EvalBlockCallback nocb = { EXP_EvalBlockCallbackType_NONE };
    EXP_EvalCall call = { srcNode, seq, seq + len, ctx->varStack.length, nocb };
    vec_push(&ctx->callStack, call);
}

static void EXP_evalEnterBlockWithCB
(
    EXP_EvalContext* ctx, const EXP_Node* seq, u32 len, EXP_Node srcNode, EXP_EvalBlockCallback cb
)
{
    assert(cb.type != EXP_EvalBlockCallbackType_NONE);
    EXP_EvalCall call = { srcNode, seq, seq + len, ctx->varStack.length, cb };
    vec_push(&ctx->callStack, call);
}

static bool EXP_evalLeaveBlock(EXP_EvalContext* ctx)
{
    EXP_EvalCall* curCall = &vec_last(&ctx->callStack);
    EXP_EvalNode* enode = ctx->nodeTable.data + curCall->srcNode.id;
    assert(ctx->varStack.length >= enode->varsCount);
    vec_resize(&ctx->varStack, curCall->varBase);
    vec_pop(&ctx->callStack);
    return ctx->callStack.length > 0;
}





static bool EXP_evalAtTail(EXP_EvalContext* ctx)
{
    EXP_EvalCall* curCall = &vec_last(&ctx->callStack);
    if (curCall->cb.type != EXP_EvalBlockCallbackType_NONE)
    {
        return false;
    }
    return curCall->p == curCall->end;
}












static void EXP_evalAfunCall
(
    EXP_EvalContext* ctx, EXP_EvalAfunInfo* afunInfo, EXP_Node srcNode, EXP_EvalNode* srcEnode
)
{
    EXP_Space* space = ctx->space;
    EXP_EvalValueVec* dataStack = &ctx->dataStack;
    EXP_EvalTypeContext* typeContext = ctx->typeContext;

    u32 argsOffset = dataStack->length - srcEnode->numIns;
    memset(ctx->ncallOutBuf, 0, sizeof(ctx->ncallOutBuf));
    afunInfo->call
    (
        space, dataStack->data + argsOffset, ctx->ncallOutBuf,
        afunInfo->mode >= EXP_EvalAfunMode_ContextDepend ? ctx : NULL
    );
    if (afunInfo->mode < EXP_EvalAfunMode_HighOrder)
    {
        vec_resize(dataStack, argsOffset);
        for (u32 i = 0; i < srcEnode->numOuts; ++i)
        {
            EXP_EvalValue v = ctx->ncallOutBuf[i];
            vec_push(dataStack, v);
        }
    }
}









static void EXP_evalTailCallElimination(EXP_EvalContext* ctx)
{
    while (EXP_evalAtTail(ctx))
    {
        if (!EXP_evalLeaveBlock(ctx))
        {
            break;
        }
    }
}












static void EXP_evalCall(EXP_EvalContext* ctx)
{
    EXP_Space* space = ctx->space;
    EXP_EvalCall* curCall;
    EXP_EvalNodeTable* nodeTable = &ctx->nodeTable;
    EXP_EvalValueVec* dataStack = &ctx->dataStack;
    EXP_EvalTypeContext* typeContext = ctx->typeContext;
    EXP_EvalAtypeInfoVec* atypeTable = &ctx->atypeTable;
    EXP_EvalArrayPtrVec* aryStack = &ctx->aryStack;
    bool r;
next:
    if (ctx->error.code)
    {
        return;
    }
    curCall = &vec_last(&ctx->callStack);
    if (curCall->p == curCall->end)
    {
        EXP_EvalBlockCallback* cb = &curCall->cb;
        switch (cb->type)
        {
        case EXP_EvalBlockCallbackType_NONE:
        {
            break;
        }
        case EXP_EvalBlockCallbackType_Ncall:
        {
            EXP_EvalAfunInfo* afunInfo = ctx->afunTable.data + cb->afun;
            EXP_EvalNode* srcEnode = nodeTable->data + curCall->srcNode.id;
            EXP_evalAfunCall(ctx, afunInfo, curCall->srcNode, srcEnode);
            break;
        }
        case EXP_EvalBlockCallbackType_CallWord:
        {
            EXP_Node blkSrc = cb->blkSrc;
            u32 bodyLen = 0;
            EXP_Node* body = NULL;
            EXP_evalWordDefGetBody(ctx, blkSrc, &bodyLen, &body);
            if (!EXP_evalLeaveBlock(ctx))
            {
                goto next;
            }
            EXP_evalTailCallElimination(ctx);
            EXP_evalEnterBlock(ctx, body, bodyLen, blkSrc);
            goto next;
        }
        case EXP_EvalBlockCallbackType_CallVar:
        {
            EXP_Node blkSrc = cb->blkSrc;
            u32 bodyLen = EXP_seqLen(space, blkSrc);
            const EXP_Node* body = EXP_seqElm(space, blkSrc);
            if (!EXP_evalLeaveBlock(ctx))
            {
                goto next;
            }
            EXP_evalTailCallElimination(ctx);
            EXP_evalEnterBlock(ctx, body, bodyLen, blkSrc);
            goto next;
        }
        case EXP_EvalBlockCallbackType_Cond:
        {
            EXP_EvalValue v = vec_last(dataStack);
            vec_pop(dataStack);
            if (!EXP_evalLeaveBlock(ctx))
            {
                goto next;
            }
            EXP_Node srcNode = curCall->srcNode;
            if (v.b)
            {
                EXP_evalEnterBlock(ctx, EXP_evalIfBranch0(space, srcNode), 1, srcNode);
                goto next;
            }
            EXP_evalEnterBlock(ctx, EXP_evalIfBranch1(space, srcNode), 1, srcNode);
            goto next;
        }
        case EXP_EvalBlockCallbackType_ArrayMap:
        {
            EXP_Node srcNode = curCall->srcNode;

            assert(aryStack->length >= 2);
            EXP_EvalArray* src = aryStack->data[aryStack->length - 2];
            EXP_EvalArray* dst = aryStack->data[aryStack->length - 1];

            u32 pos = cb->pos;
            assert(EXP_evalArraySize(dst) == EXP_evalArraySize(src));
            u32 size = EXP_evalArraySize(src);

            EXP_EvalNode* enode = nodeTable->data + srcNode.id;
            assert(EXP_evalArrayElmSize(src) == enode->numIns);
            u32 dstElmSize = enode->numOuts;
            assert(dataStack->length >= dstElmSize);

            EXP_EvalValue* inBuf = dataStack->data + dataStack->length - dstElmSize;
            r = EXP_evalArraySetElm(dst, pos, inBuf);
            assert(r);
            vec_resize(dataStack, dataStack->length - dstElmSize);

            pos = ++cb->pos;
            if (pos < size)
            {
                u32 srcElmSize = EXP_evalArrayElmSize(src);
                EXP_EvalValue* outBuf = dataStack->data + dataStack->length;
                vec_resize(dataStack, dataStack->length + srcElmSize);
                r = EXP_evalArrayGetElm(src, pos, outBuf);
                assert(r);

                assert(EXP_isSeqSquare(space, srcNode));
                u32 bodyLen = EXP_seqLen(space, srcNode);
                curCall->p -= bodyLen;
            }
            else
            {
                EXP_EvalValue v = { .ary = dst, EXP_EvalValueType_Object };
                vec_push(dataStack, v);

                vec_resize(aryStack, aryStack->length - 2);
                EXP_evalLeaveBlock(ctx);
            }
            goto next;
        }
        case EXP_EvalBlockCallbackType_ArrayFilter:
        {
            assert(aryStack->length >= 2);
            EXP_EvalArray* src = aryStack->data[aryStack->length - 2];
            EXP_EvalArray* dst = aryStack->data[aryStack->length - 1];

            u32 pos = cb->pos;
            u32 size = EXP_evalArraySize(src);
            assert(EXP_evalArrayElmSize(src) == EXP_evalArrayElmSize(dst));
            u32 elmSize = EXP_evalArrayElmSize(src);

            assert(dataStack->length >= 1);
            if (vec_last(dataStack).b)
            {
                EXP_EvalValue* elm = dataStack->data + dataStack->length;
                vec_resize(dataStack, dataStack->length + elmSize);
                bool r = EXP_evalArrayGetElm(src, pos, elm);
                assert(r);
                EXP_evalArrayPushElm(dst, elm);
                vec_resize(dataStack, dataStack->length - elmSize);
            }
            vec_pop(dataStack);
            pos = ++cb->pos;
            if (pos < size)
            {
                EXP_EvalValue* elm = dataStack->data + dataStack->length;
                vec_resize(dataStack, dataStack->length + elmSize);
                bool r = EXP_evalArrayGetElm(src, pos, elm);
                assert(r);

                EXP_Node srcNode = curCall->srcNode;
                assert(EXP_isSeqSquare(space, srcNode));
                u32 bodyLen = EXP_seqLen(space, srcNode);
                curCall->p -= bodyLen;
            }
            else
            {
                EXP_EvalValue v = { .ary = dst, EXP_EvalValueType_Object };
                vec_push(dataStack, v);

                vec_resize(aryStack, aryStack->length - 2);
                EXP_evalLeaveBlock(ctx);
            }
            goto next;
        }
        case EXP_EvalBlockCallbackType_ArrayReduce:
        {
            assert(aryStack->length >= 1);
            EXP_EvalArray* src = aryStack->data[aryStack->length - 1];

            u32 pos = cb->pos;
            u32 size = EXP_evalArraySize(src);
            u32 elmSize = EXP_evalArrayElmSize(src);
            assert(dataStack->length >= elmSize);

            pos = ++cb->pos;
            if (pos < size)
            {
                EXP_EvalValue* elm = dataStack->data + dataStack->length;
                vec_resize(dataStack, dataStack->length + elmSize);
                bool r = EXP_evalArrayGetElm(src, pos, elm);
                assert(r);

                EXP_Node srcNode = curCall->srcNode;
                assert(EXP_isSeqSquare(space, srcNode));
                u32 bodyLen = EXP_seqLen(space, srcNode);
                curCall->p -= bodyLen;
            }
            else
            {
                vec_resize(aryStack, aryStack->length - 1);
                EXP_evalLeaveBlock(ctx);
            }
            goto next;
        }
        default:
            assert(false);
            goto next;
        }
        if (EXP_evalLeaveBlock(ctx))
        {
            goto next;
        }
        return;
    }
    EXP_Node node = *(curCall->p++);
#ifndef NDEBUG
    const EXP_NodeSrcInfo* nodeSrcInfo = EXP_nodeSrcInfo(&ctx->srcInfo, node);
#endif
    EXP_EvalNode* enode = nodeTable->data + node.id;
    switch (enode->type)
    {
    case EXP_EvalNodeType_VarDefBegin:
    {
        assert(EXP_isTok(space, node));
        assert(EXP_EvalBlockCallbackType_NONE == curCall->cb.type);
        const EXP_Node* begin = curCall->p;
        for (u32 n = 0;;)
        {
            assert(curCall->p < curCall->end);
            node = *(curCall->p++);
            if (EXP_isTok(space, node))
            {
                enode = nodeTable->data + node.id;
                if (EXP_EvalNodeType_VarDefEnd == enode->type)
                {
                    assert(n <= dataStack->length);
                    u32 off = dataStack->length - n;
                    for (u32 i = 0; i < n; ++i)
                    {
                        EXP_EvalValue val = dataStack->data[off + i];
                        vec_push(&ctx->varStack, val);
                    }
                    vec_resize(dataStack, off);
                    goto next;
                }
                else
                {
                    assert(EXP_EvalNodeType_None == enode->type);
                }
                ++n;
            }
        }
    }
    case EXP_EvalNodeType_GC:
    {
        EXP_evalGC(ctx);
        goto next;
    }
    case EXP_EvalNodeType_Apply:
    {
        EXP_EvalValue v = vec_last(dataStack);
        vec_pop(dataStack);
        EXP_Node blkSrc = v.src;
        assert(EXP_isSeqSquare(space, blkSrc));
        u32 bodyLen = EXP_seqLen(space, blkSrc);
        const EXP_Node* body = EXP_seqElm(space, blkSrc);
        EXP_evalEnterBlock(ctx, body, bodyLen, blkSrc);
        goto next;
    }
    case EXP_EvalNodeType_BlockExe:
    {
        assert(EXP_isSeqCurly(space, node));
        u32 bodyLen = EXP_seqLen(space, node);
        const EXP_Node* body = EXP_seqElm(space, node);
        EXP_evalEnterBlock(ctx, body, bodyLen, node);
        goto next;
    }
    case EXP_EvalNodeType_Afun:
    {
        assert(EXP_isTok(space, node));
        assert(dataStack->length >= enode->numIns);
        EXP_EvalAfunInfo* afunInfo = ctx->afunTable.data + enode->afun;
        assert(afunInfo->call);
        EXP_evalAfunCall(ctx, afunInfo, node, enode);
        goto next;
    }
    case EXP_EvalNodeType_Var:
    {
        assert(EXP_isTok(space, node));
        EXP_EvalValue* val = EXP_evalGetVar(ctx, &enode->var);
        vec_push(dataStack, *val);
        goto next;
    }
    case EXP_EvalNodeType_Word:
    {
        assert(EXP_isTok(space, node));
        EXP_Node blkSrc = enode->blkSrc;
        EXP_Node* body = NULL;
        u32 bodyLen = 0;
        EXP_evalWordDefGetBody(ctx, blkSrc, &bodyLen, &body);
        EXP_evalEnterBlock(ctx, body, bodyLen, blkSrc);
        goto next;
    }
    case EXP_EvalNodeType_Atom:
    {
        assert(EXP_isTok(space, node));
        u32 l = EXP_tokSize(space, node);
        const char* s = EXP_tokCstr(space, node);
        EXP_EvalValue v = EXP_evalNewAtom(ctx, s, l, enode->atype);
        vec_push(dataStack, v);
        goto next;
    }
    case EXP_EvalNodeType_Block:
    {
        assert(EXP_isSeqSquare(space, node));
        EXP_EvalValue v = { 0 };
        v.src = enode->blkSrc;
        vec_push(dataStack, v);
        goto next;
    }
    case EXP_EvalNodeType_CallAfun:
    {
        assert(EXP_evalCheckCall(space, node));
        const EXP_Node* elms = EXP_seqElm(space, node);
        u32 len = EXP_seqLen(space, node);
        u32 afun = enode->afun;
        assert(afun != -1);
        EXP_EvalAfunInfo* afunInfo = ctx->afunTable.data + afun;
        assert(afunInfo->call);
        EXP_EvalBlockCallback cb = { EXP_EvalBlockCallbackType_Ncall, .afun = afun };
        EXP_evalEnterBlockWithCB(ctx, elms + 1, len - 1, node, cb);
        goto next;
    }
    case EXP_EvalNodeType_CallWord:
    {
        assert(EXP_evalCheckCall(space, node));
        const EXP_Node* elms = EXP_seqElm(space, node);
        u32 len = EXP_seqLen(space, node);
        EXP_EvalBlockCallback cb = { EXP_EvalBlockCallbackType_CallWord, .blkSrc = enode->blkSrc };
        EXP_evalEnterBlockWithCB(ctx, elms + 1, len - 1, node, cb);
        goto next;
    }
    case EXP_EvalNodeType_CallVar:
    {
        assert(EXP_evalCheckCall(space, node));
        EXP_EvalValue* val = EXP_evalGetVar(ctx, &enode->var);
        EXP_Node blkSrc = val->src;
        assert(EXP_isSeqSquare(space, blkSrc));
        const EXP_Node* elms = EXP_seqElm(space, node);
        u32 len = EXP_seqLen(space, node);
        EXP_EvalBlockCallback cb = { EXP_EvalBlockCallbackType_CallVar, .blkSrc = blkSrc };
        EXP_evalEnterBlockWithCB(ctx, elms + 1, len - 1, node, cb);
        goto next;
    }
    case EXP_EvalNodeType_Def:
    {
        assert(EXP_evalCheckCall(space, node));
        assert(EXP_EvalBlockCallbackType_NONE == curCall->cb.type);
        goto next;
    }
    case EXP_EvalNodeType_If:
    {
        assert(EXP_evalCheckCall(space, node));
        const EXP_Node* elms = EXP_seqElm(space, node);
        u32 len = EXP_seqLen(space, node);
        assert(4 == len);
        EXP_EvalBlockCallback cb = { EXP_EvalBlockCallbackType_Cond };
        EXP_evalEnterBlockWithCB(ctx, elms + 1, 1, node, cb);
        goto next;
    }
    default:
        assert(false);
    }
}































void EXP_evalAfunCall_Array(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs, EXP_EvalContext* ctx)
{
    u32 size = (u32)ins[0].f;
    outs[0] = EXP_evalNewArray(ctx, size);
}



void EXP_evalAfunCall_AtLoad(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs, EXP_EvalContext* ctx)
{
    EXP_EvalArray* ary = ins[0].ary;
    u32 pos = (u32)ins[1].f;
    bool r = EXP_evalArrayGetElm(ary, pos, outs);
    if (!r)
    {
        u32 elmSize = EXP_evalArrayElmSize(ary);
        memset(outs, 0, elmSize * sizeof(EXP_EvalValue));
    }
}



void EXP_evalAfunCall_AtSave(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs, EXP_EvalContext* ctx)
{
    EXP_EvalArray* ary = ins[0].ary;
    u32 pos = (u32)ins[1].f;
    EXP_evalArraySetElm(ary, pos, ins + 2);
}



void EXP_evalAfunCall_Map(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs, EXP_EvalContext* ctx)
{
    EXP_EvalValueVec* dataStack = &ctx->dataStack;
    EXP_EvalArrayPtrVec* aryStack = &ctx->aryStack;

    assert(EXP_EvalValueType_Object == ins[0].type);
    assert(EXP_EvalValueType_Inline == ins[1].type);

    EXP_EvalArray* src = ins[0].ary;
    vec_push(aryStack, src);
    u32 size = EXP_evalArraySize(src);
    EXP_EvalArray* dst = EXP_evalNewArray(ctx, size).ary;
    vec_push(aryStack, dst);

    EXP_Node blkSrc = ins[1].src;
    assert(EXP_isSeqSquare(space, blkSrc));
    u32 bodyLen = EXP_seqLen(space, blkSrc);
    const EXP_Node* body = EXP_seqElm(space, blkSrc);

    u32 elmSize = EXP_evalArrayElmSize(src);
    EXP_EvalValue* outBuf = dataStack->data + dataStack->length - 2;
    vec_resize(dataStack, dataStack->length - 2 + elmSize);
    bool r = EXP_evalArrayGetElm(src, 0, outBuf);
    assert(r);

    EXP_EvalBlockCallback cb = { EXP_EvalBlockCallbackType_ArrayMap, .pos = 0 };
    EXP_evalEnterBlockWithCB(ctx, body, bodyLen, blkSrc, cb);
}




void EXP_evalAfunCall_Filter(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs, EXP_EvalContext* ctx)
{
    EXP_EvalValueVec* dataStack = &ctx->dataStack;
    EXP_EvalArrayPtrVec* aryStack = &ctx->aryStack;

    assert(EXP_EvalValueType_Object == ins[0].type);
    assert(EXP_EvalValueType_Inline == ins[1].type);

    EXP_EvalArray* src = ins[0].ary;
    vec_push(aryStack, src);
    EXP_EvalArray* dst = EXP_evalNewArray(ctx, 0).ary;
    vec_push(aryStack, dst);

    EXP_Node blkSrc = ins[1].src;
    assert(EXP_isSeqSquare(space, blkSrc));
    u32 bodyLen = EXP_seqLen(space, blkSrc);
    const EXP_Node* body = EXP_seqElm(space, blkSrc);

    u32 elmSize = EXP_evalArrayElmSize(src);
    EXP_EvalValue* outBuf = dataStack->data + dataStack->length - 2;
    vec_resize(dataStack, dataStack->length - 2 + elmSize);
    bool r = EXP_evalArrayGetElm(src, 0, outBuf);
    assert(r);

    EXP_EvalBlockCallback cb = { EXP_EvalBlockCallbackType_ArrayFilter, .pos = 0 };
    EXP_evalEnterBlockWithCB(ctx, body, bodyLen, blkSrc, cb);
}




void EXP_evalAfunCall_Reduce(EXP_Space* space, EXP_EvalValue* ins, EXP_EvalValue* outs, EXP_EvalContext* ctx)
{
    EXP_EvalValueVec* dataStack = &ctx->dataStack;
    EXP_EvalArrayPtrVec* aryStack = &ctx->aryStack;

    assert(EXP_EvalValueType_Object == ins[0].type);
    assert(EXP_EvalValueType_Inline == ins[1].type);

    EXP_EvalArray* src = ins[0].ary;
    vec_push(aryStack, src);

    EXP_Node blkSrc = ins[1].src;
    assert(EXP_isSeqSquare(space, blkSrc));
    u32 bodyLen = EXP_seqLen(space, blkSrc);
    const EXP_Node* body = EXP_seqElm(space, blkSrc);

    u32 elmSize = EXP_evalArrayElmSize(src);
    EXP_EvalValue* outBuf = dataStack->data + dataStack->length - 2;
    vec_resize(dataStack, dataStack->length - 2 + elmSize);
    bool r = EXP_evalArrayGetElm(src, 0, outBuf);
    assert(r);

    EXP_EvalBlockCallback cb = { EXP_EvalBlockCallbackType_ArrayReduce, .pos = 0 };
    EXP_evalEnterBlockWithCB(ctx, body, bodyLen, blkSrc, cb);
}































void EXP_evalBlock(EXP_EvalContext* ctx, EXP_Node root)
{
    EXP_Space* space = ctx->space;
    u32 len = EXP_seqLen(space, root);
    const EXP_Node* seq = EXP_seqElm(space, root);
    EXP_evalEnterBlock(ctx, seq, len, root);
    EXP_evalCall(ctx);
}









EXP_EvalError EXP_evalCompile
(
    EXP_Space* space, EXP_Node root, EXP_SpaceSrcInfo* srcInfo,
    EXP_EvalAtypeInfoVec* atypeTable, EXP_EvalAfunInfoVec* afunTable, vec_u32* afunTypeTable,
    EXP_EvalNodeTable* nodeTable,
    EXP_EvalTypeContext* typeContext, vec_u32* typeStack
);



bool EXP_evalCode(EXP_EvalContext* ctx, const char* filename, const char* code, bool enableSrcInfo)
{
    if (enableSrcInfo)
    {
        EXP_EvalFileInfo fileInfo = { 0 };
        stzncpy(fileInfo.name, filename, EXP_EvalFileName_MAX);
        vec_push(&ctx->fileInfoTable, fileInfo);
    }
    EXP_SpaceSrcInfo* srcInfo = NULL;
    if (enableSrcInfo)
    {
        srcInfo = &ctx->srcInfo;
    }
    EXP_Space* space = ctx->space;
    EXP_Node root = EXP_loadSrcAsList(space, code, srcInfo);
    if (EXP_NodeId_Invalid == root.id)
    {
        ctx->error.code = EXP_EvalErrCode_ExpSyntax;
        ctx->error.file = 0;
        if (srcInfo)
        {
#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable : 6011)
#endif
            ctx->error.line = vec_last(&srcInfo->nodes).line;
            ctx->error.column = vec_last(&srcInfo->nodes).column;
#ifdef _MSC_VER
# pragma warning(pop)
#endif
        }
        else
        {
            ctx->error.line = -1;
            ctx->error.column = -1;
        }
        return false;
    }


    if (!EXP_isSeq(space, root))
    {
        ctx->error.code = EXP_EvalErrCode_EvalSyntax;
        ctx->error.file = srcInfo->nodes.data[root.id].file;
        ctx->error.line = srcInfo->nodes.data[root.id].line;
        ctx->error.column = srcInfo->nodes.data[root.id].column;
        return false;
    }
    EXP_EvalError error = EXP_evalCompile
    (
        space, root, &ctx->srcInfo,
        &ctx->atypeTable, &ctx->afunTable, &ctx->afunTypeTable,
        &ctx->nodeTable,
        ctx->typeContext, &ctx->typeStack
    );
    if (error.code)
    {
        ctx->error = error;
        return false;
    }
    EXP_evalBlock(ctx, root);
    if (ctx->error.code)
    {
        return false;
    }
    return true;
}



bool EXP_evalFile(EXP_EvalContext* ctx, const char* fileName, bool enableSrcInfo)
{
    char* code = NULL;
    u32 codeSize = FILEU_readFile(fileName, &code);
    if (-1 == codeSize)
    {
        ctx->error.code = EXP_EvalErrCode_SrcFile;
        ctx->error.file = 0;
        return false;
    }
    if (0 == codeSize)
    {
        return false;
    }
    bool r = EXP_evalCode(ctx, fileName, code, enableSrcInfo);
    free(code);
    return r;
}

























const EXP_EvalFileInfoTable* EXP_evalFileInfoTable(EXP_EvalContext* ctx)
{
    return &ctx->fileInfoTable;
}













const char** EXP_EvalErrCodeNameTable(void)
{
    static const char* a[EXP_NumEvalErrorCodes] =
    {
        "NONE",
        "SrcFile",
        "ExpSyntax",
        "EvalSyntax",
        "EvalUnkWord",
        "EvalUnkCall",
        "EvalUnkFunType",
        "EvalUnkTypeDecl",
        "EvalArgs",
        "EvalBranchUneq",
        "EvalRecurNoBaseCase",
        "EvalUnification",
        "EvalAtomCtorByStr",
        "EvalTypeUnsolvable",
    };
    return a;
}












































































































