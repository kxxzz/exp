#include "inf_a.h"






INF_Space* INF_newSpace(void)
{
    INF_Space* space = zalloc(sizeof(*space));
    space->dataPool = upool_new(256);
    return space;
}

void INF_spaceFree(INF_Space* space)
{
    vec_free(space->cstrBuf);
    vec_free(space->seqDefFrameStack);
    vec_free(space->seqDefStack);
    upool_free(space->dataPool);
    vec_free(space->nodes);
    free(space);
}





void INF_spaceSrcInfoFree(INF_SpaceSrcInfo* srcInfo)
{
    vec_free(srcInfo->nodes);
    vec_free(srcInfo->baseNodeIds);
}


const INF_NodeSrcInfo* INF_nodeSrcInfo(const INF_SpaceSrcInfo* srcInfo, INF_Node node)
{
    return srcInfo->nodes->data + node.id;
}






u32 INF_spaceNodesTotal(const INF_Space* space)
{
    return space->nodes->length;
}








INF_NodeType INF_nodeType(const INF_Space* space, INF_Node node)
{
    INF_NodeInfo* info = space->nodes->data + node.id;
    return info->type;
}









INF_Node INF_addTok(INF_Space* space, const char* str, bool quoted)
{
    u32 len = (u32)strlen(str);
    u32 offset = upool_elm(space->dataPool, str, len + 1, NULL);
    INF_NodeInfo info = { INF_NodeType_Tok, offset, len, quoted };
    INF_Node node = { space->nodes->length };
    vec_push(space->nodes, info);
    return node;
}

INF_Node INF_addTokL(INF_Space* space, const char* str, u32 len, bool quoted)
{
    vec_resize(space->cstrBuf, len + 1);
    memcpy(space->cstrBuf->data, str, len);
    space->cstrBuf->data[len] = 0;
    u32 offset = upool_elm(space->dataPool, space->cstrBuf->data, len + 1, NULL);
    INF_NodeInfo info = { INF_NodeType_Tok, offset, len, quoted };
    INF_Node node = { space->nodes->length };
    vec_push(space->nodes, info);
    return node;
}





void INF_addSeqEnter(INF_Space* space, INF_NodeType type)
{
    assert(type > INF_NodeType_Tok);
    INF_SeqDefFrame f = { type, space->seqDefStack->length };
    vec_push(space->seqDefFrameStack, f);
}

void INF_addSeqPush(INF_Space* space, INF_Node x)
{
    vec_push(space->seqDefStack, x);
}

void INF_addSeqCancel(INF_Space* space)
{
    assert(space->seqDefFrameStack->length > 0);
    INF_SeqDefFrame f = vec_last(space->seqDefFrameStack);
    vec_pop(space->seqDefFrameStack);
    vec_resize(space->seqDefStack, f.p);
}

INF_Node INF_addSeqDone(INF_Space* space)
{
    assert(space->seqDefFrameStack->length > 0);
    INF_SeqDefFrame f = vec_last(space->seqDefFrameStack);
    vec_pop(space->seqDefFrameStack);
    u32 lenSeq = space->seqDefStack->length - f.p;
    INF_Node* seq = space->seqDefStack->data + f.p;
    u32 offset = upool_elm(space->dataPool, seq, sizeof(INF_Node)*lenSeq, NULL);
    INF_NodeInfo nodeInfo = { f.seqType, offset, lenSeq };
    vec_resize(space->seqDefStack, f.p);
    INF_Node node = { space->nodes->length };
    vec_push(space->nodes, nodeInfo);
    return node;
}









u32 INF_tokSize(const INF_Space* space, INF_Node node)
{
    INF_NodeInfo* info = space->nodes->data + node.id;
    assert(INF_NodeType_Tok == info->type);
    return info->length;
}

const char* INF_tokCstr(const INF_Space* space, INF_Node node)
{
    INF_NodeInfo* info = space->nodes->data + node.id;
    assert(INF_NodeType_Tok == info->type);
    return upool_elmData(space->dataPool, info->offset);
}

bool INF_tokQuoted(const INF_Space* space, INF_Node node)
{
    INF_NodeInfo* info = space->nodes->data + node.id;
    assert(INF_NodeType_Tok == info->type);
    return info->quoted;
}




u32 INF_seqLen(const INF_Space* space, INF_Node node)
{
    INF_NodeInfo* info = space->nodes->data + node.id;
    assert(INF_NodeType_Tok < info->type);
    return info->length;
}

const INF_Node* INF_seqElm(const INF_Space* space, INF_Node node)
{
    INF_NodeInfo* info = space->nodes->data + node.id;
    assert(INF_NodeType_Tok < info->type);
    return upool_elmData(space->dataPool, info->offset);
}






bool INF_nodeDataEq(const INF_Space* space, INF_Node a, INF_Node b)
{
    INF_NodeInfo* aInfo = space->nodes->data + a.id;
    INF_NodeInfo* bInfo = space->nodes->data + b.id;
    return 0 == memcmp(aInfo, bInfo, sizeof(*aInfo));
}




















































































































































