#include "exp_a.h"






EXP_Space* EXP_newSpace(void)
{
    EXP_Space* space = zalloc(sizeof(*space));
    space->dataPool = upool_new(256);
    return space;
}

void EXP_spaceFree(EXP_Space* space)
{
    vec_free(&space->cstrBuf);
    vec_free(&space->seqDefFrameStack);
    vec_free(&space->seqDefStack);
    upool_free(space->dataPool);
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






u32 EXP_spaceNodesTotal(const EXP_Space* space)
{
    return space->nodes.length;
}








EXP_NodeType EXP_nodeType(const EXP_Space* space, EXP_Node node)
{
    EXP_NodeInfo* info = space->nodes.data + node.id;
    return info->type;
}









EXP_Node EXP_addTok(EXP_Space* space, const char* str, bool quoted)
{
    u32 len = (u32)strlen(str);
    u32 offset = upool_elm(space->dataPool, str, len + 1, NULL);
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
    u32 offset = upool_elm(space->dataPool, space->cstrBuf.data, len + 1, NULL);
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
    u32 offset = upool_elm(space->dataPool, seq, sizeof(EXP_Node)*lenSeq, NULL);
    EXP_NodeInfo nodeInfo = { f.seqType, offset, lenSeq };
    vec_resize(&space->seqDefStack, f.p);
    EXP_Node node = { space->nodes.length };
    vec_push(&space->nodes, nodeInfo);
    return node;
}









u32 EXP_tokSize(const EXP_Space* space, EXP_Node node)
{
    EXP_NodeInfo* info = space->nodes.data + node.id;
    assert(EXP_NodeType_Tok == info->type);
    return info->length;
}

const char* EXP_tokCstr(const EXP_Space* space, EXP_Node node)
{
    EXP_NodeInfo* info = space->nodes.data + node.id;
    assert(EXP_NodeType_Tok == info->type);
    return upool_elmData(space->dataPool, info->offset);
}

bool EXP_tokQuoted(const EXP_Space* space, EXP_Node node)
{
    EXP_NodeInfo* info = space->nodes.data + node.id;
    assert(EXP_NodeType_Tok == info->type);
    return info->quoted;
}




u32 EXP_seqLen(const EXP_Space* space, EXP_Node node)
{
    EXP_NodeInfo* info = space->nodes.data + node.id;
    assert(EXP_NodeType_Tok < info->type);
    return info->length;
}

const EXP_Node* EXP_seqElm(const EXP_Space* space, EXP_Node node)
{
    EXP_NodeInfo* info = space->nodes.data + node.id;
    assert(EXP_NodeType_Tok < info->type);
    return upool_elmData(space->dataPool, info->offset);
}






bool EXP_nodeDataEq(const EXP_Space* space, EXP_Node a, EXP_Node b)
{
    EXP_NodeInfo* aInfo = space->nodes.data + a.id;
    EXP_NodeInfo* bInfo = space->nodes.data + b.id;
    return 0 == memcmp(aInfo, bInfo, sizeof(*aInfo));
}




















































































































































