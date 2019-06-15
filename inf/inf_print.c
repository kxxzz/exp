#include "inf_a.h"






static void INF_seqBracketChs(INF_NodeType type, char ch[2])
{
    switch (type)
    {
    case INF_NodeType_SeqNaked:
        break;
    case INF_NodeType_SeqRound:
        ch[0] = '(';
        ch[1] = ')';
        break;
    case INF_NodeType_SeqSquare:
        ch[0] = '[';
        ch[1] = ']';
        break;
    case INF_NodeType_SeqCurly:
        ch[0] = '{';
        ch[1] = '}';
        break;
    default:
        assert(false);
        break;
    }
}








static u32 INF_printSlTok(const INF_Space* space, char* buf, u32 bufSize, const INF_SpaceSrcInfo* srcInfo, INF_Node src)
{
    assert(INF_isTok(space, src));

    INF_NodeInfo* info = space->nodes->data + src.id;
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







typedef struct INF_PrintSlSeqLevel
{
    INF_Node src;
    u32 p;
    char ch[2];
} INF_PrintSlSeqLevel;

typedef vec_t(INF_PrintSlSeqLevel) INF_PrintSlSeqStack;



static u32 INF_printSlSeq(const INF_Space* space, char* buf, u32 bufSize, const INF_SpaceSrcInfo* srcInfo, INF_Node src)
{
    INF_PrintSlSeqStack seqStack[1] = { 0 };

    assert(INF_isSeq(space, src));
    INF_PrintSlSeqLevel root = { src };
    vec_push(seqStack, root);

    u32 bufRemain = bufSize;
    char* bufPtr = buf;
    u32 n = 0;

    INF_PrintSlSeqLevel* top = NULL;
    INF_Node node;
    INF_NodeInfo* seqInfo = NULL;
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
    assert(seqInfo->type > INF_NodeType_Tok);
    p = top->p++;

    if (0 == p)
    {
        INF_seqBracketChs(seqInfo->type, top->ch);
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
    INF_Node e = ((INF_Node*)upool_elmData(space->dataPool, seqInfo->offset))[p];
    if (INF_isTok(space, e))
    {
        u32 a = INF_printSlTok(space, bufPtr, bufRemain, srcInfo, e);
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
        INF_PrintSlSeqLevel l = { e };
        vec_push(seqStack, l);
    }
    goto next;
}








u32 INF_printSL(const INF_Space* space, INF_Node node, char* buf, u32 bufSize, const INF_SpaceSrcInfo* srcInfo)
{
    if (INF_isTok(space, node))
    {
        return INF_printSlTok(space, buf, bufSize, srcInfo, node);
    }
    else
    {
        return INF_printSlSeq(space, buf, bufSize, srcInfo, node);
    }
}













typedef struct INF_PrintMlSeqLevel
{
    INF_Node src;
    u32 p;
    char ch[2];
} INF_PrintMlSeqLevel;

typedef vec_t(INF_PrintMlSeqLevel) INF_PrintMlSeqStack;




typedef struct INF_PrintMlContext
{
    const INF_Space* space;
    const INF_PrintMlOpt* opt;
    const u32 bufSize;
    char* const buf;

    u32 n;
    u32 column;
    u32 depth;

    INF_PrintMlSeqStack seqStack[1];
} INF_PrintMlContext;


static void INF_printMlContextFree(INF_PrintMlContext* ctx)
{
    vec_free(ctx->seqStack);
}





static bool INF_printMlForward(INF_PrintMlContext* ctx, u32 a)
{
    ctx->n += a;
    ctx->column += a;
    return ctx->column <= ctx->opt->width;
}

static void INF_printMlBack(INF_PrintMlContext* ctx, u32 a)
{
    assert(ctx->n >= a);
    assert(ctx->column >= a);
    ctx->n -= a;
    ctx->column -= a;
}


static void INF_printMlAddCh(INF_PrintMlContext* ctx, char c)
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


static void INF_printMlAdd(INF_PrintMlContext* ctx, const char* s)
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



static void INF_printMlAddIdent(INF_PrintMlContext* ctx)
{
    u32 n = ctx->opt->indent * ctx->depth;
    for (u32 i = 0; i < n; ++i)
    {
        INF_printMlAdd(ctx, " ");
    }
}










static void INF_printMlSeq(INF_PrintMlContext* ctx, INF_Node src)
{
    const INF_Space* space = ctx->space;
    INF_PrintMlSeqStack* seqStack = ctx->seqStack;
    assert(!seqStack->length);

    assert(INF_isSeq(space, src));
    INF_PrintMlSeqLevel root = { src };
    vec_push(seqStack, root);

    INF_PrintMlSeqLevel* top;
    INF_NodeInfo* seqInfo;
    u32 p;
next:
    if (0 == seqStack->length)
    {
        return;
    }
    top = &vec_last(seqStack);
    seqInfo = space->nodes->data + top->src.id;
    assert(seqInfo->type > INF_NodeType_Tok);
    p = top->p++;

    if (0 == p)
    {
        u32 bufRemain = (ctx->bufSize > ctx->n) ? (ctx->bufSize - ctx->n) : 0;
        char* bufPtr = ctx->buf ? (ctx->buf + ctx->n) : NULL;
        u32 a = INF_printSL(space, top->src, bufPtr, bufRemain, ctx->opt->srcInfo);
        bool ok = INF_printMlForward(ctx, a);
        if (ok)
        {
            vec_pop(seqStack);
            goto next;
        }

        INF_printMlBack(ctx, a);

        INF_seqBracketChs(seqInfo->type, top->ch);
        if (seqInfo->type != INF_NodeType_SeqNaked)
        {
            assert(top->ch[0]);
            assert(top->ch[1]);
            INF_printMlAddCh(ctx, top->ch[0]);
            if (seqInfo->type != INF_NodeType_SeqRound)
            {
                INF_printMlAddCh(ctx, '\n');
            }
            ++ctx->depth;
        }
    }
    else
    {
        if (p < seqInfo->length)
        {
            INF_printMlAddCh(ctx, '\n');
        }
        else
        {
            assert(p == seqInfo->length);
            if (INF_NodeType_SeqRound == seqInfo->type)
            {
                INF_printMlAddCh(ctx, top->ch[1]);
            }
            else
            {
                INF_printMlAddCh(ctx, '\n');
            }

            if (seqInfo->type != INF_NodeType_SeqNaked)
            {
                --ctx->depth;
                if (seqInfo->type != INF_NodeType_SeqRound)
                {
                    INF_printMlAddIdent(ctx);
                    INF_printMlAddCh(ctx, top->ch[1]);
                }
            }
            vec_pop(seqStack);
            goto next;
        }
    }

    if ((p > 0) || (seqInfo->type != INF_NodeType_SeqRound))
    {
        INF_printMlAddIdent(ctx);
    }
    INF_Node e = ((INF_Node*)upool_elmData(space->dataPool, seqInfo->offset))[p];
    INF_NodeInfo* eInfo = space->nodes->data + e.id;
    switch (eInfo->type)
    {
    case INF_NodeType_Tok:
    {
        u32 bufRemain = (ctx->bufSize > ctx->n) ? (ctx->bufSize - ctx->n) : 0;
        char* bufPtr = ctx->buf ? (ctx->buf + ctx->n) : NULL;
        u32 a = INF_printSL(space, e, bufPtr, bufRemain, ctx->opt->srcInfo);
        INF_printMlForward(ctx, a);
        break;
    }
    default:
    {
        INF_PrintMlSeqLevel l = { e };
        vec_push(seqStack, l);
        break;
    }
    }
    goto next;
}





u32 INF_printML(const INF_Space* space, INF_Node node, char* buf, u32 bufSize, const INF_PrintMlOpt* opt)
{
    INF_NodeInfo* info = space->nodes->data + node.id;
    switch (info->type)
    {
    case INF_NodeType_Tok:
    {
        return INF_printSL(space, node, buf, bufSize, opt->srcInfo);
    }
    default:
    {
        INF_PrintMlContext ctx[1] =
        {
            { space, opt, bufSize, buf }
        };
        INF_printMlSeq(ctx, node);
        INF_printMlContextFree(ctx);
        return ctx->n;
    }
    }
}
















































































































