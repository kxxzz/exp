#include "txn_a.h"






static void TXN_seqBracketChs(TXN_NodeType type, char ch[2])
{
    switch (type)
    {
    case TXN_NodeType_SeqNaked:
        break;
    case TXN_NodeType_SeqRound:
        ch[0] = '(';
        ch[1] = ')';
        break;
    case TXN_NodeType_SeqSquare:
        ch[0] = '[';
        ch[1] = ']';
        break;
    case TXN_NodeType_SeqCurly:
        ch[0] = '{';
        ch[1] = '}';
        break;
    default:
        assert(false);
        break;
    }
}








static u32 TXN_printSlTok(const TXN_Space* space, char* buf, u32 bufSize, const TXN_SpaceSrcInfo* srcInfo, TXN_Node src)
{
    assert(TXN_nodeIsTok(space, src));

    TXN_NodeInfo* info = space->nodes->data + src.id;
    const char* str = upool_elmData(space->dataPool, info->offset);
    u32 sreLen = info->length;
    u32 n;
    bool isQuotStr = false;
    if (srcInfo && (src.id < srcInfo->nodes->length))
    {
        isQuotStr = srcInfo->nodes->data[src.id].isQuotStr;
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







typedef struct TXN_PrintSlSeqLevel
{
    TXN_Node src;
    u32 p;
    char ch[2];
} TXN_PrintSlSeqLevel;

typedef vec_t(TXN_PrintSlSeqLevel) TXN_PrintSlSeqStack;



static u32 TXN_printSlSeq(const TXN_Space* space, char* buf, u32 bufSize, const TXN_SpaceSrcInfo* srcInfo, TXN_Node src)
{
    TXN_PrintSlSeqStack seqStack[1] = { 0 };

    assert(TXN_nodeIsSeq(space, src));
    TXN_PrintSlSeqLevel root = { src };
    vec_push(seqStack, root);

    u32 bufRemain = bufSize;
    char* bufPtr = buf;
    u32 n = 0;

    TXN_PrintSlSeqLevel* top = NULL;
    TXN_Node node;
    TXN_NodeInfo* seqInfo = NULL;
    u32 p;
next:
    if (!seqStack->length)
    {
        vec_free(seqStack);
        return n;
    }
    top = &vec_last(seqStack);
    node = top->src;
    seqInfo = space->nodes->data + node.id;
    assert(seqInfo->type > TXN_NodeType_Tok);
    p = top->p++;

    if (0 == p)
    {
        TXN_seqBracketChs(seqInfo->type, top->ch);
        if (top->ch[0])
        {
            if (1 < bufRemain)
            {
                *bufPtr = top->ch[0];
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
    else
    {
        if (p < seqInfo->length)
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
        else
        {
            assert(p == seqInfo->length);
            if (top->ch[0])
            {
                assert(top->ch[1]);
                if (1 < bufRemain)
                {
                    *bufPtr = top->ch[1];
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
            vec_pop(seqStack);
            goto next;
        }
    }
    TXN_Node e = ((TXN_Node*)upool_elmData(space->dataPool, seqInfo->offset))[p];
    if (TXN_nodeIsTok(space, e))
    {
        u32 a = TXN_printSlTok(space, bufPtr, bufRemain, srcInfo, e);
        if (a < bufRemain)
        {
            bufRemain -= a;
            bufPtr += a;
        }
        else
        {
            bufRemain = 0;
            bufPtr = NULL;
        }
        n += a;
    }
    else
    {
        TXN_PrintSlSeqLevel l = { e };
        vec_push(seqStack, l);
    }
    goto next;
}








u32 TXN_printSL(const TXN_Space* space, TXN_Node node, char* buf, u32 bufSize, const TXN_SpaceSrcInfo* srcInfo)
{
    if (TXN_nodeIsTok(space, node))
    {
        return TXN_printSlTok(space, buf, bufSize, srcInfo, node);
    }
    else
    {
        return TXN_printSlSeq(space, buf, bufSize, srcInfo, node);
    }
}













typedef struct TXN_PrintMlSeqLevel
{
    TXN_Node src;
    u32 p;
    char ch[2];
} TXN_PrintMlSeqLevel;

typedef vec_t(TXN_PrintMlSeqLevel) TXN_PrintMlSeqStack;




typedef struct TXN_PrintMlContext
{
    const TXN_Space* space;
    const TXN_PrintMlOpt* opt;
    const u32 bufSize;
    char* const buf;

    u32 n;
    u32 column;
    u32 depth;

    TXN_PrintMlSeqStack seqStack[1];
} TXN_PrintMlContext;


static void TXN_printMlContextFree(TXN_PrintMlContext* ctx)
{
    vec_free(ctx->seqStack);
}





static bool TXN_printMlForward(TXN_PrintMlContext* ctx, u32 a)
{
    ctx->n += a;
    ctx->column += a;
    return ctx->column <= ctx->opt->width;
}

static void TXN_printMlBack(TXN_PrintMlContext* ctx, u32 a)
{
    assert(ctx->n >= a);
    assert(ctx->column >= a);
    ctx->n -= a;
    ctx->column -= a;
}


static void TXN_printMlAddCh(TXN_PrintMlContext* ctx, char c)
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


static void TXN_printMlAdd(TXN_PrintMlContext* ctx, const char* s)
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



static void TXN_printMlAddIdent(TXN_PrintMlContext* ctx)
{
    u32 n = ctx->opt->indent * ctx->depth;
    for (u32 i = 0; i < n; ++i)
    {
        TXN_printMlAdd(ctx, " ");
    }
}










static void TXN_printMlSeq(TXN_PrintMlContext* ctx, TXN_Node src)
{
    const TXN_Space* space = ctx->space;
    TXN_PrintMlSeqStack* seqStack = ctx->seqStack;
    assert(!seqStack->length);

    assert(TXN_nodeIsSeq(space, src));
    TXN_PrintMlSeqLevel root = { src };
    vec_push(seqStack, root);

    TXN_PrintMlSeqLevel* top;
    TXN_NodeInfo* seqInfo;
    u32 p;
next:
    if (0 == seqStack->length)
    {
        return;
    }
    top = &vec_last(seqStack);
    seqInfo = space->nodes->data + top->src.id;
    assert(seqInfo->type > TXN_NodeType_Tok);
    p = top->p++;

    if (0 == p)
    {
        u32 bufRemain = (ctx->bufSize > ctx->n) ? (ctx->bufSize - ctx->n) : 0;
        char* bufPtr = ctx->buf ? (ctx->buf + ctx->n) : NULL;
        u32 a = TXN_printSL(space, top->src, bufPtr, bufRemain, ctx->opt->srcInfo);
        bool ok = TXN_printMlForward(ctx, a);
        if (ok)
        {
            vec_pop(seqStack);
            goto next;
        }

        TXN_printMlBack(ctx, a);

        TXN_seqBracketChs(seqInfo->type, top->ch);
        if (seqInfo->type != TXN_NodeType_SeqNaked)
        {
            assert(top->ch[0]);
            assert(top->ch[1]);
            TXN_printMlAddCh(ctx, top->ch[0]);
            if (seqInfo->type != TXN_NodeType_SeqRound)
            {
                TXN_printMlAddCh(ctx, '\n');
            }
            ++ctx->depth;
        }
    }
    else
    {
        if (p < seqInfo->length)
        {
            TXN_printMlAddCh(ctx, '\n');
        }
        else
        {
            assert(p == seqInfo->length);
            if (TXN_NodeType_SeqRound == seqInfo->type)
            {
                TXN_printMlAddCh(ctx, top->ch[1]);
            }
            else
            {
                TXN_printMlAddCh(ctx, '\n');
            }

            if (seqInfo->type != TXN_NodeType_SeqNaked)
            {
                --ctx->depth;
                if (seqInfo->type != TXN_NodeType_SeqRound)
                {
                    TXN_printMlAddIdent(ctx);
                    TXN_printMlAddCh(ctx, top->ch[1]);
                }
            }
            vec_pop(seqStack);
            goto next;
        }
    }

    if ((p > 0) || (seqInfo->type != TXN_NodeType_SeqRound))
    {
        TXN_printMlAddIdent(ctx);
    }
    TXN_Node e = ((TXN_Node*)upool_elmData(space->dataPool, seqInfo->offset))[p];
    TXN_NodeInfo* eInfo = space->nodes->data + e.id;
    switch (eInfo->type)
    {
    case TXN_NodeType_Tok:
    {
        u32 bufRemain = (ctx->bufSize > ctx->n) ? (ctx->bufSize - ctx->n) : 0;
        char* bufPtr = ctx->buf ? (ctx->buf + ctx->n) : NULL;
        u32 a = TXN_printSL(space, e, bufPtr, bufRemain, ctx->opt->srcInfo);
        TXN_printMlForward(ctx, a);
        break;
    }
    default:
    {
        TXN_PrintMlSeqLevel l = { e };
        vec_push(seqStack, l);
        break;
    }
    }
    goto next;
}





u32 TXN_printML(const TXN_Space* space, TXN_Node node, char* buf, u32 bufSize, const TXN_PrintMlOpt* opt)
{
    TXN_NodeInfo* info = space->nodes->data + node.id;
    switch (info->type)
    {
    case TXN_NodeType_Tok:
    {
        return TXN_printSL(space, node, buf, bufSize, opt->srcInfo);
    }
    default:
    {
        TXN_PrintMlContext ctx[1] =
        {
            { space, opt, bufSize, buf }
        };
        TXN_printMlSeq(ctx, node);
        TXN_printMlContextFree(ctx);
        return ctx->n;
    }
    }
}
















































































































