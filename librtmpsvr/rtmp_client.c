/******************************************************************************
 * Author: liguoqiang
 * Date: 2024-01-16 10:19:41
 * LastEditors: liguoqiang
 * LastEditTime: 2024-05-17 21:48:54
 * Description: 
********************************************************************************/
#include "rtmp_client.h"
#include "librtmp/log.h"
#include "thread.h"
#include "utils.h"

#define STR2AVAL(av, str) \
  av.av_val = str;        \
  av.av_len = strlen(av.av_val)

#define SAVC(x) static const AVal av_##x = AVC(#x)

static FILE * _logFile = NULL;
static int _rtmpChunkSize = 4096;

/******************************************************************************
 * function: sendChunkSize
 * description: reponse chunk size to client
 * param {RTMP} *r
 * return {*}
********************************************************************************/
static int sendChunkSize(RTMP *r, int chunkSize) {
    RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, sendChunkSize, size=%d", __FUNCTION__, chunkSize);
    RTMPPacket packet;
    char pbuf[384], *pend = pbuf + sizeof(pbuf);

    packet.m_nChannel = 0x02;   // flow control channel
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet.m_packetType = 0x1; // chunk size
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = 0;
    packet.m_hasAbsTimestamp = 0;
    packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

    char *enc = packet.m_body;
    r->m_outChunkSize = chunkSize;
    enc = AMF_EncodeInt32(enc, pend, chunkSize);//RTMP_DEFAULT_CHUNKSIZE);

    packet.m_nBodySize = enc - packet.m_body;
    return RTMP_SendPacket(r, &packet, FALSE);
}

void changeChunkSize(RTMP_CLIENT *client, int size)
{
    if(client == NULL || client->rtmp == NULL) {
        return;
    }
    _rtmpChunkSize = size;
    if(!sendChunkSize(client->rtmp, size)) {
        RTMP_Log(client->rtmp->logCtx, RTMP_LOGERROR, "%s, change chunk size failed, size=%d", __FUNCTION__, size);
    }
}

typedef struct
{
  char encoder[64];
  int duration;
  int width;
  int height;
  int fps;
  char audioid[32];
  int audioVolume;
} MetaData;
static MetaData _metaData = {0};
static const AVal av_setDataFrame = AVC("@setDataFrame");
SAVC(onMetaData);
SAVC(duration);
SAVC(width);
SAVC(height);
SAVC(framerate);
SAVC(encoder);
SAVC(filesize);
SAVC(audiocodecid);
SAVC(audioinputvolume);
static RTMPPacket* getMetaDataPacket(RTMP_CLIENT *client)
{
    RTMP * rtmp = client->rtmp;
    if(rtmp == NULL) {
        return NULL;
    }
    if(client->metaDataSize <= 0 || client->metaDataBuf == NULL) {
        return NULL;
    }

    RTMPPacket *packet = (RTMPPacket *)malloc(sizeof(RTMPPacket) + RTMP_MAX_HEADER_SIZE);
    if(!packet) {
        return NULL;
    }
    // RTMPPacket_Alloc(packet, client->metaDataSize + RTMP_MAX_HEADER_SIZE);
    RTMPPacket_Alloc(packet, 2048);
    RTMPPacket_Reset(packet);
    packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet->m_packetType = RTMP_PACKET_TYPE_INFO;
    packet->m_nChannel = 0x04;
    packet->m_nTimeStamp = 0;
    packet->m_hasAbsTimestamp = 0;
    packet->m_nInfoField2 = rtmp->m_stream_id;
    char *enc = packet->m_body;
    char *pend = enc + 2048;//client->metaDataSize;
    // memcpy(enc, client->metaDataBuf, client->metaDataSize);
    // enc += client->metaDataSize;
    // packet->m_nBodySize = enc - packet->m_body;
    // return packet;
    enc = AMF_EncodeString(enc, pend, &av_setDataFrame);
    enc = AMF_EncodeString(enc, pend, &av_onMetaData);
    *enc++ = AMF_ECMA_ARRAY;
    enc = AMF_EncodeInt32(enc, pend, 3);
    // enc = AMF_EncodeNamedNumber(enc, pend, &av_duration, _metaData.duration);
    enc = AMF_EncodeNamedNumber(enc, pend, &av_width, _metaData.width);
    enc = AMF_EncodeNamedNumber(enc, pend, &av_height, _metaData.height);
    enc = AMF_EncodeNamedNumber(enc, pend, &av_framerate, _metaData.fps);
    AVal encoder;
    STR2AVAL(encoder, _metaData.encoder);
    // enc = AMF_EncodeNamedString(enc, pend, &av_encoder, &encoder);
    // enc = AMF_EncodeNamedNumber(enc, pend, &av_filesize, 0);
    AVal audioid;
    STR2AVAL(audioid, _metaData.audioid);
    // enc = AMF_EncodeNamedString(enc, pend, &av_audiocodecid, &audioid);
    // enc = AMF_EncodeNamedNumber(enc, pend, &av_audioinputvolume, _metaData.audioVolume);
    *enc++ = 0;
    *enc++ = 0;
    *enc++ = AMF_OBJECT_END;
    packet->m_nBodySize = enc - packet->m_body;
    return packet;
}

static char* packetSPSData(char* sps, int sps_size, char *buf)
{
    if(buf == NULL || sps == NULL || sps_size <= 0) {
        return NULL;
    }
    char *p = buf;
    *p++ = 0x17;
    *p++ = 0x00; // avc pack type
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x00;
    *p++ = 0x01; //version
    *p++ = sps[1]; //profile
    *p++ = sps[2]; //compatibility
    *p++ = sps[3]; //level
    *p++ = 0xff; //nal size
    *p++ = 0xe1; //sps num
    *p++ = (sps_size >> 8) & 0xff; //sps size
    *p++ = sps_size & 0xff;
    memcpy(p, sps, sps_size);
    p += sps_size;
    return p;
}
static char* packetPPSData(char* pps, int pps_size, char* buf)
{
    if(buf == NULL || pps == NULL || pps_size <= 0) {
        return NULL;
    }
    char *p = buf;
    *p++ = 0x01;
    *p++ = (pps_size >> 8) & 0xff; //pps size
    *p++ = pps_size & 0xff;
    memcpy(p, pps, pps_size);
    p += pps_size;
    return p;
}
static RTMPPacket* getSpsAndPps(RTMP_CLIENT *client)
{
    RTMP* rtmp = client->rtmp;
    SpsData* spsData = client->spsData;
    if(spsData == NULL || spsData->sps == NULL || spsData->sps_size <= 0 || spsData->pps == NULL || spsData->pps_size <= 0) {
        return NULL;
    }
    RTMPPacket *packSps = (RTMPPacket *)malloc(sizeof(RTMPPacket) + RTMP_MAX_HEADER_SIZE);
    if(!packSps) {
        return NULL;
    }
    packSps->m_packetType = RTMP_PACKET_TYPE_VIDEO;
    packSps->m_nTimeStamp = 0;
    packSps->m_hasAbsTimestamp = 0;
    packSps->m_nChannel = 0x06;
    packSps->m_headerType = RTMP_PACKET_SIZE_LARGE;
    packSps->m_nInfoField2 = rtmp->m_stream_id;
    int body_size = spsData->sps_size + 13 + spsData->pps_size + 3;
    RTMPPacket_Alloc(packSps, body_size);
    char *p = packSps->m_body;
    p = packetSPSData(spsData->sps, spsData->sps_size, p);
    p = packetPPSData(spsData->pps, spsData->pps_size, p);
    packSps->m_nBodySize = body_size;
    return packSps;
}
/******************************************************************************
 * function: reconnectSocket
 * description: only close socket and connect again
 * no any protocol handshake
 * param {RTMP*} r
 * return {*}
********************************************************************************/
static int connectSocket(RTMP* r)
{
    if (RTMP_IsConnected(r)) {
        RTMPSockBuf_Close(&r->m_sb);
    }
    if (RTMP_Connect(r, NULL) == 0) {
        RTMP_Log(r->logCtx, RTMP_LOGERROR, "%s, rtmp_connect error", __FUNCTION__);
        return -1;
    }
    return 0;
}
static int connectStream(RTMP_CLIENT *client)
{
    RTMP* rtmp = client->rtmp;
    if(rtmp == NULL) {
        return -1;
    }
    if(connectSocket(rtmp) == -1) {
        return -1;
    }
    rtmp->Link.timeout = 10;
    rtmp->Link.lFlags |= RTMP_LF_LIVE;
    RTMP_SetupURL(rtmp, client->rtmpUrl);
    RTMP_EnableWrite(rtmp);
    sendChunkSize(rtmp, _rtmpChunkSize);
    if (RTMP_ConnectStream(rtmp, 0) == 0) {
        RTMP_Log(rtmp->logCtx, RTMP_LOGERROR, "%s, rtmp_connect_stream error", __FUNCTION__);
        RTMP_Close(rtmp);
        return -1;
    }
    return 0;
}
static int sendSpsPacket(RTMP_CLIENT *client)
{
    RTMP* rtmp = client->rtmp;
    int ret = 0;
    RTMPPacket * packSps = getSpsAndPps(client);
    if(packSps) {
        if (!RTMP_SendPacket(rtmp, packSps, FALSE)) {
            ret = -1;
        }
        RTMPPacket_Free(packSps);
        free(packSps);
    }
    return ret;
}
static int sendMetaDataPacket(RTMP_CLIENT *client)
{
    RTMP* rtmp = client->rtmp;
    int ret = 0;
    RTMPPacket * packMeta = getMetaDataPacket(client);
    if(packMeta) {
        if (!RTMP_SendPacket(rtmp, packMeta, FALSE)) {
            ret = -1;
        }
        RTMPPacket_Free(packMeta);
        free(packMeta);
    }
    return ret;
}

/******************************************************************************
 * function: clientStreamThread
 * description: 
 * param {void} *v
 * return {*}
 * 
 * *************************************************************************/
static TFTYPE clientStreamThread(void* v)
{
    RTMP_CLIENT* client = (RTMP_CLIENT*)v;
    RTMP* rtmp = client->rtmp;
#ifdef ANDROID
    int64_t cores = sysconf(_SC_NPROCESSORS_CONF);
    RTMP_Log(rtmp->logCtx, RTMP_LOGDEBUG, "CPUS: %lu\n", cores);
    bindToCpu(cores - 1);
#elif defined(linux)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    for (int i = 0; i < __CPU_SETSIZE; i++) {
        if(CPU_ISSET(i, &cpuset)) {
            RTMP_Log(rtmp->logCtx, RTMP_LOGDEBUG, "thread is running in cpu %d\n", i);
        }
    }
#endif
    RTMPPacket *packet = 0;
    client->hasSendSps = 0;
    client->hasSendMeta = 0;
    while(client->runThread) {
        connectStream(client);
        client->hasSendSps = 0;
        client->hasSendMeta = 0;
        while (rtmp->m_bPlaying && client->runThread && RTMP_IsConnected(rtmp)) {
            RTMP_Log(rtmp->logCtx, RTMP_LOGDEBUG, "%s, ready call frontFifo", __FUNCTION__);
            packet = client->dataFifo->frontFifo(client->dataFifo);
            if(packet) {
                int type = packet->m_packetType;
                RTMP_Log(rtmp->logCtx, RTMP_LOGDEBUG, "%s, get a package from fifo", __FUNCTION__);
                if(client->dataFifo->size > 25*3 && type == 9) {
                    if(packet->m_body[0] == 0x27) {
                        client->dataFifo->popFifo(client->dataFifo);
                        RTMP_Log(rtmp->logCtx, RTMP_LOGINFO, "%s, the fifo size > 3*25 and type=%d and it is not key-frame drop it!", __FUNCTION__, type);
                        continue;
                    }
                }
                if (client->hasSendMeta == 0) {
                    if(sendMetaDataPacket(client) == -1) {
                        RTMP_Log(rtmp->logCtx, RTMP_LOGERROR, "%s, send meta data packet error", __FUNCTION__);
                        break;
                    }
                    client->hasSendMeta = 1;
                }
                if(packet->m_packetType == RTMP_PACKET_TYPE_VIDEO && client->hasSendSps == 0) {
                    if(sendSpsPacket(client) == -1) {
                        RTMP_Log(rtmp->logCtx, RTMP_LOGERROR, "%s, send sps data packet error", __FUNCTION__);
                        break;
                    }
                    client->hasSendSps = 1;
                }
                RTMP_Log(rtmp->logCtx, RTMP_LOGDEBUG, "%s, ready send a packet to remote", __FUNCTION__);
                
                if (!RTMP_SendPacket(rtmp, packet, FALSE)) {
                    RTMP_Log(rtmp->logCtx, RTMP_LOGERROR, "%s, send video packet error", __FUNCTION__);
                    if(RTMP_IsTimedout(rtmp)) {
                        RTMP_Log(rtmp->logCtx, RTMP_LOGERROR, "%s, send video packet timeout will reconnect and resend again", __FUNCTION__);
                        connectSocket(rtmp);
                        continue;
                    }
                    break;
                }
                RTMP_Log(rtmp->logCtx, RTMP_LOGDEBUG, "%s, ready pop a packet from fifo", __FUNCTION__);
                float t1 = client->dataFifo->popFifo(client->dataFifo);
                float t2 = getMSecOfTime();
                float delay = t2-t1;
                RTMP_Log(rtmp->logCtx, RTMP_LOGDEBUG, "send a rtmp packet ok, type=%d, and fifo size=%d, t1=%.2f, t2=%.2f, delay time:%.2f", type, client->dataFifo->size, t1, t2, delay);
                // sleep_m(0);
            } else {
                RTMP_Log(rtmp->logCtx, RTMP_LOGDEBUG, "%s, fifo is empty, sleep 20ms", __FUNCTION__);
                sleep_m(20);
            }
        }
        RTMP_Log(rtmp->logCtx, RTMP_LOGINFO, "%s, rtmp is not connected, try to reconnect", __FUNCTION__);
        RTMP_Close(rtmp);
        sleep_m(100);
    }
    RTMP_Log(rtmp->logCtx, RTMP_LOGINFO, "%s, thread exit!!!", __FUNCTION__);
    return TFRET();
}
/******************************************************************************
 * function: beforeSendData
 * description: 
 * param {RTMP} *rtmp
 * return {*}
********************************************************************************/
void beforeSendData(RTMP_CLIENT *client)
{
    if(client == NULL) {
        return;
    }
    if(client->spsData->sps) {
        free(client->spsData->sps);
        client->spsData->sps = 0;
    }
    client->spsData->sps_size = 0;
    if(client->spsData->pps) {
        free(client->spsData->pps);
        client->spsData->pps = 0;
    }
    client->spsData->pps_size = 0;
    client->dataFifo->clearFifo(client->dataFifo);
    client->hasSendMeta = 0;
    client->hasSendSps = 0;
    RTMP_Close(client->rtmp);
}
static void initRtmpClient(RTMP_CLIENT *client)
{
    if(client) {
        client->dataFifo = createFifoAgent();
        client->metaDataBuf = 0;
        client->spsData = (SpsData*)malloc(sizeof(SpsData));
        client->spsData->sps = 0;
        client->spsData->sps_size = 0;
        client->spsData->pps = 0;
        client->spsData->pps_size = 0;
    }
}
static void uninitRtmpClient(RTMP_CLIENT *client)
{
    if(client) {
        if(client->metaDataBuf) {
            free(client->metaDataBuf);
        }
        if(client->spsData) {
            if(client->spsData->sps) {
                free(client->spsData->sps);
                client->spsData->sps = 0;
            }
            if(client->spsData->pps) {
                free(client->spsData->pps);
                client->spsData->pps = 0;
            }
            free(client->spsData);
            client->spsData = 0;
        }
        if(client->dataFifo) {
            releaseFifoAgent(client->dataFifo);
            client->dataFifo = 0;
        }
    }
}


/******************************************************************************
 * function: 
 * description: 
 * param {char} *url
 * return {*}
********************************************************************************/
RTMP_CLIENT* startRtmpClient(char *url, char* logFileName)
{
    if(url == NULL) {
        return NULL;
    }
    RTMP *rtmp = NULL;
    RTMP_CLIENT* client = (RTMP_CLIENT*)malloc(sizeof(RTMP_CLIENT));
    if (!client) {
        goto error;
    }
    memset(client, 0, sizeof(RTMP_CLIENT));
    rtmp = RTMP_Alloc();
    if(!rtmp) {
        goto error;
    }
    RTMP_Init(rtmp);
    if(logFileName && strlen(logFileName) > 0) {
        _logFile = fopen(logFileName, "w");
    }
    client->rtmp = rtmp;
    rtmp->logCtx = RTMP_LogInit();
    rtmp->logCtx->file = _logFile;
    RTMP_LogSetLevel(rtmp->logCtx, RTMP_LOGERROR);
    rtmp->Link.timeout = 30;
    rtmp->Link.lFlags |= RTMP_LF_LIVE;
    strncpy(client->rtmpUrl, url, sizeof(client->rtmpUrl));
    if (RTMP_SetupURL(rtmp, client->rtmpUrl) == 0) {
        RTMP_Log(rtmp->logCtx, RTMP_LOGERROR, "%s, set push url error", __FUNCTION__);
        goto error;
    }
    RTMP_EnableWrite(rtmp);
    if (RTMP_Connect(rtmp, NULL) == 0) {
        RTMP_Log(rtmp->logCtx, RTMP_LOGERROR, "%s, rtmp_connect error", __FUNCTION__);
        goto error;
    }
    initRtmpClient(client);
    client->runThread = 1;
    client->clientThreadId = ThreadCreate(clientStreamThread, client);
    return client;
error:
    if (client) {
        if(client->dataFifo) {
            releaseFifoAgent(client->dataFifo);
        }
        free(client);
    }
    RTMP_LogUninit(rtmp->logCtx);
    if(rtmp) {
        RTMP_Free(rtmp);
    }
    if(_logFile) {
        fclose(_logFile);
    }
    return NULL;
}

void stopRtmpClient(RTMP_CLIENT *client)
{
    client->runThread = 0;
    RTMP* rtmp = client->rtmp;
    rtmp->m_bPlaying = FALSE;
    if (client->clientThreadId != 0) {
        ThreadJoin(client->clientThreadId);
        client->clientThreadId = 0;
    }
    uninitRtmpClient(client);
    RTMP_LogUninit(rtmp->logCtx);
    if(rtmp) {
        RTMP_Close(rtmp);
        RTMP_Free(rtmp);
    }
    if (_logFile) {
        fclose(_logFile);
        _logFile = 0;
    }
}

void setMetaData(RTMP_CLIENT *client, char* data, int size)
{
    if(client->metaDataBuf) {
        free(client->metaDataBuf);
        client->metaDataBuf = 0;
    }
    client->metaDataSize = size;
    client->metaDataBuf = (char*)malloc(size + 64);
    memcpy(client->metaDataBuf, data, size);
}

int sendRawVideoData(RTMP_CLIENT *client, const char *data, int size, unsigned int timestamp, int absTimestamp)
{
    if(client == NULL || client->rtmp == NULL || data == NULL || size <= 0) {
        return -1;
    }
    RTMP *rtmp = client->rtmp;
    RTMPPacket *packet = (RTMPPacket *)malloc(sizeof(RTMPPacket) + RTMP_MAX_HEADER_SIZE);
    if(!packet) {
        return -1;
    }
    RTMPPacket_Reset(packet);
    packet->m_packetType = RTMP_PACKET_TYPE_VIDEO;
    packet->m_nTimeStamp = timestamp;
    packet->m_hasAbsTimestamp = absTimestamp;// ? 0 : 1;
    packet->m_nChannel = 0x06;
    packet->m_headerType = absTimestamp ? RTMP_PACKET_SIZE_LARGE : RTMP_PACKET_SIZE_MEDIUM;
    packet->m_nInfoField2 = rtmp->m_stream_id;

    RTMPPacket_Alloc(packet, size);
    packet->m_nBodySize = size;
    memcpy(packet->m_body, data, size);
    client->dataFifo->pushFifo(client->dataFifo, packet);
}
/******************************************************************************
 * function: 
 * description: 
 * param {RTMP} *rtmp
 * param {char} *data
 * param {int} size
 * param {unsigned int} timestamp
 * return {*}
********************************************************************************/
int sendX264VideoData(RTMP_CLIENT *client, const char *data, int size, unsigned int timestamp, int absTimestamp, int key)
{
    if(client == NULL || client->rtmp == NULL || data == NULL || size <= 0) {
        return -1;
    }
    RTMP *rtmp = client->rtmp;
    SpsData *spsData = client->spsData;
    RTMPPacket *packet = (RTMPPacket *)malloc(sizeof(RTMPPacket) + RTMP_MAX_HEADER_SIZE);
    if(!packet) {
        return -1;
    }
    int ret = 0;
    char *p = 0;
    char *ptr = 0;
    unsigned int val = 0;
    int zore_count = 0;
    int body_size = 0;
    unsigned char keyframe = 0x27;

    RTMP_Log(rtmp->logCtx, RTMP_LOGDEBUG, "%s, video data, size:%d, timestamp:%d, abstimestamp:%d", __FUNCTION__, size, timestamp, absTimestamp);
    RTMPPacket_Reset(packet);
    packet->m_packetType = RTMP_PACKET_TYPE_VIDEO;
    packet->m_nTimeStamp = timestamp;
    packet->m_hasAbsTimestamp = absTimestamp;// ? 0 : 1;
    packet->m_nChannel = 0x06;
    packet->m_headerType = absTimestamp ? RTMP_PACKET_SIZE_LARGE : RTMP_PACKET_SIZE_MEDIUM;
    packet->m_nInfoField2 = rtmp->m_stream_id;
    
    ptr = (char*)data;
    if(memcmp(ptr, "\x00\x00\x00\x01", 4) == 0) {
        ptr += 4;
        size -= 4;
    } else if(memcmp(ptr, "\x00\x00\x01", 3) == 0) {
        ptr += 3;
        size -= 3;
    } else {
        RTMP_Log(rtmp->logCtx, RTMP_LOGERROR, "%s, not 264 type", __FUNCTION__);
        ret = -1;
        return ret;
    }
    if (*ptr == 0x67) { //SPS
        RTMP_Log(rtmp->logCtx, RTMP_LOGDEBUG, "%s, first byte=0x67, is sps", __FUNCTION__);
        zore_count = 0;
        spsData->sps_size = 0;
        spsData->pps_size = 0;
        if(spsData->sps) {
            free(spsData->sps);
            spsData->sps = 0;
        }
        if(spsData->pps) {
            free(spsData->pps);
            spsData->pps = 0;
        }
        for (spsData->sps_size = 0; spsData->sps_size < size; spsData->sps_size++) {
            if (ptr[spsData->sps_size] == 0x00 || ptr[spsData->sps_size] == 0x01) {
                zore_count++;
            } else {
                zore_count = 0;
            }
            if (ptr[spsData->sps_size] == 0x01 && zore_count >= 3) {
                spsData->sps_size -= (zore_count - 1);
                break;
            }
        }
        if (spsData->sps_size > 0) {
            spsData->sps = (char*)malloc(spsData->sps_size + 64);
            memcpy(spsData->sps, ptr, spsData->sps_size);
        }
        ptr += (spsData->sps_size + zore_count);
        size -= (spsData->sps_size + zore_count);
    }
    if (size > 0 && *ptr == 0x68) { //PPS
        spsData->pps_size = 0;
        zore_count = 0;
        if(spsData->pps) {
            free(spsData->pps);
            spsData->pps = 0;
        }
        for (spsData->pps_size = 0; spsData->pps_size < size; spsData->pps_size++) {
            if (ptr[spsData->pps_size] == 0x00 || ptr[spsData->pps_size] == 0x01) {
                zore_count++;
            } else {
                zore_count = 0;
            }
            if (ptr[spsData->pps_size] == 0x01 && zore_count >= 3) {
                spsData->pps_size -= (zore_count - 1);
                break;
            }
        }
        if (spsData->pps_size > 0) {
            spsData->pps = (char*)malloc(spsData->pps_size + 64);
            memcpy(spsData->pps, ptr, spsData->pps_size);
        }
        ptr += (spsData->pps_size + zore_count);
        size -= (spsData->pps_size + zore_count);
        RTMP_Log(rtmp->logCtx, RTMP_LOGDEBUG, "%s, sps size:%d, pps size:%d, sum=%d", __FUNCTION__, spsData->sps_size, spsData->pps_size, spsData->sps_size + spsData->pps_size);
    }
    
    if (size > 0) {
        RTMP_Log(rtmp->logCtx, RTMP_LOGDEBUG, "%s, frame flag: %02x ", __FUNCTION__, *ptr);
        if (*ptr == 0x65 || key == 1) { // I frame
            keyframe = 0x17;
        } else {
            keyframe = 0x27;
        }
        body_size = size + 9;
        RTMPPacket_Alloc(packet, body_size);
        packet->m_nBodySize = body_size;
        p = packet->m_body;
        *p++ = keyframe;
        *p++ = 0x01; // avc pack type
        *p++ = 0x00;
        *p++ = 0x00;
        *p++ = 0x00;
        val = htonl(size);
        memcpy(p, &val, 4);
        p += 4;
        memcpy(p, ptr, size);
        p += size;
        client->dataFifo->pushFifo(client->dataFifo, packet);
    }
    return ret;
}
void setDataFrame(RTMP_CLIENT *client, int duration, int width, int height, int fps, char* encoder, char* audioid, int audioVolume)
{
    _metaData.duration = duration;
    _metaData.width = width;
    _metaData.height = height;
    _metaData.fps = fps;
    memcpy(_metaData.encoder, encoder, sizeof(_metaData.encoder));
    memcpy(_metaData.audioid, audioid, sizeof(_metaData.audioid));
    _metaData.audioVolume = audioVolume;
}
/******************************************************************************
 * function: sendAudioData
 * description: 
 * param {RTMP_CLIENT} *client
 * param {char} *data
 * param {int} size
 * param {unsigned int} timestamp
 * param {int} absTimestamp
 * return {*}
********************************************************************************/
int sendAudioData(RTMP_CLIENT *client, const char *data, int size, unsigned int timestamp, int absTimestamp)
{
    int ret = 0;
    RTMP* rtmp = client->rtmp;
    if(client == NULL || rtmp == NULL || data == NULL || size <= 0) {
        return -1;
    }
    RTMPPacket *packet = (RTMPPacket *)malloc(sizeof(RTMPPacket) + RTMP_MAX_HEADER_SIZE);
    if(!packet) {
        return -1;
    }
    RTMP_Log(rtmp->logCtx, RTMP_LOGDEBUG, "%s, audio data, size:%d, timestamp:%d, abstimestamp:%d", __FUNCTION__, size, timestamp, absTimestamp);
    
    char *ptr = 0;
    int body_size = 0;

    RTMPPacket_Reset(packet);
    packet->m_packetType = RTMP_PACKET_TYPE_AUDIO;
    packet->m_nTimeStamp = timestamp;
    packet->m_hasAbsTimestamp = absTimestamp;// ? 0 : 1;
    packet->m_nChannel = 0x07;
    packet->m_headerType = absTimestamp ? RTMP_PACKET_SIZE_LARGE : RTMP_PACKET_SIZE_MEDIUM;
    packet->m_nInfoField2 = rtmp->m_stream_id;

    body_size = size;
    RTMPPacket_Alloc(packet, body_size);
    packet->m_nBodySize = body_size;
    ptr = packet->m_body;
    memcpy(ptr, data, size);
    client->dataFifo->pushFifo(client->dataFifo, packet);
    return ret;
}