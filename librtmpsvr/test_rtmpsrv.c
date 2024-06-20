/******************************************************************************
 * Author: liguoqiang
 * Date: 2023-12-25 23:22:09
 * LastEditors: liguoqiang
 * LastEditTime: 2024-05-04 22:27:42
 * Description: 
********************************************************************************/
#include "rtmpsrv.h"
#include <stdio.h>
FILE * _fp = NULL;
static void rtmpBeginPublish(void* user_data)
{
    printf("rtmp begin publish\n");
}

static void rtmpVideoReceiv(const char *data, int size, unsigned long timestamp, uint8_t absTimestamp, int key, void* user_data)
{
    printf("Video::timestamp:%lu, adsTimestemp:%d, size:%d, key:%d\n", timestamp, absTimestamp, size, key);
    int ret = 0;
    do {
        ret = fwrite(data, 1, size, _fp);
        size -= ret;
        data += ret;
    }while(size > 0);
}
static void rtmpAudioReceiv(const char *data, int size, unsigned long timestamp, uint8_t absTimestamp, void* user_data)
{
    printf("Audio::timestamp:%lu, adsTimestemp:%d, size:%d\n", timestamp, absTimestamp, size);
}

int main(int argc, char const *argv[])
{
    RTMP_SERVER * server = openRtmpServer(getDefaultRtmpRequest(), "./test_rtmpsvr.log");
    if(server == NULL)
    {
        printf("open rtmp server failed\n");
        return -1;
    }
    printf("open rtmp server success\n");
    setRtmpVideoCallback(rtmpVideoReceiv, NULL);
    setRtmpAudioCallback(rtmpAudioReceiv, NULL);
    setRtmpBeginPublishCallback(rtmpBeginPublish, NULL);
    _fp = fopen("./test_rtmp.264", "wb");
    char ch = getchar();
    while(ch != 'q')
    {
        ch = getchar();
    }
    closeRtmpServer(server);
    fflush(_fp);
    fclose(_fp);
    return 0;
}