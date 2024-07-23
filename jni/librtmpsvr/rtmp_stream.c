/******************************************************************************
 * Author: liguoqiang
 * Date: 2023-12-24 09:10:18
 * LastEditors: liguoqiang
 * LastEditTime: 2024-07-23 10:56:27
 * Description: 
********************************************************************************/
#include "librtmp/log.h"
#include "librtmp/rtmp_sys.h"
#include "rtmp_stream.h"

#define RD_SUCCESS 0
#define RD_FAILED 1
#define RD_INCOMPLETE 2

#define DUPTIME 5000 /* interval we disallow duplicate requests, in msec */
#define DEFAULT_BUF_SIZE (2*1024*1024)

#define STR2AVAL(av, str) \
  av.av_val = str;        \
  av.av_len = strlen(av.av_val)

#define SAVC(x) static const AVal av_##x = AVC(#x)  

SAVC(app);
SAVC(connect);
SAVC(flashVer);
SAVC(swfUrl);
SAVC(pageUrl);
SAVC(tcUrl);
SAVC(fpad);
SAVC(capabilities);
SAVC(audioCodecs);
SAVC(videoCodecs);
SAVC(videoFunction);
SAVC(objectEncoding);
SAVC(_result);
SAVC(createStream);
SAVC(releaseStream);
SAVC(deleteStream);
SAVC(closeStream);
SAVC(getStreamLength);
SAVC(play);
SAVC(live);
SAVC(fmsVer);
SAVC(mode);
SAVC(onStatus);
SAVC(status);
SAVC(level);
SAVC(code);
SAVC(description);
SAVC(secureToken);

/******************************************************************************
 * function: SendConnectResult
 * description: send connect result to client
 * param {RTMP} *r
 * param {double} txn
 * return {*}
********************************************************************************/
static int SendConnectResult(RTMP *r, double txn)
{
  RTMPPacket packet;
  char pbuf[384], *pend = pbuf + sizeof(pbuf);
  AMFObject obj;
  AMFObjectProperty p, op;
  AVal av;

  packet.m_nChannel = 0x03;   // control channel (invoke)
  packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;    /* RTMP_PACKET_SIZE_MEDIUM; */
  packet.m_packetType = 0x14; // INVOKE
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  char *enc = packet.m_body;
  enc = AMF_EncodeString(enc, pend, &av__result);
  enc = AMF_EncodeNumber(enc, pend, txn);
  *enc++ = AMF_OBJECT;

  STR2AVAL(av, "FMS/3,5,1,525");
  enc = AMF_EncodeNamedString(enc, pend, &av_fmsVer, &av);
  enc = AMF_EncodeNamedNumber(enc, pend, &av_capabilities, 31.0);
  enc = AMF_EncodeNamedNumber(enc, pend, &av_mode, 1.0);
  *enc++ = 0;
  *enc++ = 0;
  *enc++ = AMF_OBJECT_END;

  *enc++ = AMF_OBJECT;

  STR2AVAL(av, "status");
  enc = AMF_EncodeNamedString(enc, pend, &av_level, &av);
  STR2AVAL(av, "NetConnection.Connect.Success");
  enc = AMF_EncodeNamedString(enc, pend, &av_code, &av);
  STR2AVAL(av, "Connection succeeded.");
  enc = AMF_EncodeNamedString(enc, pend, &av_description, &av);
  enc = AMF_EncodeNamedNumber(enc, pend, &av_objectEncoding, r->m_fEncoding);

  // STR2AVAL(p.p_name, "version");
  // STR2AVAL(p.p_vu.p_aval, "3,5,1,525");
  // p.p_type = AMF_STRING;
  // obj.o_num = 1;
  // obj.o_props = &p;
  // op.p_type = AMF_OBJECT;
  // STR2AVAL(op.p_name, "data");
  // op.p_vu.p_object = obj;
  // enc = AMFProp_Encode(&op, enc, pend);
  // *enc++ = 0;
  // *enc++ = 0;
  // *enc++ = AMF_OBJECT_END;
  *enc++ = 0;
  *enc++ = 0;
  *enc++ = AMF_OBJECT_END;

  packet.m_nBodySize = enc - packet.m_body;

  return RTMP_SendPacket(r, &packet, FALSE);
}

/******************************************************************************
 * function: SendResultNumber
 * description: send result number to client
 * param {RTMP} *r
 * param {double} txn
 * param {double} ID
 * return {*}
********************************************************************************/
static int SendResultNumber(RTMP *r, double txn, double ID)
{
  RTMPPacket packet;
  char pbuf[256], *pend = pbuf + sizeof(pbuf);

  packet.m_nChannel = 0x03;   // control channel (invoke)
  packet.m_headerType = RTMP_PACKET_SIZE_MEDIUM;    /* RTMP_PACKET_SIZE_MEDIUM; */
  packet.m_packetType = 0x14; // INVOKE
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  char *enc = packet.m_body;
  enc = AMF_EncodeString(enc, pend, &av__result);
  enc = AMF_EncodeNumber(enc, pend, txn);
  *enc++ = AMF_NULL;
  enc = AMF_EncodeNumber(enc, pend, ID);

  packet.m_nBodySize = enc - packet.m_body;

  return RTMP_SendPacket(r, &packet, FALSE);
}


static const AVal av_NetStream_Play_Start = AVC("NetStream.Play.Start");
static const AVal av_Started_playing = AVC("Started playing");
static const AVal av_NetStream_Play_Stop = AVC("NetStream.Play.Stop");
static const AVal av_Stopped_playing = AVC("Stopped playing");
SAVC(details);
SAVC(clientid);
/******************************************************************************
 * function: SendPlayStart
 * description: 
 * param {RTMP} *r
 * return {*}
********************************************************************************/
static int SendPlayStart(RTMP *r)
{
  RTMPPacket packet;
  char pbuf[384], *pend = pbuf + sizeof(pbuf);

  packet.m_nChannel = 0x03;   // control channel (invoke)
  packet.m_headerType = 1;    /* RTMP_PACKET_SIZE_MEDIUM; */
  packet.m_packetType = 0x14; // INVOKE
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  char *enc = packet.m_body;
  enc = AMF_EncodeString(enc, pend, &av_onStatus);
  enc = AMF_EncodeNumber(enc, pend, 0);
  *enc++ = AMF_OBJECT;

  enc = AMF_EncodeNamedString(enc, pend, &av_level, &av_status);
  enc = AMF_EncodeNamedString(enc, pend, &av_code, &av_NetStream_Play_Start);
  enc = AMF_EncodeNamedString(enc, pend, &av_description, &av_Started_playing);
  enc = AMF_EncodeNamedString(enc, pend, &av_details, &r->Link.playpath);
  enc = AMF_EncodeNamedString(enc, pend, &av_clientid, &av_clientid);
  *enc++ = 0;
  *enc++ = 0;
  *enc++ = AMF_OBJECT_END;

  packet.m_nBodySize = enc - packet.m_body;
  return RTMP_SendPacket(r, &packet, FALSE);
}
/******************************************************************************
 * function: SendPlayStop
 * description: 
 * param {RTMP} *r
 * return {*}
********************************************************************************/
static int SendPlayStop(RTMP *r)
{
  RTMPPacket packet;
  char pbuf[384], *pend = pbuf + sizeof(pbuf);

  packet.m_nChannel = 0x03;   // control channel (invoke)
  packet.m_headerType = 1;    /* RTMP_PACKET_SIZE_MEDIUM; */
  packet.m_packetType = 0x14; // INVOKE
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  char *enc = packet.m_body;
  enc = AMF_EncodeString(enc, pend, &av_onStatus);
  enc = AMF_EncodeNumber(enc, pend, 0);
  *enc++ = AMF_OBJECT;

  enc = AMF_EncodeNamedString(enc, pend, &av_level, &av_status);
  enc = AMF_EncodeNamedString(enc, pend, &av_code, &av_NetStream_Play_Stop);
  enc = AMF_EncodeNamedString(enc, pend, &av_description, &av_Stopped_playing);
  enc = AMF_EncodeNamedString(enc, pend, &av_details, &r->Link.playpath);
  enc = AMF_EncodeNamedString(enc, pend, &av_clientid, &av_clientid);
  *enc++ = 0;
  *enc++ = 0;
  *enc++ = AMF_OBJECT_END;

  packet.m_nBodySize = enc - packet.m_body;
  return RTMP_SendPacket(r, &packet, FALSE);
}

static const AVal av_NetStream_Publish_Start = AVC("NetStream.Publish.Start");
static const AVal av_Publish_playing = AVC("Start Publishing");
SAVC(publish);
SAVC(FCPublish);

/******************************************************************************
 * function: streamBegin
 * description: send stream begin response to publisher
 * param {RTMP} *r
 * return {*}
********************************************************************************/
static int streamBegin(RTMP *r)
{
  RTMPPacket packet;
  char pbuf[RTMP_MAX_HEADER_SIZE + 32], *pend = pbuf + sizeof(pbuf);

  packet.m_nChannel = 0x02;   // control channel
  packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
  packet.m_packetType = 0x04; // USER_CONTROL
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = r->m_stream_id;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  char *enc = packet.m_body;
  enc = AMF_EncodeInt16(enc, pend, 0);
  enc = AMF_EncodeInt32(enc, pend, r->m_stream_id);

  packet.m_nBodySize = enc - packet.m_body;
  return RTMP_SendPacket(r, &packet, FALSE);
}
/******************************************************************************
 * function: SendPublishResult
 * description: send publisher result to client
 * param {RTMP} *r
 * return {*}
********************************************************************************/
static int SendPublishResult(RTMP *r)
{
  RTMPPacket packet;
  char pbuf[384], *pend = pbuf + sizeof(pbuf);

  packet.m_nChannel = 0x05;   // data flow channel
  packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
  packet.m_packetType = 0x14; // INVOKE
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = r->m_stream_id;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  char *enc = packet.m_body;
  enc = AMF_EncodeString(enc, pend, &av_onStatus);
  enc = AMF_EncodeNumber(enc, pend, 0);
  *enc++ = AMF_NULL;   // important
  *enc++ = AMF_OBJECT;
  enc = AMF_EncodeNamedString(enc, pend, &av_level, &av_status);
  enc = AMF_EncodeNamedString(enc, pend, &av_code, &av_NetStream_Publish_Start);
  enc = AMF_EncodeNamedString(enc, pend, &av_description, &av_Publish_playing);
  *enc++ = 0;
  *enc++ = 0;
  *enc++ = AMF_OBJECT_END;

  packet.m_nBodySize = enc - packet.m_body;
  return RTMP_SendPacket(r, &packet, FALSE);
}

/******************************************************************************
 * function: sendChunkSize
 * description: reponse chunk size to client
 * param {RTMP} *r
 * return {*}
********************************************************************************/
static int sendChunkSize(RTMP *r, int chunkSize) {
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
  // r->m_inChunkSize = chunkSize;
  r->m_outChunkSize = chunkSize;
  enc = AMF_EncodeInt32(enc, pend, chunkSize);//RTMP_DEFAULT_CHUNKSIZE);

  packet.m_nBodySize = enc - packet.m_body;
  return RTMP_SendPacket(r, &packet, FALSE);
}
/******************************************************************************
 * function: HandleUserCtrl
 * description: 
 * param {RTMP} *r
 * param {RTMPPacket} *packet
 * return {*}
********************************************************************************/
static void HandleUserCtrl(RTMP *r, const RTMPPacket *packet)
{
  short nType = -1;
  unsigned int tmp;
  if (packet->m_body && packet->m_nBodySize >= 2)
    nType = AMF_DecodeInt16(packet->m_body);
  RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, received ctrl. type: %d, len: %d", __FUNCTION__, nType,
           packet->m_nBodySize);
  /*RTMP_LogHex(r->logCtx, packet.m_body, packet.m_nBodySize); */

  if (packet->m_nBodySize >= 6)
  {
    switch (nType)
    {
    case 0:
      tmp = AMF_DecodeInt32(packet->m_body + 2);
      RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, Stream Begin %d", __FUNCTION__, tmp);
      break;

    case 1:
      tmp = AMF_DecodeInt32(packet->m_body + 2);
      RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, Stream EOF %d", __FUNCTION__, tmp);
      if (r->m_pausing == 1)
        r->m_pausing = 2;
      break;

    case 2:
      tmp = AMF_DecodeInt32(packet->m_body + 2);
      RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, Stream Dry %d", __FUNCTION__, tmp);
      break;

    case 4:
      tmp = AMF_DecodeInt32(packet->m_body + 2);
      RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, Stream IsRecorded %d", __FUNCTION__, tmp);
      break;

    case 6: /* server ping. reply with pong. */
      tmp = AMF_DecodeInt32(packet->m_body + 2);
      RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, Ping %d", __FUNCTION__, tmp);
      RTMP_SendCtrl(r, 0x07, tmp, 0);
      break;

    /* FMS 3.5 servers send the following two controls to let the client
     * know when the server has sent a complete buffer. I.e., when the
     * server has sent an amount of data equal to m_nBufferMS in duration.
     * The server meters its output so that data arrives at the client
     * in realtime and no faster.
     *
     * The rtmpdump program tries to set m_nBufferMS as large as
     * possible, to force the server to send data as fast as possible.
     * In practice, the server appears to cap this at about 1 hour's
     * worth of data. After the server has sent a complete buffer, and
     * sends this BufferEmpty message, it will wait until the play
     * duration of that buffer has passed before sending a new buffer.
     * The BufferReady message will be sent when the new buffer starts.
     * (There is no BufferReady message for the very first buffer;
     * presumably the Stream Begin message is sufficient for that
     * purpose.)
     *
     * If the network speed is much faster than the data bitrate, then
     * there may be long delays between the end of one buffer and the
     * start of the next.
     *
     * Since usually the network allows data to be sent at
     * faster than realtime, and rtmpdump wants to download the data
     * as fast as possible, we use this RTMP_LF_BUFX hack: when we
     * get the BufferEmpty message, we send a Pause followed by an
     * Unpause. This causes the server to send the next buffer immediately
     * instead of waiting for the full duration to elapse. (That's
     * also the purpose of the ToggleStream function, which rtmpdump
     * calls if we get a read timeout.)
     *
     * Media player apps don't need this hack since they are just
     * going to play the data in realtime anyway. It also doesn't work
     * for live streams since they obviously can only be sent in
     * realtime. And it's all moot if the network speed is actually
     * slower than the media bitrate.
     */
    case 31:
      tmp = AMF_DecodeInt32(packet->m_body + 2);
      RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, Stream BufferEmpty %d", __FUNCTION__, tmp);
      if (!(r->Link.lFlags & RTMP_LF_BUFX))
        break;
      if (!r->m_pausing)
      {
        r->m_pauseStamp = r->m_channelTimestamp[r->m_mediaChannel];
        RTMP_SendPause(r, TRUE, r->m_pauseStamp);
        r->m_pausing = 1;
      }
      else if (r->m_pausing == 2)
      {
        RTMP_SendPause(r, FALSE, r->m_pauseStamp);
        r->m_pausing = 3;
      }
      break;

    case 32:
      tmp = AMF_DecodeInt32(packet->m_body + 2);
      RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, Stream BufferReady %d", __FUNCTION__, tmp);
      break;

    default:
      tmp = AMF_DecodeInt32(packet->m_body + 2);
      RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, Stream xx %d", __FUNCTION__, tmp);
      break;
    }
  }

  if (nType == 0x1A)
  {
    RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, SWFVerification ping received: ", __FUNCTION__);
#ifdef CRYPTO
    /*RTMP_LogHex(r->logCtx, packet.m_body, packet.m_nBodySize); */

    /* respond with HMAC SHA256 of decompressed SWF, key is the 30byte player key, also the last 30 bytes of the server handshake are applied */
    if (r->Link.SWFSize)
    {
      RTMP_SendCtrl(r, 0x1B, 0, 0);
    }
    else
    {
      RTMP_Log(r->logCtx, RTMP_LOGERROR,
               "%s: Ignoring SWFVerification request, use --swfVfy!",
               __FUNCTION__);
    }
#else
    RTMP_Log(r->logCtx, RTMP_LOGERROR,
             "%s: Ignoring SWFVerification request, no CRYPTO support!",
             __FUNCTION__);
#endif
  }
}

/******************************************************************************
 * function: countAMF
 * description: 
 * param {AMFObject} *obj
 * param {int} *argc
 * return {*}
********************************************************************************/
static int countAMF(AMFObject *obj, int *argc)
{
  int i, len;

  for (i = 0, len = 0; i < obj->o_num; i++)
  {
    AMFObjectProperty *p = &obj->o_props[i];
    len += 4;
    (*argc) += 2;
    if (p->p_name.av_val)
      len += 1;
    len += 2;
    if (p->p_name.av_val)
      len += p->p_name.av_len + 1;
    switch (p->p_type)
    {
    case AMF_BOOLEAN:
      len += 1;
      break;
    case AMF_STRING:
      len += p->p_vu.p_aval.av_len;
      break;
    case AMF_NUMBER:
      len += 40;
      break;
    case AMF_OBJECT:
      len += 9;
      len += countAMF(&p->p_vu.p_object, argc);
      (*argc) += 2;
      break;
    case AMF_NULL:
    default:
      break;
    }
  }
  return len;
}

static char * dumpAMF(AMFObject *obj, char *ptr, AVal *argv, int *argc)
{
  int i, len, ac = *argc;
  const char opt[] = "NBSO Z";

  for (i = 0, len = 0; i < obj->o_num; i++)
  {
    AMFObjectProperty *p = &obj->o_props[i];
    argv[ac].av_val = ptr + 1;
    argv[ac++].av_len = 2;
    ptr += sprintf(ptr, " -C ");
    argv[ac].av_val = ptr;
    if (p->p_name.av_val)
      *ptr++ = 'N';
    *ptr++ = opt[p->p_type];
    *ptr++ = ':';
    if (p->p_name.av_val)
      ptr += sprintf(ptr, "%.*s:", p->p_name.av_len, p->p_name.av_val);
    switch (p->p_type)
    {
    case AMF_BOOLEAN:
      *ptr++ = p->p_vu.p_number != 0 ? '1' : '0';
      argv[ac].av_len = ptr - argv[ac].av_val;
      break;
    case AMF_STRING:
      memcpy(ptr, p->p_vu.p_aval.av_val, p->p_vu.p_aval.av_len);
      ptr += p->p_vu.p_aval.av_len;
      argv[ac].av_len = ptr - argv[ac].av_val;
      break;
    case AMF_NUMBER:
      ptr += sprintf(ptr, "%f", p->p_vu.p_number);
      argv[ac].av_len = ptr - argv[ac].av_val;
      break;
    case AMF_OBJECT:
      *ptr++ = '1';
      argv[ac].av_len = ptr - argv[ac].av_val;
      ac++;
      *argc = ac;
      ptr = dumpAMF(&p->p_vu.p_object, ptr, argv, argc);
      ac = *argc;
      argv[ac].av_val = ptr + 1;
      argv[ac++].av_len = 2;
      argv[ac].av_val = ptr + 4;
      argv[ac].av_len = 3;
      ptr += sprintf(ptr, " -C O:0");
      break;
    case AMF_NULL:
    default:
      argv[ac].av_len = ptr - argv[ac].av_val;
      break;
    }
    ac++;
  }
  *argc = ac;
  return ptr;
}
static void handleCloseStream(RTMP *r, RTMPPacket *packet)
{
  unsigned int nStreamID;
  char *body = packet->m_body;
  nStreamID = AMF_DecodeInt32(body);
  RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, close stream %d", __FUNCTION__, nStreamID);
  if (r->m_stream_id == nStreamID)
  {
    RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, close current stream %d", __FUNCTION__, nStreamID);
    r->m_stream_id = 0;
  }
  RTMP_Close(r);
}
/******************************************************************************
 * function: ServeInvoke
 * description: handle invoke packet
 *  Returns 0 for OK/Failed/error, 1 for 'Stop or Complete'
 * return {*}
********************************************************************************/
int ServeInvoke(RTMP_STREAM *stream, RTMP *r, RTMPPacket *packet, unsigned int offset)
{
  const char *body;
  unsigned int nBodySize;
  int ret = 0, nRes;

  body = packet->m_body + offset;
  nBodySize = packet->m_nBodySize - offset;

  if (body[0] != 0x02) // make sure it is a string method name we start with
  {
    RTMP_Log(r->logCtx, RTMP_LOGWARNING, "%s, Sanity failed. no string method in invoke packet",
             __FUNCTION__);
    return 0;
  }

  AMFObject obj;
  nRes = AMF_Decode(r->logCtx, &obj, body, nBodySize, FALSE);
  if (nRes < 0)
  {
    RTMP_Log(r->logCtx, RTMP_LOGERROR, "%s, error decoding invoke packet", __FUNCTION__);
    return 0;
  }

  AMF_Dump(r->logCtx, &obj);
  AVal method;
  AMFProp_GetString(AMF_GetProp(&obj, NULL, 0), &method);
  double txn = AMFProp_GetNumber(AMF_GetProp(&obj, NULL, 1));
  RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, client invoking <%s>", __FUNCTION__, method.av_val);

  if (AVMATCH(&method, &av_connect))
  {
    AMFObject cobj;
    AVal pname, pval;
    int i;

    stream->connect = packet->m_body;
    packet->m_body = NULL;

    AMFProp_GetObject(AMF_GetProp(&obj, NULL, 2), &cobj);
    for (i = 0; i < cobj.o_num; i++)
    {
      pname = cobj.o_props[i].p_name;
      pval.av_val = NULL;
      pval.av_len = 0;
      if (cobj.o_props[i].p_type == AMF_STRING)
        pval = cobj.o_props[i].p_vu.p_aval;
      if (AVMATCH(&pname, &av_app))
      {
        r->Link.app = pval;
        pval.av_val = NULL;
        if (!r->Link.app.av_val)
          r->Link.app.av_val = "";
        stream->arglen += 6 + pval.av_len;
        stream->argc += 2;
      }
      else if (AVMATCH(&pname, &av_flashVer))
      {
        r->Link.flashVer = pval;
        pval.av_val = NULL;
        stream->arglen += 6 + pval.av_len;
        stream->argc += 2;
      }
      else if (AVMATCH(&pname, &av_swfUrl))
      {
        r->Link.swfUrl = pval;
        pval.av_val = NULL;
        stream->arglen += 6 + pval.av_len;
        stream->argc += 2;
      }
      else if (AVMATCH(&pname, &av_tcUrl))
      {
        r->Link.tcUrl = pval;
        pval.av_val = NULL;
        stream->arglen += 6 + pval.av_len;
        stream->argc += 2;
      }
      else if (AVMATCH(&pname, &av_pageUrl))
      {
        r->Link.pageUrl = pval;
        pval.av_val = NULL;
        stream->arglen += 6 + pval.av_len;
        stream->argc += 2;
      }
      else if (AVMATCH(&pname, &av_audioCodecs))
      {
        r->m_fAudioCodecs = cobj.o_props[i].p_vu.p_number;
      }
      else if (AVMATCH(&pname, &av_videoCodecs))
      {
        r->m_fVideoCodecs = cobj.o_props[i].p_vu.p_number;
      }
      else if (AVMATCH(&pname, &av_objectEncoding))
      {
        r->m_fEncoding = cobj.o_props[i].p_vu.p_number;
      }
    }
    /* Still have more parameters? Copy them */
    if (obj.o_num > 3)
    {
      int i = obj.o_num - 3;
      r->Link.extras.o_num = i;
      r->Link.extras.o_props = (AMFObjectProperty*)malloc(i * sizeof(AMFObjectProperty));
      memcpy(r->Link.extras.o_props, obj.o_props + 3, i * sizeof(AMFObjectProperty));
      obj.o_num = 3;
      stream->arglen += countAMF(&r->Link.extras, &stream->argc);
    }
    sendChunkSize(r, stream->chunkSize);
    RTMP_SendServerBW(r);
    RTMP_SendClientBW(r);
    SendConnectResult(r, txn);
  }
  else if (AVMATCH(&method, &av_createStream))
  {
    SendResultNumber(r, txn, ++r->m_stream_id);
  }
  else if(AVMATCH(&method, &av_closeStream)) {
    handleCloseStream(r, packet);
  }
  else if (AVMATCH(&method, &av_getStreamLength))
  {
    SendResultNumber(r, txn, 10.0);
  }
  else if(AVMATCH(&method, &av_publish))
  {
    if (streamBegin(r) == 0)
    {
      RTMP_Log(r->logCtx, RTMP_LOGERROR, "%s, error sending stream begin", __FUNCTION__);
      return 0;
    }
    if(stream && stream->beginPublishFunc) {
      stream->beginPublishFunc(stream->user_data);
    }
    SendPublishResult(r);
  }
  else if (AVMATCH(&method, &av_live))
  {
    r->Link.lFlags |= RTMP_LF_LIVE;
    SendResultNumber(r, txn, 1.0);
  }
  else if (AVMATCH(&method, &av_play))
  {
    char *file, *p, *q, *cmd, *ptr;
    AVal *argv, av;
    int len, argc;
    uint32_t now;
    RTMPPacket pc = {0};
    AMFProp_GetString(AMF_GetProp(&obj, NULL, 3), &r->Link.playpath);
    /*
    r->Link.seekTime = AMFProp_GetNumber(AMF_GetProp(&obj, NULL, 4));
    if (obj.o_num > 5)
r->Link.length = AMFProp_GetNumber(AMF_GetProp(&obj, NULL, 5));
    */
    if (r->Link.tcUrl.av_len)
    {
      len = stream->arglen + r->Link.playpath.av_len + 4 +
            sizeof("rtmpdump") + r->Link.playpath.av_len + 12;
      stream->argc += 5;

      cmd = (char*)malloc(len + stream->argc * sizeof(AVal));
      ptr = cmd;
      argv = (AVal *)(cmd + len);
      argv[0].av_val = cmd;
      argv[0].av_len = sizeof("rtmpdump") - 1;
      ptr += sprintf(ptr, "rtmpdump");
      argc = 1;

      argv[argc].av_val = ptr + 1;
      argv[argc++].av_len = 2;
      argv[argc].av_val = ptr + 5;
      ptr += sprintf(ptr, " -r \"%s\"", r->Link.tcUrl.av_val);
      argv[argc++].av_len = r->Link.tcUrl.av_len;

      if (r->Link.app.av_val)
      {
        argv[argc].av_val = ptr + 1;
        argv[argc++].av_len = 2;
        argv[argc].av_val = ptr + 5;
        ptr += sprintf(ptr, " -a \"%s\"", r->Link.app.av_val);
        argv[argc++].av_len = r->Link.app.av_len;
      }
      if (r->Link.flashVer.av_val)
      {
        argv[argc].av_val = ptr + 1;
        argv[argc++].av_len = 2;
        argv[argc].av_val = ptr + 5;
        ptr += sprintf(ptr, " -f \"%s\"", r->Link.flashVer.av_val);
        argv[argc++].av_len = r->Link.flashVer.av_len;
      }
      if (r->Link.swfUrl.av_val)
      {
        argv[argc].av_val = ptr + 1;
        argv[argc++].av_len = 2;
        argv[argc].av_val = ptr + 5;
        ptr += sprintf(ptr, " -W \"%s\"", r->Link.swfUrl.av_val);
        argv[argc++].av_len = r->Link.swfUrl.av_len;
      }
      if (r->Link.pageUrl.av_val)
      {
        argv[argc].av_val = ptr + 1;
        argv[argc++].av_len = 2;
        argv[argc].av_val = ptr + 5;
        ptr += sprintf(ptr, " -p \"%s\"", r->Link.pageUrl.av_val);
        argv[argc++].av_len = r->Link.pageUrl.av_len;
      }
      if (r->Link.extras.o_num)
      {
        ptr = dumpAMF(&r->Link.extras, ptr, argv, &argc);
        AMF_Reset(&r->Link.extras);
      }
      argv[argc].av_val = ptr + 1;
      argv[argc++].av_len = 2;
      argv[argc].av_val = ptr + 5;
      ptr += sprintf(ptr, " -y \"%.*s\"",
                     r->Link.playpath.av_len, r->Link.playpath.av_val);
      argv[argc++].av_len = r->Link.playpath.av_len;

      av = r->Link.playpath;
      /* strip trailing URL parameters */
      q = (char*)memchr(av.av_val, '?', av.av_len);
      if (q)
      {
        if (q == av.av_val)
        {
          av.av_val++;
          av.av_len--;
        }
        else
        {
          av.av_len = q - av.av_val;
        }
      }
      /* strip leading slash components */
      for (p = av.av_val + av.av_len - 1; p >= av.av_val; p--)
        if (*p == '/')
        {
          p++;
          av.av_len -= p - av.av_val;
          av.av_val = p;
          break;
        }
      /* skip leading dot */
      if (av.av_val[0] == '.')
      {
        av.av_val++;
        av.av_len--;
      }
      file = (char*)malloc(av.av_len + 5);

      memcpy(file, av.av_val, av.av_len);
      file[av.av_len] = '\0';
      for (p = file; *p; p++)
        if (*p == ':')
          *p = '_';

      /* Add extension if none present */
      if (file[av.av_len - 4] != '.')
      {
        av.av_len += 4;
      }
      /* Always use flv extension, regardless of original */
      if (strcmp(file + av.av_len - 4, ".flv"))
      {
        strcpy(file + av.av_len - 4, ".flv");
      }
      argv[argc].av_val = ptr + 1;
      argv[argc++].av_len = 2;
      argv[argc].av_val = file;
      argv[argc].av_len = av.av_len;
      ptr += sprintf(ptr, " -o %s", file);
      now = RTMP_GetTime();
      if (now - stream->filetime < DUPTIME && AVMATCH(&argv[argc], &stream->filename))
      {
        printf("Duplicate request, skipping.\n");
        free(file);
      }
      else
      {
        printf("\n%s\n\n", cmd);
        fflush(stdout);
        stream->filetime = now;
        free(stream->filename.av_val);
        stream->filename = argv[argc++];
      }

      free(cmd);
    }
    pc.m_body = stream->connect;
    stream->connect = NULL;
    RTMPPacket_Free(&pc);
    ret = 1;
    RTMP_SendCtrl(r, 0, 1, 0);
    SendPlayStart(r);
    RTMP_SendCtrl(r, 1, 1, 0);
    SendPlayStop(r);
  }
  RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, before AMF_Reset", __FUNCTION__);
  AMF_Reset(&obj);
  RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, after AMF_Reset", __FUNCTION__);
  return ret;
}
/******************************************************************************
 * function: handleDataFrame
 * description: 
 * param {RTMP_STREAM} *stream
 * param {RTMP} *r
 * param {RTMPPacket} *packet
 * return {*}
********************************************************************************/
SAVC(onMetaData);
SAVC(duration);
SAVC(width);
SAVC(height);
SAVC(framerate);
SAVC(encoder);
SAVC(audiocodecid);
SAVC(audioinputvolume);
static void handleDataFrame(RTMP_STREAM *stream, RTMP *r, RTMPPacket *packet)
{
  int ret = 0;
  RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, received: data frame %lu bytes", __FUNCTION__, packet->m_nBodySize);
  AMFObject obj;
  if (packet->m_body[0] != 0x02) {
    RTMP_Log(r->logCtx, RTMP_LOGWARNING, "%s, Sanity failed. no string method in invoke packet", __FUNCTION__);
    return;
  }
  if (stream->dataFrameBuf) {
    free(stream->dataFrameBuf);
  }
  stream->dataFrameBuf = (char*)malloc(packet->m_nBodySize + RTMP_MAX_HEADER_SIZE);
  memcpy(stream->dataFrameBuf, packet->m_body, packet->m_nBodySize);
  stream->dataFrameSize = packet->m_nBodySize;
  ret = AMF_Decode(r->logCtx, &obj, packet->m_body, packet->m_nBodySize, FALSE);
  if (ret < 0) {
    RTMP_Log(r->logCtx, RTMP_LOGERROR, "%s, error decoding data frame", __FUNCTION__);
    return;
  }
  AVal method;
  AMFProp_GetString(AMF_GetProp(&obj, NULL, 1), &method);
  if (AVMATCH(&method, &av_onMetaData)) {
    RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, received: onMetaData", __FUNCTION__);
    AMFObjectProperty prop;
    if (RTMP_FindFirstMatchingProperty(&obj, &av_duration, &prop)) {
      stream->duration = prop.p_vu.p_number;
      RTMP_Log(r->logCtx, RTMP_LOGINFO, "%s, received, duration:%d", __FUNCTION__, stream->duration);
    }
    if (RTMP_FindFirstMatchingProperty(&obj, &av_width, &prop)) {
      stream->width = prop.p_vu.p_number;
      RTMP_Log(r->logCtx, RTMP_LOGINFO, "%s, received, width:%d", __FUNCTION__, stream->width);
    }
    if (RTMP_FindFirstMatchingProperty(&obj, &av_height, &prop)) {
      stream->height = prop.p_vu.p_number;
      RTMP_Log(r->logCtx, RTMP_LOGINFO, "%s, received, height:%d", __FUNCTION__, stream->height);
    }
    if (RTMP_FindFirstMatchingProperty(&obj, &av_framerate, &prop)) {
      stream->framerate = prop.p_vu.p_number;
      RTMP_Log(r->logCtx, RTMP_LOGINFO, "%s, received, framerate:%d", __FUNCTION__, stream->framerate);
    }
    if (RTMP_FindFirstMatchingProperty(&obj, &av_encoder, &prop)) {
      memset(stream->encoder, 0, sizeof(stream->encoder));
      memcpy(stream->encoder, prop.p_vu.p_aval.av_val, prop.p_vu.p_aval.av_len);
      RTMP_Log(r->logCtx, RTMP_LOGINFO, "%s, received, encoder:%s", __FUNCTION__, stream->encoder);
    }
    if (RTMP_FindFirstMatchingProperty(&obj, &av_audiocodecid, &prop)) {
      memcpy(stream->audioid, prop.p_vu.p_aval.av_val, prop.p_vu.p_aval.av_len);
      RTMP_Log(r->logCtx, RTMP_LOGINFO, "%s, received, andioid:%s", __FUNCTION__, stream->audioid);
    }
    if (RTMP_FindFirstMatchingProperty(&obj, &av_audioinputvolume, &prop)) {
      stream->audioVolume = prop.p_vu.p_number;
      RTMP_Log(r->logCtx, RTMP_LOGINFO, "%s, received, andiovoume:%d", __FUNCTION__, stream->audioVolume);
    }
  }
}
/******************************************************************************
 * function: 
 * description: 
 * param {RTMP_STREAM} *stream
 * param {RTMP} *r
 * param {RTMPPacket} *packet
 * return {*}
********************************************************************************/
void handleVideoPacket(RTMP_STREAM *stream, RTMP *r, RTMPPacket *packet)
{
  RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, received: video %lu bytes", __FUNCTION__, packet->m_nBodySize);
  if(stream->needRawVideo) {
    if(stream->videoFunc) {
      stream->videoFunc(packet->m_body, packet->m_nBodySize, packet->m_nTimeStamp, packet->m_hasAbsTimestamp, 0, stream->user_data);
    }
    return;
  }
  if(stream->videoBuffer == NULL) {
    stream->videoBufferSize = DEFAULT_BUF_SIZE;
    stream->videoBuffer = (char*)malloc(DEFAULT_BUF_SIZE);
  }
  char *buf = stream->videoBuffer;
  int bufLen = 0;
  int useLen = 0;
  unsigned int videoLen = 0;
  char* data = packet->m_body;
  int len = packet->m_nBodySize;
  int type = *data & 0x0f;
  int key = (*data >> 4) & 0x0f; // if key frame
  data++;
  int avcType = *data++;
  if (type == 7 && key == 1 && avcType == 0) { // avc sequence header
    if (len < 18) {
      return;
    }
    data += 4; // skip composition 3 byte time and 1 byte version
    data += 3; // skip 3 byte size
    data += 2; // skip 2 byte type
    short spsLen = 0;
    memcpy(&spsLen, data, 2);
    data += 2;
    spsLen = ntohs(spsLen);
    char *sps = 0;
    if(spsLen > 0) {
      memcpy(buf, "\x00\x00\x00\x01", 4);
      buf += 4;
      sps = data;
      memcpy(buf, data, spsLen);
      buf += spsLen;
    }
    data += spsLen;
    data ++;
    short ppsLen = 0;
    memcpy(&ppsLen, data, 2);
    data += 2;
    ppsLen = ntohs(ppsLen);
    char *pps = 0;
    if(ppsLen > 0) {
      memcpy(buf, "\x00\x00\x00\x01", 4);
      buf += 4;
      pps = data;
      memcpy(buf, data, ppsLen);
      buf += ppsLen;
    }
    data += ppsLen;
    bufLen = buf - stream->videoBuffer;
    RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, first video data this is sps and pps data", __FUNCTION__);
    if(stream->spsPpsFunc) {
      stream->spsPpsFunc(sps, spsLen, pps, ppsLen, stream->user_data);
    }
    if(stream->videoFunc) {
      stream->videoFunc(stream->videoBuffer, bufLen, packet->m_nTimeStamp, packet->m_hasAbsTimestamp, key, stream->user_data);
    }
  }
  
  if(type == 7 && (key <= 2) && avcType == 1) { // h264 AVC
    if(len < 9) {
      return;
    }
    data += 3;
    len -= 3;
    videoLen = 0;
    while(len > 4) {
      buf = stream->videoBuffer;
      memcpy(&videoLen, data, 4);
      data += 4;
      len -= 4;
      videoLen = ntohl(videoLen);
      if(videoLen > 1024*1024*20) {
        RTMP_Log(r->logCtx, RTMP_LOGERROR, "%s, videoLen %d is too large", __FUNCTION__, videoLen);
        return;
      }
      if (videoLen > 0) {
        if (videoLen >= stream->videoBufferSize) {
          stream->videoBufferSize = videoLen + 64;
          stream->videoBuffer = (char*)realloc((void*)stream->videoBuffer, stream->videoBufferSize);
          if (stream->videoBuffer == NULL) {
            RTMP_Log(r->logCtx, RTMP_LOGERROR, "%s, realloc video buffer failed", __FUNCTION__);
            return;
          }
          buf = stream->videoBuffer;
        }
        memcpy(buf, "\x00\x00\x00\x01", 4);
        buf += 4;
        memcpy(buf, data, videoLen);
        data += videoLen;
        len -= videoLen;
        buf += videoLen;
        bufLen = buf - stream->videoBuffer;
        if(stream->videoFunc) {
          stream->videoFunc(stream->videoBuffer, bufLen, packet->m_nTimeStamp, packet->m_hasAbsTimestamp, key, stream->user_data);
        }
      } else {
        break;
      }
      useLen = data - packet->m_body;
      if(useLen >= packet->m_nBodySize) {
        break;
      }
    }
  }
}
static void HandleChangeChunkSize(RTMP *r, const RTMPPacket *packet)
{
  if (packet->m_nBodySize >= 4)
  {
    r->m_inChunkSize = AMF_DecodeInt32(packet->m_body);
    RTMP_Log(r->logCtx, RTMP_LOGINFO, "%s, rtmp stream received: chunk size change to %d", __FUNCTION__,
             r->m_inChunkSize);
  }
}
static int sendServerBW(RTMP *r)
{
  RTMPPacket packet;
  char pbuf[256], *pend = pbuf + sizeof(pbuf);

  packet.m_nChannel = 0x02; /* control channel (invoke) */
  packet.m_headerType = RTMP_PACKET_SIZE_LARGE;
  packet.m_packetType = 0x06; /* Server BW */
  packet.m_nTimeStamp = 0;
  packet.m_nInfoField2 = 0;
  packet.m_hasAbsTimestamp = 0;
  packet.m_body = pbuf + RTMP_MAX_HEADER_SIZE;

  packet.m_nBodySize = 4;

  AMF_EncodeInt32(packet.m_body, pend, r->m_nServerBW);
  return RTMP_SendPacket(r, &packet, FALSE);
}
static void
HandleClientBW(RTMP *r, const RTMPPacket *packet)
{
  r->m_nClientBW = AMF_DecodeInt32(packet->m_body);
  if (packet->m_nBodySize > 4)
    r->m_nClientBW2 = packet->m_body[4];
  else
    r->m_nClientBW2 = -1;
  RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s: client BW = %d %d", __FUNCTION__, r->m_nClientBW,
           r->m_nClientBW2);
}

int ServePacket(RTMP_STREAM *stream, RTMP *r, RTMPPacket *packet)
{
  int ret = 0;

  RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, received packet type %02X, size %lu bytes", __FUNCTION__,
           packet->m_packetType, packet->m_nBodySize);

  switch (packet->m_packetType)
  {
  case 0x01:
    // chunk size
    HandleChangeChunkSize(r, packet);
    break;

  case 0x03:
    // bytes read report
    break;

  case 0x04:
    // ctrl
    HandleUserCtrl(r, packet);
    break;

  case 0x05:
    RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, received: type=0x05 %lu bytes", __FUNCTION__, packet->m_nBodySize);
    // client bw
    // HandleClientBW(r, packet);
    // sendServerBW(r);
    break;

  case 0x06:
    RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, received: type=0x06 %lu bytes", __FUNCTION__, packet->m_nBodySize);
    // server bw
    //      HandleServerBW(r, packet);
    break;

  case 0x08:
    // audio data
    RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, received: audio %lu bytes", __FUNCTION__, packet->m_nBodySize);
    if(stream->audioFunc) {
      stream->audioFunc(packet->m_body, packet->m_nBodySize, packet->m_nTimeStamp, packet->m_hasAbsTimestamp, stream->user_data);
    }
    break;

  case 0x09:
    // video data
    handleVideoPacket(stream, r, packet);
    break;

  case 0x0F: // flex stream send, AMF3 metadata
    RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, AMF3 metadata received, %lu bytes", __FUNCTION__, packet->m_nBodySize);
    break;

  case 0x10: // flex shared object message, AMF3 shared object
    break;

  case 0x11: // flex message
  {
    RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, flex message, size %lu bytes, not fully supported",
             __FUNCTION__, packet->m_nBodySize);
    // RTMP_LogHex(r->logCtx, packet.m_body, packet.m_nBodySize);

    // some DEBUG code
    /*RTMP_LIB_AMFObject obj;
      int nRes = obj.Decode(packet.m_body+1, packet.m_nBodySize-1);
      if(nRes < 0) {
      RTMP_Log(r->logCtx, RTMP_LOGERROR, "%s, error decoding AMF3 packet", __FUNCTION__);
      //return;
      }

      obj.Dump(); */

    if (ServeInvoke(stream, r, packet, 1))
      RTMP_Close(r);
    break;
  }
  case 0x12: // AMF0 metadata 
    // metadata (notify)
    RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, AMF0 metadata received, %lu bytes", __FUNCTION__, packet->m_nBodySize);
    handleDataFrame(stream, r, packet);
    break;

  case 0x13: // AMF0 shared object
    /* shared object */
    break;

  case 0x14:
    // invoke
    RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, received: invoke %lu bytes", __FUNCTION__, packet->m_nBodySize);
    // RTMP_LogHex(r->logCtx, packet.m_body, packet.m_nBodySize);
    if (ServeInvoke(stream, r, packet, 0))
      RTMP_Close(r);
    break;
  case 0x16:
    /* flv */
    RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, flv %lu bytes", __FUNCTION__, packet->m_nBodySize);
    break;
  default:
    RTMP_Log(r->logCtx, RTMP_LOGDEBUG, "%s, unknown packet type received: 0x%02x", __FUNCTION__,
             packet->m_packetType);
#ifdef _DEBUG
    RTMP_LogHex(r->logCtx, RTMP_LOGDEBUG, packet->m_body, packet->m_nBodySize);
#endif
  }
  return ret;
}
/******************************************************************************
 * function: readPacketThread
 * description: read packet from socket and handle it
 * param {RTMP_STREAM *} rtmpStream
 * return {*}
********************************************************************************/
static TFTYPE readPacketThread(void* v)
{
  RTMP_STREAM * rtmpStream = (RTMP_STREAM*)v;
  RTMPPacket packet = {0};
  RTMP *rtmp = rtmpStream->rtmp;
  while (rtmpStream->state == STREAMING_IN_PROGRESS
    && RTMP_IsConnected(rtmp)
    && (RTMP_ReadPacket(rtmp, &packet)) )
  {
    if (!RTMPPacket_IsReady(&packet))
      continue;
    ServePacket(rtmpStream, rtmp, &packet);
    RTMP_Log(rtmp->logCtx, RTMP_LOGDEBUG, "%s, RTMPPacket_Free ", __FUNCTION__);
    RTMPPacket_Free(&packet);
    sleep_m(1);
  }
  rtmpStream->state = STREAMING_STOPPING;
  RTMP_Close(rtmp);
  if(rtmpStream->exitPublishThreadFunc) {
    rtmpStream->exitPublishThreadFunc(rtmpStream->user_data);
  }
  RTMP_Log(rtmp->logCtx, RTMP_LOGDEBUG, "%s, readPacketThread exit, state=STREAMING_STOPPING RTMP_Close ", __FUNCTION__);
  return TFRET();
}
/******************************************************************************
 * function: initRtmpStream
 * description: init rtmp stream
 * param {int} chunkSize
 * return {*}
********************************************************************************/
RTMP_STREAM* initRtmpStream(int chunkSize, int needRawVideo)
{
  RTMP_STREAM * rtmpStream = (RTMP_STREAM*)malloc(sizeof(RTMP_STREAM));
  memset(rtmpStream, 0, sizeof(RTMP_STREAM));
  rtmpStream->rtmp = (RTMP*)malloc(sizeof(RTMP));
  RTMP_Init(rtmpStream->rtmp);
  rtmpStream->state = STREAMING_STOPPED;
  rtmpStream->mutex = createMutex();
  rtmpStream->needRawVideo = needRawVideo;
  rtmpStream->chunkSize = chunkSize;
  rtmpStream->videoBufferSize = DEFAULT_BUF_SIZE;
  rtmpStream->videoBuffer = (char*)malloc(rtmpStream->videoBufferSize);
  return rtmpStream;
}
/******************************************************************************
 * function: streamIsConnect
 * description: connect to rtmp server
 * param {RTMP_STREAM} *rtmpStream
 * param {char *} url
 * return {*}
********************************************************************************/
int streamIsConnect(RTMP_STREAM *rtmpStream)
{
  return rtmpStream->state == STREAMING_IN_PROGRESS && RTMP_IsConnected(rtmpStream->rtmp);
}
/******************************************************************************
 * function: 
 * description: 
 * param {int} sockfd
 * return {*}
********************************************************************************/
int openRtmpStreaming(RTMP_STREAM* rtmpStream, int sockfd )
{
  // timeout for http requests
  fd_set fds;
  struct timeval tv;
  unsigned long snd_size = 0;
  unsigned long rcv_size = 0;
  socklen_t optlen = sizeof(snd_size);
  memset(&tv, 0, sizeof(struct timeval));
  tv.tv_sec = 5;

  RTMP* rtmp = rtmpStream->rtmp;
  FD_ZERO(&fds);
  FD_SET(sockfd, &fds);

  if (select(sockfd + 1, &fds, NULL, NULL, &tv) <= 0)
  {
    RTMP_Log(rtmp->logCtx, RTMP_LOGERROR, "Request timeout/select failed, ignoring request");
    goto quit;
  }
  else
  {
    optlen = sizeof(snd_size);
    if (getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (char*)&snd_size, &optlen) == 0) {
      snd_size *= 2;
      setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &snd_size, sizeof(snd_size));
      RTMP_Log(rtmp->logCtx, RTMP_LOGDEBUG, "%s: send socket sndbuf size: %d\n", __FUNCTION__, snd_size);
    }
    optlen = sizeof(rcv_size);
    if (getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char*)&rcv_size, &optlen) == 0) {
      rcv_size *= 2;
      setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &rcv_size, sizeof(rcv_size));
      RTMP_Log(rtmp->logCtx, RTMP_LOGDEBUG, "%s: send socket rcvbuf size: %d\n", __FUNCTION__, rcv_size);
    }
    /* set timeout */
    {
      rtmpStream->rtmp->Link.timeout = 30;
      RTMP_Log(rtmp->logCtx, RTMP_LOGDEBUG, "%s: Setting socket timeout to %ds", __FUNCTION__, rtmpStream->rtmp->Link.timeout);
      SET_RCVTIMEO(tv, rtmpStream->rtmp->Link.timeout);
      if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv)))
      {
        RTMP_Log(rtmp->logCtx, RTMP_LOGERROR, "%s, Setting socket timeout to %d s failed!", __FUNCTION__, rtmpStream->rtmp->Link.timeout);
      }
    }
    int on = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&on, sizeof(on));
    rtmpStream->rtmp->m_sb.sb_socket = sockfd;
    rtmpStream->state = STREAMING_IN_PROGRESS;
    rtmpStream->rtmp->m_bSendCounter = TRUE;
    
    if (!RTMP_Serve(rtmpStream->rtmp))
    {
      RTMP_Log(rtmp->logCtx, RTMP_LOGERROR, "Handshake failed");
      goto cleanup;
      
    }
    
  }
  rtmpStream->arglen = 0;
  ThreadCreate(readPacketThread, rtmpStream);
  
  return 0;
cleanup:
  RTMP_LogPrintf("Closing connection... ");
  RTMP_Close(rtmpStream->rtmp);
  destroyMutex(rtmpStream->mutex);
  free(rtmpStream->rtmp);
  free(rtmpStream);
  RTMP_LogPrintf("done!\n\n");
quit:
  return -1;
}
/******************************************************************************
 * function: 
 * description: 
 * param {RTMP_STREAM} *rtmpStream
 * return {*}
********************************************************************************/
void closeRtmpStreaming(RTMP_STREAM *rtmpStream)
{
  if (rtmpStream) {
    if (rtmpStream->state != STREAMING_STOPPED) {
      rtmpStream->state = STREAMING_STOPPING;
      if(rtmpStream->rtmp) {
        RTMP_Close(rtmpStream->rtmp);
        free(rtmpStream->rtmp);
        rtmpStream->rtmp = NULL;
      }
      rtmpStream->state = STREAMING_STOPPED;
      if(rtmpStream->videoBuffer) {
        free(rtmpStream->videoBuffer);
        rtmpStream->videoBuffer = NULL;
      }
    }
    if(rtmpStream->mutex) {
      destroyMutex(rtmpStream->mutex);
      rtmpStream->mutex = NULL;
    }
    free(rtmpStream);
  }
}