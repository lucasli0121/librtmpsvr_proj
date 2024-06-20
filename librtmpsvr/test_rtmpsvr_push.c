/******************************************************************************
 * Author: liguoqiang
 * Date: 2023-12-25 23:22:09
 * LastEditors: liguoqiang
 * LastEditTime: 2024-05-04 19:38:21
 * Description: 
********************************************************************************/
#include "rtmpsrv.h"
#include "rtmp_client.h"
#include <stdio.h>
static RTMP_CLIENT* _rtmpClient = 0;
static RTMP_SERVER * _server = 0;
static int hasDataFrame = 0;
static MetaData _metaData;
static void beginPublish(void* user_data)
{
    printf("begin publish\n");
    hasDataFrame = 0;
    beforeSendData(_rtmpClient);
}
static void rtmpVideoReceiv(const char *data, int size, unsigned long timestamp, uint8_t absTimestamp, int key, void* user_data)
{
    if(hasDataFrame == 0) {
        hasDataFrame = 1;
        char *data = NULL;
        int dataSize = getRtmpMetaData(&data);
        getRtmpDataFrame(&_metaData);
        setMetaData(_rtmpClient, data, dataSize);
        setDataFrame(_rtmpClient, _metaData.duration, _metaData.width, _metaData.height, _metaData.fps, _metaData.encoder, _metaData.audioid, _metaData.audioVolume);
    }
    if(_server->needRawVideo) {
        sendRawVideoData(_rtmpClient, data, size, timestamp, absTimestamp);
    } else {
        sendX264VideoData(_rtmpClient, data, size, timestamp, absTimestamp);
    }
}
static void rtmpAudioReceiv(const char *data, int size, unsigned long timestamp, uint8_t absTimestamp, void* user_data)
{
    if(hasDataFrame == 0) {
        hasDataFrame = 1;
        char *data = NULL;
        int dataSize = getRtmpMetaData(&data);
        getRtmpDataFrame(&_metaData);
        setMetaData(_rtmpClient, data, dataSize);
    }
    sendAudioData(_rtmpClient, data, size, timestamp, absTimestamp);
}

int main(int argc, char const *argv[])
{
    char url[255], logFileName[255];
    RTMP_REQUEST* request = getDefaultRtmpRequest();
    request->rtmpport = 1935;
    _server = openRtmpServer(request, "./rtmp_svr.log");
    if(_server == NULL)
    {
        printf("open rtmp server failed\n");
        return -1;
    }
    printf("open rtmp server success\n");
    _server->needRawVideo = 0;
    setRtmpVideoCallback(rtmpVideoReceiv, NULL);
    setRtmpAudioCallback(rtmpAudioReceiv, NULL);
    setRtmpBeginPublishCallback(beginPublish, NULL);
    // strcpy(url, "rtmp://192.168.1.63:1935/live/test");   // rtmp://120.79.139.90:1935/live/test2
    strcpy(url, "rtmp://push.ecloudai.com/live/test");
    strcpy(logFileName, "./rtmp.log");
    _rtmpClient = startRtmpClient(url, logFileName);
    if(_rtmpClient == NULL)
    {
        printf("start rtmp client failed\n");
        closeRtmpServer(_server);
        return -1;
    }
    char ch = getchar();
    while(ch != 'q')
    {
        ch = getchar();
    }
    closeRtmpServer(_server);
    stopRtmpClient(_rtmpClient);
    return 0;
}