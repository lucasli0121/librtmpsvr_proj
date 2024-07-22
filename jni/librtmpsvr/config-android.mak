ifeq ("$(API)", "")
	API=32
endif
ifeq ("$(NDK)", "")
	NDK=E:/android-sdk-windows/ndk/26.3.11579264
endif

TARGET_ARCH=$(APP_ABI)
TARGET_ARCH_ABI=$(TARGET_ARCH)
TOOLCHAIN=$(NDK)/toolchains/llvm/prebuilt/$(SYSTEM_ARCH)
SYSTEM_ROOT=$(TOOLCHAIN)/sysroot
PREBUILT=$(TOOLCHAIN)/bin
#export PATH=$(PREBUILT):$(PATH)

ifeq ($(TARGET_ARCH), armeabi-v7a)
	ARCH=arm
	CPU=armv7-a
	EXTRA_CFLAGS="-march=armv7-a -mfloat-abi=softfp -mfpu=neon"
	EXTRA_LDFLAGS="-Wl,--fix-cortex-a8"

	CC=$(PREBUILT)/armv7a-linux-androideabi$(API)-clang
	CXX=$(PREBUILT)/armv7a-linux-androideabi$(API)-clang++
	LD=$(PREBUILT)/armv7a-linux-androideabi$(API)-clang
	DEPCC=$(PREBUILT)/armv7a-linux-androideabi$(API)-clang

	LDFLAGS=-L$(SYSTEM_ROOT)/usr/lib/arm-linux-androideabi -L$(SYSTEM_ROOT)/usr/lib/arm-linux-androideabi/$(API)
else
	ARCH=arm64
	CPU=armv8-a
	EXTRA_CFLAGS="-march=armv8-a"
	EXTRA_LDFLAGS=""
	EXTRA_LDLIBS=""

	CC=$(PREBUILT)/aarch64-linux-android$(API)-clang
	CXX=$(PREBUILT)/aarch64-linux-android$(API)-clang++
	LD=$(PREBUILT)/aarch64-linux-android$(API)-clang
	DEPCC=$(PREBUILT)/aarch64-linux-android$(API)-clang

	LDFLAGS=-L$(SYSTEM_ROOT)/usr/lib/aarch64-linux-android -L$(SYSTEM_ROOT)/usr/lib/aarch64-linux-androideabi/$(API)
endif

AS=$(CC)
ifeq ($(SYSTEM_ARCH), windows-x86_64)
	AR=$(PREBUILT)/llvm-ar.exe
	RANLIB=$(PREBUILT)/llvm-ranlib.exe
	NM=$(PREBUILT)/llvm-nm.exe
else
	AR=$(PREBUILT)/llvm-ar
	RANLIB=$(PREBUILT)/llvm-ranlib
	NM=$(PREBUILT)/llvm-nm
endif

YASM=$(PREBUILT)/yasm

CFLAGS= -fpic -fPIC -fpermissive -Wall -fexceptions -Wdeprecated-declarations -D__STDC_CONSTANT_MACROS -DNO_CRYPTO -DANDROID
LDLIBS = -lc -ldl -llog -lm -landroid
# SHARED_LIBS = libc libdl liblog libm libandroid
# STATIC_LIBS = libc.a libdl.a libm.a






