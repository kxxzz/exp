#include "inf_a.h"



typedef enum INF_TokenType
{
    INF_TokenType_Text,
    INF_TokenType_String,

    INF_TokenType_SeqParenBegin,
    INF_TokenType_SeqSquareBegin,
    INF_TokenType_SeqBraceBegin,

    INF_TokenType_SeqParenEnd,
    INF_TokenType_SeqSquareEnd,
    INF_TokenType_SeqBraceEnd,

    INF_NumTokenTypes
} INF_TokenType;



typedef struct INF_Token
{
    INF_TokenType type;
    u32 begin;
    u32 len;
} INF_Token;



typedef struct INF_ParseSeqLevel
{
    INF_TokenType beginTokType;
    INF_TokenType endTokType;
} INF_ParseSeqLevel;

typedef vec_t(INF_ParseSeqLevel) INF_ParseSeqStack;




typedef struct INF_ParseContext
{
    INF_Space* space;
    u32 srcLen;
    const char* src;
    u32 cur;
    u32 curLine;
    INF_SpaceSrcInfo* srcInfo;
    vec_char tmpStrBuf[1];
    INF_ParseSeqStack seqStack[1];
} INF_ParseContext;



static INF_ParseContext INF_newParseContext
(
    INF_Space* space, u32 strSize, const char* srcStr, INF_SpaceSrcInfo* srcInfo
)
{
    assert(strSize == strlen(srcStr));
    if (srcInfo)
    {
        srcInfo->baseNodeId = INF_spaceNodesTotal(space);
        ++srcInfo->fileCount;
    }
    INF_ParseContext ctx = { space, strSize, srcStr, 0, 1, srcInfo };
    return ctx;
}

static void INF_parseContextFree(INF_ParseContext* ctx)
{
    vec_free(ctx->seqStack);
    vec_free(ctx->tmpStrBuf);
}





static bool INF_skipSapce(INF_ParseContext* ctx)
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












static bool INF_readToken_String(INF_ParseContext* ctx, INF_Token* out)
{
    const char* src = ctx->src;
    char endCh = src[ctx->cur];
    ++ctx->cur;
    INF_Token tok = { INF_TokenType_String, ctx->cur, 0 };
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



static bool INF_readToken_Text(INF_ParseContext* ctx, INF_Token* out)
{
    INF_Token tok = { INF_TokenType_Text, ctx->cur, 0 };
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




static bool INF_readToken(INF_ParseContext* ctx, INF_Token* out)
{
    const char* src = ctx->src;
    if (ctx->cur >= ctx->srcLen)
    {
        return false;
    }
    if (!INF_skipSapce(ctx))
    {
        return false;
    }
    bool ok = false;
    if ('(' == src[ctx->cur])
    {
        INF_Token tok = { INF_TokenType_SeqParenBegin, ctx->cur, 1 };
        *out = tok;
        ++ctx->cur;
        ok = true;
    }
    else if (')' == src[ctx->cur])
    {
        INF_Token tok = { INF_TokenType_SeqParenEnd, ctx->cur, 1 };
        *out = tok;
        ++ctx->cur;
        ok = true;
    }
    else if ('[' == src[ctx->cur])
    {
        INF_Token tok = { INF_TokenType_SeqSquareBegin, ctx->cur, 1 };
        *out = tok;
        ++ctx->cur;
        ok = true;
    }
    else if (']' == src[ctx->cur])
    {
        INF_Token tok = { INF_TokenType_SeqSquareEnd, ctx->cur, 1 };
        *out = tok;
        ++ctx->cur;
        ok = true;
    }
    else if ('{' == src[ctx->cur])
    {
        INF_Token tok = { INF_TokenType_SeqBraceBegin, ctx->cur, 1 };
        *out = tok;
        ++ctx->cur;
        ok = true;
    }
    else if ('}' == src[ctx->cur])
    {
        INF_Token tok = { INF_TokenType_SeqBraceEnd, ctx->cur, 1 };
        *out = tok;
        ++ctx->cur;
        ok = true;
    }
    else if (('"' == src[ctx->cur]) || ('\'' == src[ctx->cur]))
    {
        ok = INF_readToken_String(ctx, out);
    }
    else
    {
        ok = INF_readToken_Text(ctx, out);
    }
    return ok;
}











static void INF_tokenToNodeSrcInfo(INF_ParseContext* ctx, const INF_Token* tok, INF_NodeSrcInfo* info)
{
    if (!ctx->srcInfo)
    {
        return;
    }
    assert(ctx->srcInfo->fileCount > 0);
    info->file = ctx->srcInfo->fileCount - 1;
    info->offset = ctx->cur - tok->len;
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
    info->isQuotStr = INF_TokenType_String == tok->type;
}






static bool INF_tokenToNode(INF_ParseContext* ctx, const INF_Token* tok, INF_Node* pNode)
{
    INF_Space* space = ctx->space;
    bool isQuotStr = INF_TokenType_String == tok->type;
    switch (tok->type)
    {
    case INF_TokenType_Text:
    {
        const char* str = ctx->src + tok->begin;
        *pNode = INF_addTokL(space, str, tok->len, isQuotStr);
        break;
    }
    case INF_TokenType_String:
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
        *pNode = INF_addTokL(space, ctx->tmpStrBuf->data, len, isQuotStr);
        break;
    }
    case INF_TokenType_SeqParenBegin:
    case INF_TokenType_SeqSquareBegin:
    case INF_TokenType_SeqBraceBegin:
    {
        return false;
    }
    default:
        assert(false);
        break;
    }
    return true;
}






static bool INF_parseEnd(INF_ParseContext* ctx)
{
    assert(ctx->srcLen >= ctx->cur);
    if (ctx->srcLen == ctx->cur)
    {
        return true;
    }
    return false;
}

static bool INF_parseSeqEnd(INF_ParseContext* ctx, INF_TokenType endTokType)
{
    u32 cur0 = ctx->cur;
    u32 curLine0 = ctx->curLine;
    INF_Token tok[1];
    if (!INF_readToken(ctx, tok))
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





static INF_Node INF_parseSeq(INF_ParseContext* ctx, INF_TokenType beginTokType)
{
    INF_Space* space = ctx->space;
    INF_SpaceSrcInfo* srcInfo = ctx->srcInfo;
    INF_ParseSeqStack* seqStack = ctx->seqStack;
    assert(!seqStack->length);

    INF_ParseSeqLevel root = { beginTokType, -1 };
    vec_push(seqStack, root);
    INF_ParseSeqLevel* cur = NULL;
    INF_Node r;
    INF_Token tok[1];
next:
    if (!seqStack->length)
    {
        return r;
    }
    cur = &vec_last(seqStack);
    if (-1 == cur->endTokType)
    {
        INF_NodeType seqType;
        switch (cur->beginTokType)
        {
        case INF_TokenType_SeqParenBegin:
            seqType = INF_NodeType_SeqRound;
            cur->endTokType = INF_TokenType_SeqParenEnd;
            break;
        case INF_TokenType_SeqSquareBegin:
            seqType = INF_NodeType_SeqSquare;
            cur->endTokType = INF_TokenType_SeqSquareEnd;
            break;
        case INF_TokenType_SeqBraceBegin:
            seqType = INF_NodeType_SeqCurly;
            cur->endTokType = INF_TokenType_SeqBraceEnd;
            break;
        default:
            assert(false);
            break;
        }
        INF_addSeqEnter(space, seqType);
    }
    else
    {
        INF_addSeqPush(ctx->space, r);
    }
    if (INF_parseSeqEnd(ctx, cur->endTokType))
    {
        vec_pop(seqStack);
        r = INF_addSeqDone(space);

        assert(r.id != INF_Node_Invalid.id);
        if (srcInfo)
        {
            INF_NodeSrcInfo nodeSrcInfo = { 0 };
            INF_tokenToNodeSrcInfo(ctx, tok, &nodeSrcInfo);
            vec_push(srcInfo->nodes, nodeSrcInfo);
        }
        goto next;
    }
    else
    {
        if (!INF_readToken(ctx, tok))
        {
            goto failed;
        }
        if (!INF_tokenToNode(ctx, tok, &r))
        {
            INF_ParseSeqLevel l = { tok->type, -1 };
            vec_push(seqStack, l);
        }
        assert(r.id != INF_Node_Invalid.id);
        if (srcInfo)
        {
            INF_NodeSrcInfo nodeSrcInfo = { 0 };
            INF_tokenToNodeSrcInfo(ctx, tok, &nodeSrcInfo);
            vec_push(srcInfo->nodes, nodeSrcInfo);
        }
        goto next;
    }
failed:
    vec_resize(seqStack, 0);
    INF_addSeqCancel(space);
    return INF_Node_Invalid;
}







static INF_Node INF_parseNode(INF_ParseContext* ctx)
{
    INF_Space* space = ctx->space;
    INF_SpaceSrcInfo* srcInfo = ctx->srcInfo;
    INF_Node node = INF_Node_Invalid;
    INF_Token tok[1];
    if (!INF_readToken(ctx, tok))
    {
        return node;
    }
    if (!INF_tokenToNode(ctx, tok, &node))
    {
        node = INF_parseSeq(ctx, tok->type);
        return node;
    }
    if (srcInfo)
    {
        INF_NodeSrcInfo nodeSrcInfo = { 0 };
        INF_tokenToNodeSrcInfo(ctx, tok, &nodeSrcInfo);
        vec_push(srcInfo->nodes, nodeSrcInfo);
    }
    return node;
}













INF_Node INF_parseAsCell(INF_Space* space, const char* src, INF_SpaceSrcInfo* srcInfo)
{
    INF_ParseContext ctx[1] = { INF_newParseContext(space, (u32)strlen(src), src, srcInfo) };
    INF_Node node = INF_parseNode(ctx);
    if ((INF_Node_Invalid.id == node.id) || (!INF_parseEnd(ctx)))
    {
        INF_parseContextFree(ctx);
        INF_Node node = { INF_Node_Invalid.id };
        return node;
    }
    INF_parseContextFree(ctx);
    return node;
}

INF_Node INF_parseAsList(INF_Space* space, const char* src, INF_SpaceSrcInfo* srcInfo)
{
    INF_ParseContext ctx[1] = { INF_newParseContext(space, (u32)strlen(src), src, srcInfo) };
    INF_addSeqEnter(space, INF_NodeType_SeqNaked);
    bool errorHappen = false;
    while (INF_skipSapce(ctx))
    {
        INF_Node e = INF_parseNode(ctx);
        if (INF_Node_Invalid.id == e.id)
        {
            errorHappen = true;
            break;
        }
        INF_addSeqPush(ctx->space, e);
    }
    if (!INF_parseEnd(ctx) || errorHappen)
    {
        INF_parseContextFree(ctx);
        INF_addSeqCancel(space);
        INF_Node node = { INF_Node_Invalid.id };
        return node;
    }
    INF_parseContextFree(ctx);
    INF_Node node = INF_addSeqDone(space);
    if (srcInfo)
    {
        INF_NodeSrcInfo nodeSrcInfo = { srcInfo->fileCount - 1 };
        vec_push(srcInfo->nodes, nodeSrcInfo);
    }
    return node;
}




























































































