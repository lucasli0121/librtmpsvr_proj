MAKE=make
CC=g++
LD=g++
RM=rm
AR=ar
DEBUG= -g
PARAM=-c $(DEBUG) -Wall -O2 -o
OUT_PATH=/usr/local/librtmpsvr

#-I/usr/include \

LIBRTMP_HOME=../librtmpsvr
CFLAG=-I/usr/local/include \
	-I$(MINGW)/include \
	-I./ \
	-I$(LIBRTMP_HOME) \
	-DWIN32 -D__STDC_CONSTANT_MACROS -DNO_CRYPTO -D__CRT__NO_INLINE
#-L/usr/lib

LDFLAG=-L/usr/local/lib -L./ 

EXTRA_LIBS=-lws2_32 -lwinmm -lgdi32 -pthread -lz

SRC=librtmp/amf.c \
	librtmp/hashswf.c \
	librtmp/log.c \
	librtmp/parseurl.c \
	librtmp/rtmp.c \
	rtmp_stream.c \
	rtmpsrv.c \
	thread.c \
	packet_fifo.c \
	rtmp_client.c \
	utils.c

OBJ=amf.o \
	hashswf.o \
	log.o \
	parseurl.o \
	rtmp.o \
	rtmp_stream.o \
	rtmpsrv.o \
	thread.o \
	packet_fifo.o \
	rtmp_client.o \
	utils.o

# $(LD) -static -fPIC -static-libgcc -static-libstdc++ -o $@ $? $(EXTRA_LIBS) $(LDFLAG) -Wl,--out-implib,$(IMPLIB)

SO_LIB = librtmpsvr.so
IMPLIB = librtmpsvr.lib

all:test_rtmpsrv test_rtmpsvr_push $(SO_LIB)

test_rtmpsvr_push:test_rtmpsvr_push.o $(SO_LIB)
	$(CC) -o $@ $? $(LDFLAG) $(OBJ) $(EXTRA_LIBS)
test_rtmpsrv:test_rtmpsrv.o $(SO_LIB)
	$(CC) -o $@ $?  $(LDFLAG) -lrtmpsvr $(EXTRA_LIBS)
$(SO_LIB):$(OBJ)
	$(LD) -shared -fPIC -Wall -o $@ $(OBJ) $(LDFLAG) $(EXTRA_LIBS) -Wl,--out-implib,$(IMPLIB)
amf.o:librtmp/amf.c
	$(CC) $(PARAM) $@ $? $(CFLAG)
hashswf.o:librtmp/hashswf.c
	$(CC) $(PARAM) $@ $? $(CFLAG)
log.o:librtmp/log.c
	$(CC) $(PARAM) $@ $? $(CFLAG)
parseurl.o:librtmp/parseurl.c
	$(CC) $(PARAM) $@ $? $(CFLAG)
rtmp.o:librtmp/rtmp.c
	$(CC) $(PARAM) $@ $? $(CFLAG)	
rtmp_stream.o:rtmp_stream.c
	$(CC) $(PARAM) $@ $? $(CFLAG)
thread.o:thread.c
	$(CC) $(PARAM) $@ $? $(CFLAG)
rtmpsrv.o:rtmpsrv.c
	$(CC) $(PARAM) $@ $? $(CFLAG)
rtmp_client.o:rtmp_client.c
	$(CC) $(PARAM) $@ $? $(CFLAG)
utils.o:utils.c
	$(CC) $(PARAM) $@ $? $(CFLAG)

test_rtmpsrv.o:test_rtmpsrv.c
	$(CC) $(PARAM) $@ $? $(CFLAG)
test_rtmpsvr_push.o:test_rtmpsvr_push.c
	$(CC) $(PARAM) $@ $? $(CFLAG)

install:
	mkdir -p $(OUT_PATH)
	mkdir -p $(OUT_PATH)/lib
	mkdir -p $(OUT_PATH)/include
	mkdir -p $(OUT_PATH)/include/librtmp
	mkdir -p $(OUT_PATH)/bin
	cp -vf test_rtmpsrv $(OUT_PATH)/bin
	cp -vf test_rtmpsvr_push $(OUT_PATH)/bin
	cp -vf $(SO_LIB) $(OUT_PATH)/lib
	cp -vf $(IMPLIB) $(OUT_PATH)/lib
	cp -Rvf librtmp/*.h $(OUT_PATH)/include/librtmp
	cp -vf *.h $(OUT_PATH)/include
clean:
	-$(RM) -vf $(OBJ)
	$(RM) -vf test_rtmpsrv
	$(RM) -vf test_rtmpsvr_push
	$(RM) -vf $(LIB_A)
	$(RM) -vf $(SO_LIB)

