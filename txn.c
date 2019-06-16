#include "txn_a.h"






TXN_Space* TXN_spaceNew(void)
{
    TXN_Space* space = zalloc(sizeof(*space));
    space->dataPool = upool_new(256);
    return space;
}

void TXN_spaceFree(TXN_Space* space)
{
    vec_free(space->cstrBuf);
    upool_free(space->dataPool);
    vec_free(space->nodes);
    free(space);
}





void TXN_spaceSrcInfoFree(TXN_SpaceSrcInfo* srcInfo)
{
    vec_free(srcInfo->nodes);
    vec_free(srcInfo->fileBases);
}


const TXN_NodeSrcInfo* TXN_nodeSrcInfo(const TXN_SpaceSrcInfo* srcInfo, TXN_Node node)
{
    return srcInfo->nodes->data + node.id;
}






u32 TXN_spaceNodesTotal(const TXN_Space* space)
{
    return space->nodes->length;
}








TXN_NodeType TXN_nodeType(const TXN_Space* space, TXN_Node node)
{
    TXN_NodeInfo* info = space->nodes->data + node.id;
    return info->type;
}









TXN_Node TXN_tokFromCstr(TXN_Space* space, const char* str, bool quoted)
{
    u32 len = (u32)strlen(str);
    u32 offset = upool_elm(space->dataPool, str, len + 1, NULL);
    TXN_NodeInfo info = { TXN_NodeType_Tok, offset, len, quoted };
    TXN_Node node = { space->nodes->length };
    vec_push(space->nodes, info);
    return node;
}

TXN_Node TXN_tokFromLenStr(TXN_Space* space, const char* str, u32 len, bool quoted)
{
    vec_resize(space->cstrBuf, len + 1);
    memcpy(space->cstrBuf->data, str, len);
    space->cstrBuf->data[len] = 0;
    u32 offset = upool_elm(space->dataPool, space->cstrBuf->data, len + 1, NULL);
    TXN_NodeInfo info = { TXN_NodeType_Tok, offset, len, quoted };
    TXN_Node node = { space->nodes->length };
    vec_push(space->nodes, info);
    return node;
}







TXN_Node TXN_seqNew(TXN_Space* space, TXN_NodeType type, const TXN_Node* elms, u32 len)
{
    u32 offset = upool_elm(space->dataPool, elms, sizeof(TXN_Node)*len, NULL);
    TXN_NodeInfo nodeInfo = { type, offset, len };
    TXN_Node node = { space->nodes->length };
    vec_push(space->nodes, nodeInfo);
    return node;
}









u32 TXN_tokSize(const TXN_Space* space, TXN_Node node)
{
    TXN_NodeInfo* info = space->nodes->data + node.id;
    assert(TXN_NodeType_Tok == info->type);
    return info->length;
}

const char* TXN_tokCstr(const TXN_Space* space, TXN_Node node)
{
    TXN_NodeInfo* info = space->nodes->data + node.id;
    assert(TXN_NodeType_Tok == info->type);
    return upool_elmData(space->dataPool, info->offset);
}

bool TXN_tokQuoted(const TXN_Space* space, TXN_Node node)
{
    TXN_NodeInfo* info = space->nodes->data + node.id;
    assert(TXN_NodeType_Tok == info->type);
    return info->quoted;
}




u32 TXN_seqLen(const TXN_Space* space, TXN_Node node)
{
    TXN_NodeInfo* info = space->nodes->data + node.id;
    assert(TXN_NodeType_Tok < info->type);
    return info->length;
}

const TXN_Node* TXN_seqElm(const TXN_Space* space, TXN_Node node)
{
    TXN_NodeInfo* info = space->nodes->data + node.id;
    assert(TXN_NodeType_Tok < info->type);
    return upool_elmData(space->dataPool, info->offset);
}






bool TXN_nodeDataEq(const TXN_Space* space, TXN_Node a, TXN_Node b)
{
    TXN_NodeInfo* aInfo = space->nodes->data + a.id;
    TXN_NodeInfo* bInfo = space->nodes->data + b.id;
    return 0 == memcmp(aInfo, bInfo, sizeof(*aInfo));
}




















































































































































