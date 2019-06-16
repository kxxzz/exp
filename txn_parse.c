#include "txn_a.h"



typedef enum TXN_TokenType
{
    TXN_TokenType_Text,
    TXN_TokenType_String,

    TXN_TokenType_SeqParenBegin,
    TXN_TokenType_SeqSquareBegin,
    TXN_TokenType_SeqBraceBegin,

    TXN_TokenType_SeqParenEnd,
    TXN_TokenType_SeqSquareEnd,
    TXN_TokenType_SeqBraceEnd,

    TXN_NumTokenTypes
} TXN_TokenType;



typedef struct TXN_Token
{
    TXN_TokenType type;
    u32 begin;
    u32 len;
} TXN_Token;



typedef struct TXN_ParseSeqLevel
{
    TXN_TokenType beginTokType;
    TXN_TokenType endTokType;
} TXN_ParseSeqLevel;

typedef vec_t(TXN_ParseSeqLevel) TXN_ParseSeqStack;




typedef struct TXN_ParseContext
{
    TXN_Space* space;
    u32 srcLen;
    const char* src;
    u32 cur;
    u32 curLine;
    TXN_SpaceSrcInfo* srcInfo;
    vec_char tmpStrBuf[1];
    TXN_ParseSeqStack seqStack[1];
    TXN_NodeVec seqDefStack[1];
    TXN_SeqDefFrameVec seqDefFrameStack[1];
} TXN_ParseContext;



static TXN_ParseContext TXN_parseContextNew
(
    TXN_Space* space, u32 strSize, const char* srcStr, TXN_SpaceSrcInfo* srcInfo
)
{
    assert(strSize == strlen(srcStr));
    if (srcInfo)
    {
        vec_push(srcInfo->fileBases, TXN_spaceNodesTotal(space));
    }
    TXN_ParseContext ctx = { space, strSize, srcStr, 0, 1, srcInfo };
    return ctx;
}

static void TXN_parseContextFree(TXN_ParseContext* ctx)
{
    vec_free(ctx->seqDefFrameStack);
    vec_free(ctx->seqDefStack);
    vec_free(ctx->seqStack);
    vec_free(ctx->tmpStrBuf);
}





static bool TXN_skipSapce(TXN_ParseContext* ctx)
{
    const char* src = ctx->src;
    for (;;)
    {
        if (ctx->cur >= ctx->srcLen)
        {
            return false;
        }
        else if (' ' >= src[ctx->cur])
        {
            if ('\n' == src[ctx->cur])
            {
                ++ctx->curLine;
            }
            ++ctx->cur;
            continue;
        }
        else if (ctx->cur + 1 < ctx->srcLen)
        {
            if (0 == strncmp("//", src + ctx->cur, 2))
            {
                ctx->cur += 2;
                for (;;)
                {
                    if (ctx->cur >= ctx->srcLen)
                    {
                        return false;
                    }
                    else if ('\n' == src[ctx->cur])
                    {
                        ++ctx->cur;
                        ++ctx->curLine;
                        break;
                    }
                    ++ctx->cur;
                }
                continue;
            }
            else if (0 == strncmp("/*", src + ctx->cur, 2))
            {
                ctx->cur += 2;
                u32 n = 1;
                for (;;)
                {
                    if (ctx->cur >= ctx->srcLen)
                    {
                        return false;
                    }
                    else if (ctx->cur + 1 < ctx->srcLen)
                    {
                        if (0 == strncmp("/*", src + ctx->cur, 2))
                        {
                            ++n;
                            ctx->cur += 2;
                            continue;
                        }
                        else if (0 == strncmp("*/", src + ctx->cur, 2))
                        {
                            --n;
                            if (0 == n)
                            {
                                ctx->cur += 2;
                                break;
                            }
                            ctx->cur += 2;
                            continue;
                        }
                    }
                    else if ('\n' == src[ctx->cur])
                    {
                        ++ctx->curLine;
                    }
                    ++ctx->cur;
                }
                continue;
            }
        }
        break;
    }
    return true;
}












static bool TXN_readToken_String(TXN_ParseContext* ctx, TXN_Token* out)
{
    const char* src = ctx->src;
    char endCh = src[ctx->cur];
    ++ctx->cur;
    TXN_Token tok = { TXN_TokenType_String, ctx->cur, 0 };
    for (;;)
    {
        if (ctx->cur >= ctx->srcLen)
        {
            return false;
        }
        else if (endCh == src[ctx->cur])
        {
            break;
        }
        else if ('\\' == src[ctx->cur])
        {
            ctx->cur += 2;
            continue;
        }
        else
        {
            if ('\n' == src[ctx->cur])
            {
                ++ctx->curLine;
            }
            ++ctx->cur;
        }
    }
    tok.len = ctx->cur - tok.begin;
    assert(tok.len > 0);
    *out = tok;
    ++ctx->cur;
    return true;
}



static bool TXN_readToken_Text(TXN_ParseContext* ctx, TXN_Token* out)
{
    TXN_Token tok = { TXN_TokenType_Text, ctx->cur, 0 };
    const char* src = ctx->src;
    for (;;)
    {
        if (ctx->cur >= ctx->srcLen)
        {
            break;
        }
        else if (strchr(",;", src[ctx->cur]))
        {
            if (0 == (ctx->cur - tok.begin))
            {
                ++ctx->cur;
                break;
            }
            else
            {
                break;
            }
        }
        else if (strchr("()[]{}\"' \t\n\r\b\f", src[ctx->cur]))
        {
            break;
        }
        else
        {
            ++ctx->cur;
        }
    }
    tok.len = ctx->cur - tok.begin;
    assert(tok.len > 0);
    *out = tok;
    return true;
}




static bool TXN_readToken(TXN_ParseContext* ctx, TXN_Token* out)
{
    const char* src = ctx->src;
    if (ctx->cur >= ctx->srcLen)
    {
        return false;
    }
    if (!TXN_skipSapce(ctx))
    {
        return false;
    }
    bool ok = false;
    if ('(' == src[ctx->cur])
    {
        TXN_Token tok = { TXN_TokenType_SeqParenBegin, ctx->cur, 1 };
        *out = tok;
        ++ctx->cur;
        ok = true;
    }
    else if (')' == src[ctx->cur])
    {
        TXN_Token tok = { TXN_TokenType_SeqParenEnd, ctx->cur, 1 };
        *out = tok;
        ++ctx->cur;
        ok = true;
    }
    else if ('[' == src[ctx->cur])
    {
        TXN_Token tok = { TXN_TokenType_SeqSquareBegin, ctx->cur, 1 };
        *out = tok;
        ++ctx->cur;
        ok = true;
    }
    else if (']' == src[ctx->cur])
    {
        TXN_Token tok = { TXN_TokenType_SeqSquareEnd, ctx->cur, 1 };
        *out = tok;
        ++ctx->cur;
        ok = true;
    }
    else if ('{' == src[ctx->cur])
    {
        TXN_Token tok = { TXN_TokenType_SeqBraceBegin, ctx->cur, 1 };
        *out = tok;
        ++ctx->cur;
        ok = true;
    }
    else if ('}' == src[ctx->cur])
    {
        TXN_Token tok = { TXN_TokenType_SeqBraceEnd, ctx->cur, 1 };
        *out = tok;
        ++ctx->cur;
        ok = true;
    }
    else if (('"' == src[ctx->cur]) || ('\'' == src[ctx->cur]))
    {
        ok = TXN_readToken_String(ctx, out);
    }
    else
    {
        ok = TXN_readToken_Text(ctx, out);
    }
    return ok;
}











static void TXN_tokenToNodeSrcInfo(TXN_ParseContext* ctx, const TXN_Token* tok, TXN_NodeSrcInfo* info)
{
    if (!ctx->srcInfo)
    {
        return;
    }
    assert(ctx->srcInfo->fileBases->length > 0);
    info->file = ctx->srcInfo->fileBases->length - 1;
    info->offset = tok->begin;
    info->line = ctx->curLine;
    info->column = 1;
    for (u32 i = 0; i < info->offset; ++i)
    {
        char c = ctx->src[info->offset - i];
        if (strchr("\n\r", c))
        {
            info->column = i;
            break;
        }
    }
    info->isQuotStr = TXN_TokenType_String == tok->type;
}






static bool TXN_tokenToNode(TXN_ParseContext* ctx, const TXN_Token* tok, TXN_Node* pNode)
{
    TXN_Space* space = ctx->space;
    bool isQuotStr = TXN_TokenType_String == tok->type;
    switch (tok->type)
    {
    case TXN_TokenType_Text:
    {
        const char* str = ctx->src + tok->begin;
        *pNode = TXN_tokFromBuf(space, str, tok->len, isQuotStr);
        break;
    }
    case TXN_TokenType_String:
    {
        char endCh = ctx->src[tok->begin - 1];
        const char* src = ctx->src + tok->begin;
        u32 n = 0;
        for (u32 i = 0; i < tok->len; ++i)
        {
            if ('\\' == src[i])
            {
                ++n;
                ++i;
            }
        }
        u32 len = tok->len - n;
        vec_resize(ctx->tmpStrBuf, len + 1);
        u32 si = 0;
        for (u32 i = 0; i < tok->len; ++i)
        {
            if ('\\' == src[i])
            {
                ++i;
                ctx->tmpStrBuf->data[si++] = src[i];
                continue;
            }
            ctx->tmpStrBuf->data[si++] = src[i];
        }
        ctx->tmpStrBuf->data[len] = 0;
        assert(si == len);
        *pNode = TXN_tokFromBuf(space, ctx->tmpStrBuf->data, len, isQuotStr);
        break;
    }
    case TXN_TokenType_SeqParenBegin:
    case TXN_TokenType_SeqSquareBegin:
    case TXN_TokenType_SeqBraceBegin:
    {
        return false;
    }
    default:
        assert(false);
        break;
    }
    return true;
}







static void TXN_addSeqEnter(TXN_ParseContext* ctx, TXN_NodeType type)
{
    assert(type > TXN_NodeType_Tok);
    TXN_SeqDefFrame f = { type, ctx->seqDefStack->length };
    vec_push(ctx->seqDefFrameStack, f);
}

static void TXN_addSeqPush(TXN_ParseContext* ctx, TXN_Node x)
{
    vec_push(ctx->seqDefStack, x);
}

static void TXN_addSeqCancel(TXN_ParseContext* ctx)
{
    assert(ctx->seqDefFrameStack->length > 0);
    TXN_SeqDefFrame f = vec_last(ctx->seqDefFrameStack);
    vec_pop(ctx->seqDefFrameStack);
    vec_resize(ctx->seqDefStack, f.p);
}

static TXN_Node TXN_addSeqDone(TXN_ParseContext* ctx)
{
    TXN_Space* space = ctx->space;
    assert(ctx->seqDefFrameStack->length > 0);
    TXN_SeqDefFrame f = vec_last(ctx->seqDefFrameStack);
    vec_pop(ctx->seqDefFrameStack);
    u32 len = ctx->seqDefStack->length - f.p;
    TXN_Node* elms = ctx->seqDefStack->data + f.p;
    TXN_Node node = TXN_seqNew(space, f.seqType, elms, len);
    vec_resize(ctx->seqDefStack, f.p);
    return node;
}







static bool TXN_parseEnd(TXN_ParseContext* ctx)
{
    assert(ctx->srcLen >= ctx->cur);
    if (ctx->srcLen == ctx->cur)
    {
        return true;
    }
    return false;
}

static bool TXN_parseSeqEnd(TXN_ParseContext* ctx, TXN_TokenType endTokType)
{
    u32 cur0 = ctx->cur;
    u32 curLine0 = ctx->curLine;
    TXN_Token tok[1];
    if (!TXN_readToken(ctx, tok))
    {
        return true;
    }
    if (tok->type == endTokType)
    {
        return true;
    }
    ctx->cur = cur0;
    ctx->curLine = curLine0;
    return false;
}






static TXN_Node TXN_parseSeq(TXN_ParseContext* ctx, const TXN_Token* beginTok)
{
    TXN_Space* space = ctx->space;
    TXN_SpaceSrcInfo* srcInfo = ctx->srcInfo;
    TXN_ParseSeqStack* seqStack = ctx->seqStack;
    assert(!seqStack->length);

    TXN_ParseSeqLevel root = { beginTok->type, -1 };
    vec_push(seqStack, root);
    TXN_ParseSeqLevel* cur = NULL;
    TXN_Node r;
    TXN_Token tok[1];
next:
    if (!seqStack->length)
    {
        return r;
    }
    cur = &vec_last(seqStack);
    if (-1 == cur->endTokType)
    {
        TXN_NodeType seqType;
        switch (cur->beginTokType)
        {
        case TXN_TokenType_SeqParenBegin:
            seqType = TXN_NodeType_SeqRound;
            cur->endTokType = TXN_TokenType_SeqParenEnd;
            break;
        case TXN_TokenType_SeqSquareBegin:
            seqType = TXN_NodeType_SeqSquare;
            cur->endTokType = TXN_TokenType_SeqSquareEnd;
            break;
        case TXN_TokenType_SeqBraceBegin:
            seqType = TXN_NodeType_SeqCurly;
            cur->endTokType = TXN_TokenType_SeqBraceEnd;
            break;
        default:
            assert(false);
            break;
        }
        TXN_addSeqEnter(ctx, seqType);
    }
    else
    {
        TXN_addSeqPush(ctx, r);
    }
    if (TXN_parseSeqEnd(ctx, cur->endTokType))
    {
        vec_pop(seqStack);
        r = TXN_addSeqDone(ctx);
        assert(r.id != TXN_Node_Invalid.id);
        if (srcInfo)
        {
            TXN_NodeSrcInfo nodeSrcInfo = { 0 };
            TXN_tokenToNodeSrcInfo(ctx, beginTok, &nodeSrcInfo);
            vec_push(srcInfo->nodes, nodeSrcInfo);
        }
        goto next;
    }
    else
    {
        if (!TXN_readToken(ctx, tok))
        {
            goto failed;
        }
        if (!TXN_tokenToNode(ctx, tok, &r))
        {
            TXN_ParseSeqLevel l = { tok->type, -1 };
            vec_push(seqStack, l);
            goto next;
        }
        assert(r.id != TXN_Node_Invalid.id);
        if (srcInfo)
        {
            TXN_NodeSrcInfo nodeSrcInfo = { 0 };
            TXN_tokenToNodeSrcInfo(ctx, tok, &nodeSrcInfo);
            vec_push(srcInfo->nodes, nodeSrcInfo);
        }
        goto next;
    }
failed:
    vec_resize(seqStack, 0);
    TXN_addSeqCancel(ctx);
    return TXN_Node_Invalid;
}







static TXN_Node TXN_parseNode(TXN_ParseContext* ctx)
{
    TXN_Space* space = ctx->space;
    TXN_SpaceSrcInfo* srcInfo = ctx->srcInfo;
    TXN_Node node = TXN_Node_Invalid;
    TXN_Token tok[1];
    if (!TXN_readToken(ctx, tok))
    {
        return node;
    }
    if (!TXN_tokenToNode(ctx, tok, &node))
    {
        node = TXN_parseSeq(ctx, tok);
        return node;
    }
    if (srcInfo)
    {
        TXN_NodeSrcInfo nodeSrcInfo = { 0 };
        TXN_tokenToNodeSrcInfo(ctx, tok, &nodeSrcInfo);
        vec_push(srcInfo->nodes, nodeSrcInfo);
    }
    return node;
}













TXN_Node TXN_parseAsCell(TXN_Space* space, const char* src, TXN_SpaceSrcInfo* srcInfo)
{
    TXN_ParseContext ctx[1] = { TXN_parseContextNew(space, (u32)strlen(src), src, srcInfo) };
    TXN_Node node = TXN_parseNode(ctx);
    if ((TXN_Node_Invalid.id == node.id) || (!TXN_parseEnd(ctx)))
    {
        TXN_parseContextFree(ctx);
        TXN_Node node = { TXN_Node_Invalid.id };
        return node;
    }
    TXN_parseContextFree(ctx);
    return node;
}

TXN_Node TXN_parseAsList(TXN_Space* space, const char* src, TXN_SpaceSrcInfo* srcInfo)
{
    TXN_ParseContext ctx[1] = { TXN_parseContextNew(space, (u32)strlen(src), src, srcInfo) };
    TXN_addSeqEnter(ctx, TXN_NodeType_SeqNaked);
    bool errorHappen = false;
    while (TXN_skipSapce(ctx))
    {
        TXN_Node e = TXN_parseNode(ctx);
        if (TXN_Node_Invalid.id == e.id)
        {
            errorHappen = true;
            break;
        }
        TXN_addSeqPush(ctx, e);
    }
    if (!TXN_parseEnd(ctx) || errorHappen)
    {
        TXN_parseContextFree(ctx);
        TXN_addSeqCancel(ctx);
        TXN_Node node = { TXN_Node_Invalid.id };
        return node;
    }
    TXN_Node node = TXN_addSeqDone(ctx);
    if (srcInfo)
    {
        TXN_NodeSrcInfo nodeSrcInfo = { srcInfo->fileBases->length - 1 };
        vec_push(srcInfo->nodes, nodeSrcInfo);
    }
    TXN_parseContextFree(ctx);
    return node;
}




























































































