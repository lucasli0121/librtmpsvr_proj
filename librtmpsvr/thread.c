/*  Thread compatibility glue
 *  Copyright (C) 2009 Howard Chu
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with RTMPDump; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "librtmp/rtmp_sys.h"
#include "librtmp/log.h"
#include "thread.h"

#ifdef ANDROID
void bindToCpu(int cpu_id)
{
  int64_t cores = sysconf(_SC_NPROCESSORS_CONF);
  if(cpu_id>=cores) {
      return;
  }
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(cpu_id, &mask);
  sched_setaffinity(0, sizeof(mask), &mask);
}

#endif

#ifdef WIN32

#include <errno.h>
#include <sys/unistd.h>

void bindToCpu(int cpu_id)
{
}
THANDLE
ThreadCreate(thrfunc *routine, void *args)
{
  THANDLE thd;

  thd = (THANDLE) _beginthread(routine, 0, args);
  if (thd == INVALID_HANDLE_VALUE)
    RTMP_LogPrintf("%s, _beginthread failed with %d\n", __FUNCTION__, errno);

  return thd;
}

#else
THANDLE
ThreadCreate(thrfunc *routine, void *args)
{
  THANDLE id = 0;
  pthread_attr_t attributes;
  sched_param param;
  int ret;
  int policy = 0;
  int min_priority = sched_get_priority_min(SCHED_FIFO);
  int max_priority = sched_get_priority_max(SCHED_FIFO);
  pthread_attr_init(&attributes);
  // pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
  
  // ret = pthread_attr_getschedpolicy(&attributes, &policy);
  // if(ret != 0) {
  //   RTMP_Log(r, RTMP_LOGERROR, "%s, pthread_attr_getschedpolicy failed with %d\n", __FUNCTION__, ret);
  // } else {
  //   policy = SCHED_FIFO;
  //   ret = pthread_attr_setschedpolicy(&attributes, policy);
  //   if(ret != 0) {
  //     RTMP_Log(r, RTMP_LOGERROR, "%s, pthread_attr_setschedpolicy policy %d, failed with %d\n", __FUNCTION__, policy, ret);
  //   } else {
  //     ret = pthread_attr_getschedparam(&attributes, &param);
  //     if(ret != 0) {
  //       RTMP_Log(r, RTMP_LOGERROR, "%s, pthread_attr_getschedparam failed with %d\n", __FUNCTION__, ret);
  //     } else {
  //       param.sched_priority = max_priority;
  //       ret = pthread_attr_setschedparam(&attributes, &param);
  //       if(ret != 0) {
  //         RTMP_Log(r, RTMP_LOGERROR, "%s, pthread_attr_setschedparam failed with %d\n", __FUNCTION__, ret);
  //       }
  //     }
  //   }
  // }
  ret = pthread_create(&id, &attributes, routine, args);
  if (ret != 0) {
    id = 0;
  }
  return id;
}
#endif

void ThreadJoin(THANDLE thread)
{
#ifdef WIN32
  WaitForSingleObject(thread, INFINITE);
#else
  pthread_join(thread, NULL);
#endif
}


static void initLock(thread_mutex_t * mt)
{
#ifdef WIN32
	InitializeCriticalSection(mt);
#else
	pthread_mutex_init(mt, NULL);
#endif
}
static void destroyLock(thread_mutex_t *mt)
{
#ifdef WIN32
	DeleteCriticalSection(mt);
#else
	pthread_mutex_destroy(mt);
#endif
}

static void lock(thread_mutex_t * mt)
{
#ifdef WIN32
	EnterCriticalSection(mt);
#else
	pthread_mutex_lock(mt);
#endif
}

static void unlock(thread_mutex_t* mt)
{
#ifdef WIN32
	LeaveCriticalSection(mt);
#else
	pthread_mutex_unlock(mt);
#endif
}

static int trylock(thread_mutex_t* mt)
{
#ifdef WIN32
	return TryEnterCriticalSection(mt);
#else
	return (pthread_mutex_trylock(mt) == 0 ? 1 : 0);
#endif
}

RtmpMutex *createMutex()
{
    RtmpMutex *mutex = (RtmpMutex *)malloc(sizeof(RtmpMutex));
    mutex->init = initLock;
    mutex->lock = lock;
    mutex->unlock = unlock;
    mutex->destroy = destroyLock;
    mutex->trylock = trylock;
    mutex->init(&mutex->mutex);
    return mutex;
}

void destroyMutex(RtmpMutex *mutex)
{
    mutex->destroy(&mutex->mutex);
    free(mutex);
}