/*
 * @Author: liguoqiang
 * @Date: 2024-01-13 07:09:03
 * @LastEditors: liguoqiang
 * @LastEditTime: 2024-02-27 23:33:28
 * @Description: 
 */
/******************************************************************************
 * Author: liguoqiang
 * Date: 2023-12-22 15:14:00
 * LastEditors: liguoqiang
 * LastEditTime: 2024-07-23 14:34:39
 * Description: RTMP Server
 * 根据原librtmp种的rtmpsrv修改
 * 1. 去掉了对rtmpdump的依赖
 * 2. 完善了对rtmp协议的支持
 * 3. 增加了rtmp server端的支持
 * 4. 目前只实现了推流的服务端
********************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include <signal.h>
#include <getopt.h>

#include <assert.h>

#include "librtmp/rtmp_sys.h"
#include "librtmp/log.h"

#ifdef linux
#include <linux/netfilter_ipv4.h>
#endif

#ifndef WIN32
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include "rtmpsrv.h"

#define RD_SUCCESS 0
#define RD_FAILED 1
#define RD_INCOMPLETE 2

#ifdef WIN32
#define InitSockets()              \
  {                                \
    WORD version;                  \
    WSADATA wsaData;               \
                                   \
    version = MAKEWORD(1, 1);      \
    WSAStartup(version, &wsaData); \
  }

#define CleanupSockets() WSACleanup()
#else
#define InitSockets()
#define CleanupSockets()
#endif


static FILE * _logFile = NULL;

// define default params
RTMP_REQUEST defaultRTMPRequest = {
  "0.0.0.0",
  1935,
  RTMP_PROTOCOL_RTMP,
  4096,
  1024 * 1024,
  30,
  20 * 1000};

RTMP_REQUEST *getDefaultRtmpRequest()
{
  return &defaultRTMPRequest;
}

static RTMP_LogContext *_logCtx = NULL;
static RTMP_REQUEST rtmpRequest = {};
static RTMP_SERVER *startRtmpServer();
static void stopRtmpServer(RTMP_SERVER *server);
static RTMP_STREAM *rtmpStream = 0;
static VideoCallbackFunc videoFunc = 0;
static AudioCallbackFunc audioFunc = 0;
static BeginPublishFunc _beginPublishFunc = 0;
static ExitPublishThreadFunc _exitPublishThreadFunc = 0;
static SpsPpsCallbackFunc _spsPpsFunc = 0;

int getRtmpMetaData(char** data)
{
  if(rtmpStream == 0) {
    return 0;
  }
  *data = rtmpStream->dataFrameBuf;
  return rtmpStream->dataFrameSize;
}
void getRtmpDataFrame(MetaData *metaData)
{
  if(rtmpStream == 0) {
    return;
  }
  metaData->duration = rtmpStream->duration;
  metaData->width = rtmpStream->width;
  metaData->height = rtmpStream->height;
  metaData->fps = rtmpStream->framerate;
  memcpy(metaData->audioid, rtmpStream->audioid, sizeof(rtmpStream->audioid));
  metaData->audioVolume = rtmpStream->audioVolume;
  memcpy(metaData->encoder, rtmpStream->encoder, sizeof(rtmpStream->encoder));
}
/******************************************************************************
 * function: serverThread
 * description: a thread function to accept client connection
 * param {RTMP_SERVER*} server
 * return {*}
********************************************************************************/
static TFTYPE serverThread(void* v)
{
  int ret = 0;
  RTMP_SERVER *server = (RTMP_SERVER *)v;
  server->state = STREAMING_ACCEPTING;
  while (server->state == STREAMING_ACCEPTING) {
    struct sockaddr_in addr;
    
    socklen_t addrlen = sizeof(struct sockaddr_in);
    int sockfd = accept(server->socket, (struct sockaddr *)&addr, &addrlen);
    if (sockfd > 0) {
#ifdef linux
      struct sockaddr_in dest;
      char destch[16];
      socklen_t destlen = sizeof(struct sockaddr_in);
      getsockopt(sockfd, SOL_IP, SO_ORIGINAL_DST, &dest, &destlen);
      strcpy(destch, inet_ntoa(dest.sin_addr));
      RTMP_Log(_logCtx, RTMP_LOGINFO, "%s: accepted connection from %s to %s\n", __FUNCTION__,
               inet_ntoa(addr.sin_addr), destch);
#else
      RTMP_Log(_logCtx, RTMP_LOGINFO, "%s: accepted connection from %s\n", __FUNCTION__, inet_ntoa(addr.sin_addr));
#endif
      
      /* Create a new thread and transfer the control to that */
      if (rtmpStream != 0 && rtmpStream->rtmp != 0 && streamIsConnect(rtmpStream) == 1) {
        closesocket(sockfd);
        RTMP_Log(_logCtx, RTMP_LOGINFO, "%s: accept a new socket but old stream still running, so close the socket", __FUNCTION__);
        continue;
      }
      closeRtmpStreaming(rtmpStream);
      rtmpStream = 0;
      rtmpStream = initRtmpStream(rtmpRequest.chunkSize, server->needRawVideo);
      rtmpStream->rtmp->logCtx = _logCtx;
      ret = openRtmpStreaming(rtmpStream, sockfd);
      if (ret < 0) {
        RTMP_Log(_logCtx, RTMP_LOGERROR, "%s: openRtmpStreaming failed", __FUNCTION__);
      } else {
        rtmpStream->videoFunc = videoFunc;
        rtmpStream->audioFunc = audioFunc;
        rtmpStream->beginPublishFunc = _beginPublishFunc;
        rtmpStream->exitPublishThreadFunc = _exitPublishThreadFunc;
        rtmpStream->spsPpsFunc = _spsPpsFunc;
      }
    } else {
      RTMP_Log(_logCtx, RTMP_LOGERROR, "%s: accept failed, sockerr:%d", __FUNCTION__, GetSockError());
    }
  }
  server->state = STREAMING_STOPPING;
  return TFRET();
}

/******************************************************************************
 * function: startRtmpServer
 * description: start rtmp server, listen on port and wait for client to connect
 * return {*}
********************************************************************************/
static RTMP_SERVER * startRtmpServer()
{
  struct sockaddr_in addr;
  int sockfd, tmp;
  RTMP_SERVER *server = 0;

  sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (sockfd == -1)
  {
    RTMP_Log(_logCtx, RTMP_LOGERROR, "%s, couldn't create socket", __FUNCTION__);
    goto exit;
  }

  tmp = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&tmp, sizeof(tmp));

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(rtmpRequest.hostname); // htonl(INADDR_ANY);
  addr.sin_port = htons(rtmpRequest.rtmpport);

  if (bind(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1) {
    RTMP_Log(_logCtx, RTMP_LOGERROR, "%s, TCP bind failed for port number: %d", __FUNCTION__, rtmpRequest.rtmpport);
    goto exit;
  }

  if (listen(sockfd, 10) == -1) {
    RTMP_Log(_logCtx, RTMP_LOGERROR, "%s, listen failed", __FUNCTION__);
    goto clean;
  }
  server = (RTMP_SERVER *)malloc(sizeof(RTMP_SERVER));
  server->socket = sockfd;
  server->host = rtmpRequest.hostname;
  server->port = rtmpRequest.rtmpport;
  server->needRawVideo = 0;
  server->state = STREAMING_LISTENING;
  server->thread = ThreadCreate(serverThread, server);
  return server;
clean:
  closesocket(sockfd);
exit:
  return server;
}
/******************************************************************************
 * function: stopRtmpServer
 * description: 
 * param {RTMP_SERVER} *server
 * return {*}
********************************************************************************/
static void stopRtmpServer(RTMP_SERVER *server)
{
  if(!server) {
    return;
  }
  if (server->state != STREAMING_STOPPED) {
    server->state = STREAMING_STOPPING;
    if(server->socket != -1) {
      closesocket(server->socket);
      server->socket = -1;
    }
    if (server->thread) {
      ThreadJoin(server->thread);
      server->thread = 0;
    }
    server->state = STREAMING_STOPPED;
  }
  free(server);
}

/******************************************************************************
 * function: openRtmpServer
 * description: open rtmp server, listen on port and wait for client to connect
 * param {char} *host
 * param {int} port
 * param {int} chunkSize
 * param {int} bw
 * param {int} timeout
 * param {int} maxClients
 * return {*}
********************************************************************************/
RTMP_SERVER* openRtmpServer(RTMP_REQUEST * request, const char* logFileName)
{
  int nStatus = RD_SUCCESS;

  if(logFileName && strlen(logFileName) > 0) {
      _logFile = fopen(logFileName, "w");
  }
  _logCtx = RTMP_LogInit();
  _logCtx->file = _logFile;
  RTMP_LogSetLevel(_logCtx, RTMP_LOGERROR);
  // init request
  memset(&rtmpRequest, 0, sizeof(RTMP_REQUEST));
  if(request) {
    memcpy(&rtmpRequest, request, sizeof(RTMP_REQUEST));
  } else {
    memcpy(&rtmpRequest, &defaultRTMPRequest, sizeof(RTMP_REQUEST));
  }
  InitSockets();
  return startRtmpServer();
}
/******************************************************************************
 * function: closeRtmpServer
 * description: 
 * return {*}
********************************************************************************/
void closeRtmpServer(RTMP_SERVER *rtmpServer)
{
  stopRtmpServer(rtmpServer);
  rtmpServer = NULL;
  CleanupSockets();
  RTMP_Log(_logCtx, RTMP_LOGDEBUG, "Done, exiting...");
  RTMP_LogUninit(_logCtx);
  if(_logFile) {
    fclose(_logFile);
  }
}

void setRtmpVideoCallback(VideoCallbackFunc func, void* user_data)
{
  if(rtmpStream == 0) {
    videoFunc = func;
  } else {
    rtmpStream->videoFunc = func;
    rtmpStream->user_data = user_data;
  }
}
void setRtmpAudioCallback(AudioCallbackFunc func, void* user_data)
{
  if(rtmpStream == 0) {
    audioFunc = func;
  } else {
    rtmpStream->audioFunc = func;
    rtmpStream->user_data = user_data;
  }
}
void setRtmpBeginPublishCallback(BeginPublishFunc func, void* user_data)
{
  if(rtmpStream == 0) {
    _beginPublishFunc = func;
  } else {
    rtmpStream->beginPublishFunc = func;
    rtmpStream->user_data = user_data;
  }
}
void setExitPublishThreadCallback(ExitPublishThreadFunc exitPublishThreadFunc, void *user_data)
{
  if(rtmpStream == 0) {
    _exitPublishThreadFunc = exitPublishThreadFunc;
  } else {
    rtmpStream->exitPublishThreadFunc = exitPublishThreadFunc;
    rtmpStream->user_data = user_data;
  }

}
void setRtmpSpsPpsCallback(SpsPpsCallbackFunc spsPpsFunc, void *user_data)
{
  if(rtmpStream == 0) {
    _spsPpsFunc = spsPpsFunc;
  } else {
    rtmpStream->spsPpsFunc = spsPpsFunc;
    rtmpStream->user_data = user_data;
  }
}