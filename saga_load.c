#include "a.h"



typedef enum saga_TokenType
{
    saga_TokenType_NumberBIN,
    saga_TokenType_NumberOCT,
    saga_TokenType_NumberDEC,
    saga_TokenType_NumberHEX,
    saga_TokenType_NumberFloat,

    saga_TokenType_Name,
    saga_TokenType_String,

    saga_TokenType_ListBegin,
    saga_TokenType_ListEnd,

    saga_NumTokenTypes
} saga_TokenType;



typedef struct saga_Token
{
    saga_TokenType type;
    u32 begin;
    u32 len;
} saga_Token;



typedef struct saga_LoadContext
{
    u32 srcLen;
    const char* src;
    u32 cur;
    u32 curLine;
} saga_LoadContext;



static saga_LoadContext saga_newLoadContext(u32 strSize, const char* str)
{
    assert(strSize == strlen(str));
    saga_LoadContext ctx = { strSize, str, 0, 1 };
    return ctx;
}





static bool saga_skipSapce(saga_LoadContext* ctx)
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








static bool saga_readToken_Number(saga_LoadContext* ctx, saga_Token* out)
{
    const char* src = ctx->src;
    saga_Token tok = { saga_TokenType_NumberDEC, ctx->cur, 0 };
    bool floatDoted = false;
    if (ctx->cur + 1 < ctx->srcLen)
    {
        if ('0' == src[ctx->cur])
        {
            ++ctx->cur;
            if ((('b' == src[ctx->cur]) || ('B' == src[ctx->cur])) && (ctx->cur + 1 < ctx->srcLen))
            {
                ++ctx->cur;
                tok.type = saga_TokenType_NumberBIN;
            }
            else if ((('x' == src[ctx->cur] || ('X' == src[ctx->cur])) && (ctx->cur + 1 < ctx->srcLen)))
            {
                ++ctx->cur;
                tok.type = saga_TokenType_NumberHEX;
            }
            else if ('.' == src[ctx->cur])
            {
                ++ctx->cur;
                tok.type = saga_TokenType_NumberFloat;
                floatDoted = true;

            }
            else
            {
                tok.type = saga_TokenType_NumberOCT;
            }
        }
    }
    for (;;)
    {
        if (ctx->cur >= ctx->srcLen)
        {
            break;
        }
        if (saga_TokenType_NumberBIN == tok.type)
        {
            if (('0' > src[ctx->cur]) || ('1' < src[ctx->cur]))
            {
                break;
            }
        }
        else if (saga_TokenType_NumberOCT == tok.type)
        {
            if (('0' > src[ctx->cur]) || ('7' < src[ctx->cur]))
            {
                if ((ctx->cur == tok.begin + 1) && ('0' == src[ctx->cur - 1]))
                {
                    tok.type = saga_TokenType_NumberDEC;
                }
                break;
            }
        }
        else if (saga_TokenType_NumberDEC == tok.type)
        {
            if (('0' > src[ctx->cur]) || ('9' < src[ctx->cur]))
            {
                break;
            }
        }
        else if (saga_TokenType_NumberHEX == tok.type)
        {
            if ((('0' > src[ctx->cur]) || ('9' < src[ctx->cur])) &&
                (('a' > src[ctx->cur]) || ('f' < src[ctx->cur])) &&
                (('A' > src[ctx->cur]) || ('F' < src[ctx->cur])))
            {
                break;
            }
        }
        else if (saga_TokenType_NumberFloat == tok.type)
        {
            if (!floatDoted && ('.' == src[ctx->cur]))
            {
                floatDoted = true;
            }
            if (('0' > src[ctx->cur]) || ('9' < src[ctx->cur]))
            {
                break;
            }
        }
        else
        {
            assert(false);
        }
        ++ctx->cur;
    }
    if (!strchr("[]\"' \t\n\r\b\f", src[ctx->cur]))
    {
        ctx->cur = tok.begin;
        return false;
    }
    tok.len = ctx->cur - tok.begin;
    assert(tok.len > 0);
    *out = tok;
    return true;
}



static bool saga_readToken_String(saga_LoadContext* ctx, saga_Token* out)
{
    const char* src = ctx->src;
    char endCh = src[ctx->cur];
    ++ctx->cur;
    saga_Token tok = { saga_TokenType_String, ctx->cur, 0 };
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



static bool saga_readToken_Name(saga_LoadContext* ctx, saga_Token* out)
{
    saga_Token tok = { saga_TokenType_Name, ctx->cur, 0 };
    const char* src = ctx->src;
    for (;;)
    {
        if (ctx->cur >= ctx->srcLen)
        {
            break;
        }
        else if (strchr("[]\"' \t\n\r\b\f", src[ctx->cur]))
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




static bool saga_readToken(saga_LoadContext* ctx, saga_Token* out)
{
    const char* src = ctx->src;
    if (ctx->cur >= ctx->srcLen)
    {
        return false;
    }
    if (!saga_skipSapce(ctx))
    {
        return false;
    }
    bool ok = false;
    if ('[' == src[ctx->cur])
    {
        saga_Token tok = { saga_TokenType_ListBegin, ctx->cur, 1 };
        *out = tok;
        ++ctx->cur;
        ok = true;
    }
    else if (']' == src[ctx->cur])
    {
        saga_Token tok = { saga_TokenType_ListEnd, ctx->cur, 1 };
        *out = tok;
        ++ctx->cur;
        ok = true;
    }
    else if ((src[ctx->cur] >= '0') && (src[ctx->cur] <= '9'))
    {
        ok = saga_readToken_Number(ctx, out);
        if (!ok)
        {
            ok = saga_readToken_Name(ctx, out);
        }
    }
    else if (('"' == src[ctx->cur]) || ('\'' == src[ctx->cur]))
    {
        ok = saga_readToken_String(ctx, out);
    }
    else
    {
        ok = saga_readToken_Name(ctx, out);
    }
    return ok;
}









static bool saga_loadItem(saga_LoadContext* ctx, saga_Item* out);

static bool saga_loadEnd(saga_LoadContext* ctx)
{
    assert(ctx->srcLen >= ctx->cur);
    if (ctx->srcLen == ctx->cur)
    {
        return true;
    }
    return false;
}
static bool saga_loadListEnd(saga_LoadContext* ctx)
{
    if (saga_loadEnd(ctx))
    {
        return true;
    }
    saga_LoadContext ctx0 = *ctx;
    saga_Token tok;
    if (!saga_readToken(ctx, &tok))
    {
        return true;
    }
    switch (tok.type)
    {
    case saga_TokenType_ListEnd:
    {
        return true;
    }
    default:
        break;
    }
    *ctx = ctx0;
    return false;
}

static bool saga_loadList(saga_LoadContext* ctx, saga_List* list)
{
    bool ok;
    saga_List list1 = { 0 };
    while (!saga_loadListEnd(ctx))
    {
        saga_Item e = { 0 };
        ok = saga_loadItem(ctx, &e);
        if (!ok)
        {
            saga_listFree(&list1);
            return false;
        }
        saga_listAdd(&list1, &e);
    }
    if (list->length > 0)
    {
        saga_listConcat(list, &list1);
        saga_listFree(&list1);
    }
    else
    {
        *list = list1;
    }
    return true;
}



static void saga_loadItemSrcInfo(saga_LoadContext* ctx, const saga_Token* tok, saga_ItemSrcInfo* info)
{
    info->offset = ctx->cur;
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
    info->isStringTok = saga_TokenType_String == tok->type;
    switch (tok->type)
    {
    case saga_TokenType_NumberBIN:
    case saga_TokenType_NumberOCT:
    case saga_TokenType_NumberDEC:
    case saga_TokenType_NumberHEX:
    case saga_TokenType_NumberFloat:
    {
        u32 tokLen = min(tok->len, saga_NameStr_MAX - 1);
        strncpy(info->numStr, ctx->src + tok->begin, tokLen);
        info->numStr[tokLen] = 0;
        break;
    }
    default:
        break;
    }
}



static bool saga_loadItem(saga_LoadContext* ctx, saga_Item* out)
{
    saga_Token tok;
    if (!saga_readToken(ctx, &tok))
    {
        return false;
    }
    saga_ItemSrcInfo srcInfo;
    saga_loadItemSrcInfo(ctx, &tok, &srcInfo);
    bool ok = true;
    switch (tok.type)
    {
    case saga_TokenType_NumberBIN:
    {
        char* end;
        out->type = saga_ItemType_Number;
        out->number = strtol(ctx->src + tok.begin + 2, &end, 2);
        break;
    }
    case saga_TokenType_NumberOCT:
    {
        char* end;
        out->type = saga_ItemType_Number;
        out->number = strtol(ctx->src + tok.begin + 1, &end, 8);
        break;
    }
    case saga_TokenType_NumberDEC:
    {
        char* end;
        out->type = saga_ItemType_Number;
        out->number = strtol(ctx->src + tok.begin, &end, 10);
        break;
    }
    case saga_TokenType_NumberHEX:
    {
        char* end;
        out->type = saga_ItemType_Number;
        out->number = strtol(ctx->src + tok.begin + 2, &end, 16);
        break;
    }
    case saga_TokenType_NumberFloat:
    {
        char* end;
        out->type = saga_ItemType_Number;
        out->number = strtod(ctx->src + tok.begin, &end);
        break;
    }
    case saga_TokenType_Name:
    {
        out->type = saga_ItemType_String;
        const char* src = ctx->src + tok.begin;
        out->stringLen = tok.len;
        out->string = malloc(out->stringLen + 1);
        u32 si = 0;
        for (u32 i = 0; i < tok.len; ++i)
        {
            out->string[si++] = src[i];
        }
        out->string[tok.len] = 0;
        assert(si == out->stringLen);
        break;
    }
    case saga_TokenType_String:
    {
        out->type = saga_ItemType_String;
        char endCh = ctx->src[tok.begin - 1];
        const char* src = ctx->src + tok.begin;
        u32 n = 0;
        for (u32 i = 0; i < tok.len; ++i)
        {
            if ('\\' == src[i])
            {
                ++n;
                ++i;
            }
        }
        out->stringLen = tok.len - n;
        out->string = malloc(out->stringLen + 1);
        u32 si = 0;
        for (u32 i = 0; i < tok.len; ++i)
        {
            if ('\\' == src[i])
            {
                ++i;
                out->string[si++] = src[i];
                continue;
            }
            out->string[si++] = src[i];
        }
        out->string[tok.len] = 0;
        assert(si == out->stringLen);
        break;
    }
    case saga_TokenType_ListBegin:
    {
        out->type = saga_ItemType_List;
        memset(&out->list, 0, sizeof(out->list));
        ok = saga_loadList(ctx, &out->list);
        break;
    }
    default:
        assert(false);
        return false;
    }
    out->hasSrcInfo = true;
    out->srcInfo = srcInfo;
    return ok;
}











bool saga_load(saga_List* list, const char* str, u32 strSize)
{
    saga_LoadContext ctx = saga_newLoadContext(strSize, str);
    bool ok = saga_loadList(&ctx, list);
    return ok;
}


























































































