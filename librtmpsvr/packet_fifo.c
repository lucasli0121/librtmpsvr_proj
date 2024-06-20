/******************************************************************************
 * Author: liguoqiang
 * Date: 2024-02-16 13:49:20
 * LastEditors: liguoqiang
 * LastEditTime: 2024-04-15 23:13:07
 * Description: 
********************************************************************************/
#include "packet_fifo.h"
#include "utils.h"

static void initFifo(FifoAgent* fifo)
{
    fifo->packetFifoHeader = NULL;
    fifo->packetFifoTail = NULL;
    fifo->size = 0;
    fifo->fifoMutex = createMutex();
}
static void uninitFifo(FifoAgent* fifo)
{
    fifo->fifoMutex->lock(&fifo->fifoMutex->mutex);
    PacketFifo *p = fifo->packetFifoHeader;
    while(p) {
        PacketFifo *next = p->next;
        RTMPPacket_Free(p->packet);
        free(p->packet);
        free(p);
        p = next;
    }
    fifo->fifoMutex->unlock(&fifo->fifoMutex->mutex);
    if(fifo->fifoMutex) {
        destroyMutex(fifo->fifoMutex);
        fifo->fifoMutex = 0;
    }
}
static void packetFifoPush(FifoAgent* agent, RTMPPacket *packet)
{
    if(packet == NULL) {
        return;
    }
    PacketFifo *fifoObj = (PacketFifo*)malloc(sizeof(PacketFifo) + 16);
    if(fifoObj == NULL) {
        return;
    }
    fifoObj->packet = packet;
    fifoObj->next = NULL;
    fifoObj->t = getMSecOfTime();
    agent->fifoMutex->lock(&agent->fifoMutex->mutex);
    if(agent->packetFifoTail == NULL) {
        agent->packetFifoTail = fifoObj;
    } else {
        agent->packetFifoTail->next = fifoObj;
        if(agent->packetFifoHeader == NULL) {
            agent->packetFifoHeader = agent->packetFifoTail;
        }
        agent->packetFifoTail = agent->packetFifoTail->next;
    }
    agent->size++;
    agent->fifoMutex->unlock(&agent->fifoMutex->mutex);
}
static RTMPPacket* packetFifoFront(FifoAgent* agent)
{
    RTMPPacket *packet = NULL;
    agent->fifoMutex->lock(&agent->fifoMutex->mutex);
    if(agent->packetFifoHeader == NULL) {
        agent->packetFifoHeader = agent->packetFifoTail;
    }
    if(agent->packetFifoHeader) {
        packet = agent->packetFifoHeader->packet;
    }
    agent->fifoMutex->unlock(&agent->fifoMutex->mutex);
    return packet;
}
static float packetFifoPop(FifoAgent* agent)
{
    float t = 0;
    agent->fifoMutex->lock(&agent->fifoMutex->mutex);
    if(agent->packetFifoHeader == NULL) {
        agent->packetFifoHeader = agent->packetFifoTail;
    }
    if(agent->packetFifoHeader) {
        if(agent->packetFifoHeader == agent->packetFifoTail) {
            agent->packetFifoTail = NULL;
        }
        PacketFifo *fifo = agent->packetFifoHeader;
        t = fifo->t;
        agent->packetFifoHeader = agent->packetFifoHeader->next;
        RTMPPacket_Free(fifo->packet);
        free(fifo->packet);
        free(fifo);
        agent->size--;
    }
    agent->fifoMutex->unlock(&agent->fifoMutex->mutex);
    return t;
}

static void cleanPacketFifo(FifoAgent* agent)
{
    agent->fifoMutex->lock(&agent->fifoMutex->mutex);
    PacketFifo *p = agent->packetFifoHeader;
    while(p) {
        PacketFifo *next = p->next;
        RTMPPacket_Free(p->packet);
        free(p->packet);
        free(p);
        p = next;
    }
    agent->packetFifoHeader = NULL;
    agent->packetFifoTail = NULL;
    agent->size = 0;
    agent->fifoMutex->unlock(&agent->fifoMutex->mutex);
}
/******************************************************************************
 * function: 
 * description: 
 * return {*}
********************************************************************************/
FifoAgent* createFifoAgent()
{
    FifoAgent *fifoAgent = (FifoAgent*)malloc(sizeof(FifoAgent) + 16);
    if(fifoAgent == NULL) {
        return NULL;
    }
    fifoAgent->initFifo = initFifo;
    fifoAgent->uninitFifo = uninitFifo;
    fifoAgent->pushFifo = packetFifoPush;
    fifoAgent->frontFifo = packetFifoFront;
    fifoAgent->popFifo = packetFifoPop;
    fifoAgent->clearFifo = cleanPacketFifo;
    fifoAgent->initFifo(fifoAgent);
    return fifoAgent;
}
/******************************************************************************
 * function: 
 * description: 
 * param {FifoAgent} *fifoAgent
 * return {*}
********************************************************************************/
void releaseFifoAgent(FifoAgent *fifoAgent)
{
    fifoAgent->uninitFifo(fifoAgent);
    free(fifoAgent);
}