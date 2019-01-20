#include "exp_a.h"



typedef struct EXP_NodeInfo
{
    EXP_NodeType type;
    u32 offset;
    u32 length;
} EXP_NodeInfo;

typedef vec_t(EXP_NodeInfo) EXP_NodeInfoVec;


typedef vec_t(EXP_Node) EXP_NodeVec;


typedef struct EXP_Space
{
    EXP_NodeInfoVec nodes;
    vec_char toks;
    EXP_NodeVec seqs;
    EXP_NodeVec seqDefStack;
    vec_u32 seqDefStackFrames;
} EXP_Space;






EXP_Space* EXP_newSpace(void)
{
    EXP_Space* space = zalloc(sizeof(*space));
    return space;
}

void EXP_spaceFree(EXP_Space* space)
{
    vec_free(&space->seqDefStackFrames);
    vec_free(&space->seqDefStack);
    vec_free(&space->seqs);
    vec_free(&space->toks);
    vec_free(&space->nodes);
    free(space);
}







EXP_NodeType EXP_nodeType(EXP_Space* space, EXP_Node node)
{
    EXP_NodeInfo* info = space->nodes.data + node.id;
    return info->type;
}









EXP_Node EXP_addTok(EXP_Space* space, const char* str)
{
    u32 len = (u32)strlen(str);
    EXP_NodeInfo info = { EXP_NodeType_Tok, space->toks.length, len };
    vec_pusharr(&space->toks, str, len + 1);
    EXP_Node node = { space->nodes.length };
    vec_push(&space->nodes, info);
    return node;
}

EXP_Node EXP_addTokL(EXP_Space* space, u32 len, const char* str)
{
    EXP_NodeInfo info = { EXP_NodeType_Tok, space->toks.length, len };
    vec_pusharr(&space->toks, str, len);
    vec_push(&space->toks, 0);
    EXP_Node node = { space->nodes.length };
    vec_push(&space->nodes, info);
    return node;
}





void EXP_addSeqEnter(EXP_Space* space)
{
    vec_push(&space->seqDefStackFrames, space->seqDefStack.length);
}

void EXP_addSeqPush(EXP_Space* space, EXP_Node x)
{
    vec_push(&space->seqDefStack, x);
}

void EXP_addSeqCancel(EXP_Space* space)
{
    assert(space->seqDefStackFrames.length > 0);
    u32 frameP = vec_last(&space->seqDefStackFrames);
    vec_pop(&space->seqDefStackFrames);
    vec_resize(&space->seqDefStack, frameP);
}

EXP_Node EXP_addSeqDone(EXP_Space* space)
{
    assert(space->seqDefStackFrames.length > 0);
    u32 frameP = vec_last(&space->seqDefStackFrames);
    vec_pop(&space->seqDefStackFrames);
    u32 lenSeq = space->seqDefStack.length - frameP;
    EXP_Node* seq = space->seqDefStack.data + frameP;
    EXP_NodeInfo seqInfo = { EXP_NodeType_Seq, space->seqs.length, lenSeq };
    vec_pusharr(&space->seqs, seq, lenSeq);
    vec_resize(&space->seqDefStack, frameP);
    EXP_Node node = { space->nodes.length };
    vec_push(&space->nodes, seqInfo);
    return node;
}





void EXP_undoAdd1(EXP_Space* space)
{
    assert(space->nodes.length > 0);
    EXP_NodeInfo* info = &vec_last(&space->nodes);
    switch (info->type)
    {
    case EXP_NodeType_Tok:
    {
        vec_resize(&space->toks, space->toks.length - info->length - 1);
        assert(space->toks.length == info->offset);
        break;
    }
    case EXP_NodeType_Seq:
    {
        vec_resize(&space->seqs, space->seqs.length - info->length);
        assert(space->seqs.length == info->offset);
        break;
    }
    default:
        assert(false);
        break;
    }
    vec_pop(&space->nodes);
}

void EXP_undoAdd(EXP_Space* space, u32 n)
{
    for (u32 i = 0; i < n; ++i)
    {
        EXP_undoAdd1(space);
    }
}







u32 EXP_tokSize(EXP_Space* space, EXP_Node node)
{
    EXP_NodeInfo* info = space->nodes.data + node.id;
    assert(EXP_NodeType_Tok == info->type);
    return info->length;
}

const char* EXP_tokCstr(EXP_Space* space, EXP_Node node)
{
    EXP_NodeInfo* info = space->nodes.data + node.id;
    assert(EXP_NodeType_Tok == info->type);
    return space->toks.data + info->offset;
}




u32 EXP_seqLen(EXP_Space* space, EXP_Node node)
{
    EXP_NodeInfo* info = space->nodes.data + node.id;
    assert(EXP_NodeType_Seq == info->type);
    return info->length;
}

EXP_Node* EXP_seqElm(EXP_Space* space, EXP_Node node)
{
    EXP_NodeInfo* info = space->nodes.data + node.id;
    assert(EXP_NodeType_Seq == info->type);
    return space->seqs.data + info->offset;
}


















static u32 EXP_saveSeqSL
(
    const EXP_Space* space, const EXP_NodeInfo* seqInfo, char* buf, u32 bufSize,
    const EXP_NodeSrcInfoTable* srcInfoTable
)
{
    u32 n = 0;
    u32 bufRemain = bufSize;
    char* bufPtr = buf;

    for (u32 i = 0; i < seqInfo->length; ++i)
    {
        u32 en = EXP_saveSL(space, space->seqs.data[seqInfo->offset + i], bufPtr, bufRemain, srcInfoTable);
        if (en < bufRemain)
        {
            bufRemain -= en;
            bufPtr += en;
        }
        else
        {
            bufRemain = 0;
            bufPtr = NULL;
        }
        n += en;

        if (i < seqInfo->length - 1)
        {
            if (1 < bufRemain)
            {
                *bufPtr = ' ';
                bufRemain -= 1;
                bufPtr += 1;
            }
            else
            {
                bufRemain = 0;
                bufPtr = NULL;
            }
            n += 1;
        }
    }
    return n;
}


u32 EXP_saveSL
(
    const EXP_Space* space, EXP_Node node, char* buf, u32 bufSize,
    const EXP_NodeSrcInfoTable* srcInfoTable
)
{
    EXP_NodeInfo* info = space->nodes.data + node.id;
    switch (info->type)
    {
    case EXP_NodeType_Tok:
    {
        const char* str = space->toks.data + info->offset;
        u32 sreLen = info->length;
        u32 n;
        bool isQuotStr = false;
        if (srcInfoTable && (node.id < srcInfoTable->length))
        {
            isQuotStr = srcInfoTable->data[node.id].isQuotStr;
        }
        else
        {
            for (u32 i = 0; i < sreLen; ++i)
            {
                if (strchr("[]\"' \t\n\r\b\f", str[i]))
                {
                    isQuotStr = true;
                    break;
                }
            }
        }
        if (isQuotStr)
        {
            u32 l = 2;
            for (u32 i = 0; i < sreLen; ++i)
            {
                if (' ' >= str[i])
                {
                    ++l;
                }
                else if (strchr("[]\"'", str[i]))
                {
                    ++l;
                }
                ++l;
            }
            if (buf && (l < bufSize))
            {
                n = 0;
                buf[n++] = '"';
                for (u32 i = 0; i < sreLen; ++i)
                {
                    if (' ' >= str[i])
                    {
                        buf[n++] = '\\';
                    }
                    else if (strchr("[]\"'", str[i]))
                    {
                        buf[n++] = '\\';
                    }
                    buf[n++] = str[i];
                }
                buf[n++] = '"';
                assert(n == l);
            }
            else
            {
                n = l;
            }
        }
        else
        {
            n = snprintf(buf, bufSize, "%s", str);
        }
        return n;
    }
    case EXP_NodeType_Seq:
    {
        u32 n = 0;

        u32 bufRemain = bufSize;
        char* bufPtr = buf;

        if (1 < bufRemain)
        {
            *bufPtr = '[';
            bufRemain -= 1;
            bufPtr += 1;
        }
        else
        {
            bufRemain = 0;
            bufPtr = NULL;
        }
        n += 1;

        u32 n1 = EXP_saveSeqSL(space, info, bufPtr, bufRemain, srcInfoTable);
        if (n1 < bufRemain)
        {
            bufRemain -= n1;
            bufPtr += n1;
        }
        else
        {
            bufRemain = 0;
            bufPtr = NULL;
        }
        n += n1;

        if (1 < bufRemain)
        {
            *bufPtr = ']';
            *(bufPtr + 1) = 0;
            bufRemain -= 1;
            bufPtr += 1;
        }
        else
        {
            bufRemain = 0;
            bufPtr = NULL;
        }
        n += 1;
        return n;
    }
    default:
        assert(false);
        return 0;
    }
}















typedef struct EXP_SaveMLctx
{
    const EXP_Space* space;
    const EXP_SaveMLopt* opt;
    const u32 bufSize;
    char* const buf;

    u32 n;
    u32 column;
    u32 depth;
} EXP_SaveMLctx;



static bool EXP_saveMlForward(EXP_SaveMLctx* ctx, u32 a)
{
    ctx->n += a;
    ctx->column += a;
    return ctx->column <= ctx->opt->width;
}

static void EXP_saveMlBack(EXP_SaveMLctx* ctx, u32 a)
{
    assert(ctx->n >= a);
    assert(ctx->column >= a);
    ctx->n -= a;
    ctx->column -= a;
}




static void EXP_saveMlAdd(EXP_SaveMLctx* ctx, const char* s)
{
    u32 a = (u32)strlen(s);
    u32 bufRemain = (ctx->bufSize > ctx->n) ? (ctx->bufSize - ctx->n) : 0;
    if (bufRemain > 0)
    {
        assert(ctx->buf);
        u32 wn = min(bufRemain - 1, a);
        if (wn > 0)
        {
            char* bufPtr = ctx->buf + ctx->n;
            stzncpy(bufPtr, s, wn + 1);
            bufPtr[wn] = 0;
        }
    }
    ctx->n += a;
    u32 ca = 0;
    for (u32 i = 0; i < a; ++i)
    {
        if ('\n' == s[a - 1 - i])
        {
            ctx->column = 0;
            break;
        }
        else
        {
            ++ca;
        }
    }
    ctx->column += ca;
}



static void EXP_saveMlAddIdent(EXP_SaveMLctx* ctx)
{
    u32 n = ctx->opt->indent * ctx->depth;
    for (u32 i = 0; i < n; ++i)
    {
        EXP_saveMlAdd(ctx, " ");
    }
}







static void EXP_saveMlAddNode(EXP_SaveMLctx* ctx, EXP_Node node);


static void EXP_saveMlAddSeq(EXP_SaveMLctx* ctx, const EXP_NodeInfo* seqInfo)
{
    const EXP_Space* space = ctx->space;
    for (u32 i = 0; i < seqInfo->length; ++i)
    {
        EXP_saveMlAddIdent(ctx);

        EXP_saveMlAddNode(ctx, space->seqs.data[seqInfo->offset + i]);

        EXP_saveMlAdd(ctx, "\n");
    }
}




static void EXP_saveMlAddNodeSeq(EXP_SaveMLctx* ctx, EXP_Node node)
{
    const EXP_Space* space = ctx->space;

    u32 bufRemain = (ctx->bufSize > ctx->n) ? (ctx->bufSize - ctx->n) : 0;
    char* bufPtr = ctx->buf ? (ctx->buf + ctx->n) : NULL;
    u32 a = EXP_saveSL(space, node, bufPtr, bufRemain, ctx->opt->srcInfoTable);
    bool ok = EXP_saveMlForward(ctx, a);

    if (!ok)
    {
        EXP_NodeInfo* info = space->nodes.data + node.id;

        EXP_saveMlBack(ctx, a);

        EXP_saveMlAdd(ctx, "[\n");

        ++ctx->depth;
        EXP_saveMlAddSeq(ctx, info);
        --ctx->depth;

        EXP_saveMlAddIdent(ctx);
        EXP_saveMlAdd(ctx, "]");
    }
}



static void EXP_saveMlAddNode(EXP_SaveMLctx* ctx, EXP_Node node)
{
    const EXP_Space* space = ctx->space;
    EXP_NodeInfo* info = space->nodes.data + node.id;
    switch (info->type)
    {
    case EXP_NodeType_Tok:
    {
        u32 bufRemain = (ctx->bufSize > ctx->n) ? (ctx->bufSize - ctx->n) : 0;
        char* bufPtr = ctx->buf ? (ctx->buf + ctx->n) : NULL;
        u32 a = EXP_saveSL(space, node, bufPtr, bufRemain, ctx->opt->srcInfoTable);
        EXP_saveMlForward(ctx, a);
        return;
    }
    case EXP_NodeType_Seq:
    {
        EXP_saveMlAddNodeSeq(ctx, node);
        return;
    }
    default:
        assert(false);
        return;
    }
}



















u32 EXP_saveML(const EXP_Space* space, EXP_Node node, char* buf, u32 bufSize, const EXP_SaveMLopt* opt)
{
    EXP_NodeInfo* info = space->nodes.data + node.id;
    switch (info->type)
    {
    case EXP_NodeType_Tok:
    {
        return EXP_saveSL(space, node, buf, bufSize, opt->srcInfoTable);
    }
    case EXP_NodeType_Seq:
    {
        EXP_SaveMLctx ctx =
        {
            space, opt, bufSize, buf,
        };
        EXP_saveMlAddNodeSeq(&ctx, node);
        return ctx.n;
    }
    default:
        assert(false);
        return 0;
    }
}




























































































































