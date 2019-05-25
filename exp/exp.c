#include "exp_a.h"



typedef struct EXP_NodeInfo
{
    EXP_NodeType type;
    u32 offset;
    u32 length;
    u32 quoted;
} EXP_NodeInfo;

typedef vec_t(EXP_NodeInfo) EXP_NodeInfoVec;


typedef struct EXP_SeqDefFrame
{
    EXP_NodeType seqType;
    u32 p;
} EXP_SeqDefFrame;

typedef vec_t(EXP_SeqDefFrame) EXP_SeqDefFrameVec;


typedef struct EXP_Space
{
    EXP_NodeInfoVec nodes;
    Upool* dataPool;
    EXP_NodeVec seqDefStack;
    EXP_SeqDefFrameVec seqDefFrameStack;
    vec_char cstrBuf;
} EXP_Space;






EXP_Space* EXP_newSpace(void)
{
    EXP_Space* space = zalloc(sizeof(*space));
    space->dataPool = newUpool(256);
    return space;
}

void EXP_spaceFree(EXP_Space* space)
{
    vec_free(&space->cstrBuf);
    vec_free(&space->seqDefFrameStack);
    vec_free(&space->seqDefStack);
    upoolFree(space->dataPool);
    vec_free(&space->nodes);
    free(space);
}





void EXP_spaceSrcInfoFree(EXP_SpaceSrcInfo* srcInfo)
{
    vec_free(&srcInfo->nodes);
}


const EXP_NodeSrcInfo* EXP_nodeSrcInfo(const EXP_SpaceSrcInfo* srcInfo, EXP_Node node)
{
    assert(node.id - srcInfo->baseNodeId < srcInfo->nodes.length);
    return srcInfo->nodes.data + node.id - srcInfo->baseNodeId;
}






u32 EXP_spaceNodesTotal(EXP_Space* space)
{
    return space->nodes.length;
}








EXP_NodeType EXP_nodeType(EXP_Space* space, EXP_Node node)
{
    EXP_NodeInfo* info = space->nodes.data + node.id;
    return info->type;
}









EXP_Node EXP_addTok(EXP_Space* space, const char* str, bool quoted)
{
    u32 len = (u32)strlen(str);
    u32 offset = upoolElm(space->dataPool, str, len + 1, NULL);
    EXP_NodeInfo info = { EXP_NodeType_Tok, offset, len, quoted };
    EXP_Node node = { space->nodes.length };
    vec_push(&space->nodes, info);
    return node;
}

EXP_Node EXP_addTokL(EXP_Space* space, const char* str, u32 len, bool quoted)
{
    vec_resize(&space->cstrBuf, len + 1);
    memcpy(space->cstrBuf.data, str, len);
    space->cstrBuf.data[len] = 0;
    u32 offset = upoolElm(space->dataPool, space->cstrBuf.data, len + 1, NULL);
    EXP_NodeInfo info = { EXP_NodeType_Tok, offset, len, quoted };
    EXP_Node node = { space->nodes.length };
    vec_push(&space->nodes, info);
    return node;
}





void EXP_addSeqEnter(EXP_Space* space, EXP_NodeType type)
{
    assert(type > EXP_NodeType_Tok);
    EXP_SeqDefFrame f = { type, space->seqDefStack.length };
    vec_push(&space->seqDefFrameStack, f);
}

void EXP_addSeqPush(EXP_Space* space, EXP_Node x)
{
    vec_push(&space->seqDefStack, x);
}

void EXP_addSeqCancel(EXP_Space* space)
{
    assert(space->seqDefFrameStack.length > 0);
    EXP_SeqDefFrame f = vec_last(&space->seqDefFrameStack);
    vec_pop(&space->seqDefFrameStack);
    vec_resize(&space->seqDefStack, f.p);
}

EXP_Node EXP_addSeqDone(EXP_Space* space)
{
    assert(space->seqDefFrameStack.length > 0);
    EXP_SeqDefFrame f = vec_last(&space->seqDefFrameStack);
    vec_pop(&space->seqDefFrameStack);
    u32 lenSeq = space->seqDefStack.length - f.p;
    EXP_Node* seq = space->seqDefStack.data + f.p;
    u32 offset = upoolElm(space->dataPool, seq, sizeof(EXP_Node)*lenSeq, NULL);
    EXP_NodeInfo nodeInfo = { f.seqType, offset, lenSeq };
    vec_resize(&space->seqDefStack, f.p);
    EXP_Node node = { space->nodes.length };
    vec_push(&space->nodes, nodeInfo);
    return node;
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
    return upoolElmData(space->dataPool, info->offset);
}

bool EXP_tokQuoted(EXP_Space* space, EXP_Node node)
{
    EXP_NodeInfo* info = space->nodes.data + node.id;
    assert(EXP_NodeType_Tok == info->type);
    return info->quoted;
}




u32 EXP_seqLen(EXP_Space* space, EXP_Node node)
{
    EXP_NodeInfo* info = space->nodes.data + node.id;
    assert(EXP_NodeType_Tok < info->type);
    return info->length;
}

const EXP_Node* EXP_seqElm(EXP_Space* space, EXP_Node node)
{
    EXP_NodeInfo* info = space->nodes.data + node.id;
    assert(EXP_NodeType_Tok < info->type);
    return upoolElmData(space->dataPool, info->offset);
}






bool EXP_nodeDataEq(EXP_Space* space, EXP_Node a, EXP_Node b)
{
    EXP_NodeInfo* aInfo = space->nodes.data + a.id;
    EXP_NodeInfo* bInfo = space->nodes.data + b.id;
    return 0 == memcmp(aInfo, bInfo, sizeof(*aInfo));
}












static void EXP_seqBracketChs(EXP_NodeType type, char ch[2])
{
    switch (type)
    {
    case EXP_NodeType_SeqNaked:
        break;
    case EXP_NodeType_SeqRound:
        ch[0] = '(';
        ch[1] = ')';
        break;
    case EXP_NodeType_SeqSquare:
        ch[0] = '[';
        ch[1] = ']';
        break;
    case EXP_NodeType_SeqCurly:
        ch[0] = '{';
        ch[1] = '}';
        break;
    default:
        assert(false);
        break;
    }
}












static u32 EXP_saveSeqSL
(
    const EXP_Space* space, const EXP_NodeInfo* seqInfo, char* buf, u32 bufSize, const EXP_SpaceSrcInfo* srcInfo
)
{
    u32 n = 0;
    u32 bufRemain = bufSize;
    char* bufPtr = buf;

    for (u32 i = 0; i < seqInfo->length; ++i)
    {
        EXP_Node e = ((EXP_Node*)upoolElmData(space->dataPool, seqInfo->offset))[i];
        u32 en = EXP_saveSL(space, e, bufPtr, bufRemain, srcInfo);
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


u32 EXP_saveSL(const EXP_Space* space, EXP_Node node, char* buf, u32 bufSize, const EXP_SpaceSrcInfo* srcInfo)
{
    EXP_NodeInfo* info = space->nodes.data + node.id;
    switch (info->type)
    {
    case EXP_NodeType_Tok:
    {
        const char* str = upoolElmData(space->dataPool, info->offset);
        u32 sreLen = info->length;
        u32 n;
        bool isQuotStr = false;
        if (srcInfo && (node.id < srcInfo->nodes.length))
        {
            isQuotStr = srcInfo->nodes.data[node.id].isQuotStr;
        }
        else
        {
            for (u32 i = 0; i < sreLen; ++i)
            {
                if (strchr("()[]{}\"' \t\n\r\b\f", str[i]))
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
                else if (strchr("()[]{}\"'", str[i]))
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
                    else if (strchr("()[]{}\"'", str[i]))
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
    default:
    {
        char ch[2] = { 0 };
        EXP_seqBracketChs(info->type, ch);
        u32 n = 0;

        u32 bufRemain = bufSize;
        char* bufPtr = buf;

        if (ch[0])
        {
            if (1 < bufRemain)
            {
                *bufPtr = ch[0];
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

        u32 n1 = EXP_saveSeqSL(space, info, bufPtr, bufRemain, srcInfo);
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

        if (ch[0])
        {
            assert(ch[1]);
            if (1 < bufRemain)
            {
                *bufPtr = ch[1];
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
        }
        return n;
    }
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


static void EXP_saveMlAddCh(EXP_SaveMLctx* ctx, char c)
{
    assert(c);
    u32 bufRemain = (ctx->bufSize > ctx->n) ? (ctx->bufSize - ctx->n) : 0;
    if (bufRemain > 0)
    {
        assert(ctx->buf);
        u32 wn = min(bufRemain - 1, 1);
        if (wn > 0)
        {
            char* bufPtr = ctx->buf + ctx->n;
            bufPtr[0] = c;
            bufPtr[1] = 0;
        }
    }
    ctx->n += 1;
    if (c != '\n')
    {
        ctx->column += 1;
    }
    else
    {
        ctx->column = 0;
    }
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
    assert(seqInfo->type > EXP_NodeType_Tok);
    const EXP_Space* space = ctx->space;
    char ch[2] = { 0 };
    EXP_seqBracketChs(seqInfo->type, ch);
    if (seqInfo->type != EXP_NodeType_SeqNaked)
    {
        assert(ch[0]);
        assert(ch[1]);
        EXP_saveMlAddCh(ctx, ch[0]);
        if (seqInfo->type != EXP_NodeType_SeqRound)
        {
            EXP_saveMlAddCh(ctx, '\n');
        }
        ++ctx->depth;
    }
    for (u32 i = 0; i < seqInfo->length; ++i)
    {
        if ((i > 0) || (seqInfo->type != EXP_NodeType_SeqRound))
        {
            EXP_saveMlAddIdent(ctx);
        }
        EXP_Node e = ((EXP_Node*)upoolElmData(space->dataPool, seqInfo->offset))[i];
        EXP_saveMlAddNode(ctx, e);
        if (i < seqInfo->length - 1)
        {
            EXP_saveMlAddCh(ctx, '\n');
        }
        else
        {
            if (EXP_NodeType_SeqRound == seqInfo->type)
            {
                EXP_saveMlAddCh(ctx, ch[1]);
            }
            else
            {
                EXP_saveMlAddCh(ctx, '\n');
            }
        }
    }
    if (seqInfo->type != EXP_NodeType_SeqNaked)
    {
        --ctx->depth;
        if (seqInfo->type != EXP_NodeType_SeqRound)
        {
            EXP_saveMlAddIdent(ctx);
            EXP_saveMlAddCh(ctx, ch[1]);
        }
    }
}




static void EXP_saveMlAddNodeSeq(EXP_SaveMLctx* ctx, EXP_Node node)
{
    const EXP_Space* space = ctx->space;

    u32 bufRemain = (ctx->bufSize > ctx->n) ? (ctx->bufSize - ctx->n) : 0;
    char* bufPtr = ctx->buf ? (ctx->buf + ctx->n) : NULL;
    u32 a = EXP_saveSL(space, node, bufPtr, bufRemain, ctx->opt->srcInfo);
    bool ok = EXP_saveMlForward(ctx, a);

    if (!ok)
    {
        EXP_saveMlBack(ctx, a);
        EXP_NodeInfo* info = space->nodes.data + node.id;
        EXP_saveMlAddSeq(ctx, info);
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
        u32 a = EXP_saveSL(space, node, bufPtr, bufRemain, ctx->opt->srcInfo);
        EXP_saveMlForward(ctx, a);
        return;
    }
    default:
    {
        EXP_saveMlAddNodeSeq(ctx, node);
        return;
    }
    }
}



















u32 EXP_saveML(const EXP_Space* space, EXP_Node node, char* buf, u32 bufSize, const EXP_SaveMLopt* opt)
{
    EXP_NodeInfo* info = space->nodes.data + node.id;
    switch (info->type)
    {
    case EXP_NodeType_Tok:
    {
        return EXP_saveSL(space, node, buf, bufSize, opt->srcInfo);
    }
    default:
    {
        EXP_SaveMLctx ctx =
        {
            space, opt, bufSize, buf,
        };
        EXP_saveMlAddNodeSeq(&ctx, node);
        return ctx.n;
    }
    }
}




























































































































