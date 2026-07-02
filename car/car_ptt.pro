#-------------------------------------------------
# car_ptt.pro - 小车端 PTT 守护进程
# Qt 5.14.2 + RK3588 (Console, 无 GUI)
#-------------------------------------------------

QT       += core multimedia network
QT       -= gui


CONFIG   += c++11 console
CONFIG   -= app_bundle


OBJECTS_DIR = ./tmp
MOC_DIR     = ./tmp
UI_DIR      = ./tmp
RCC_DIR     = ./tmp


TARGET    = ./bin/car_ptt
TEMPLATE  = app

# ---- Opus 库 ----
LIBS += -lopus

# ---- 包含路径 ----
INCLUDEPATH += $$PWD/../common

# ---- 共享模块源文件 ----
HEADERS += \
    ../common/MultiAudioManager.h \
    ../common/CarEndpoint.h \
    ../common/JitterBuffer.h \
    ../common/OpusCodec.h \
    ../common/PTTProtocol.h \
    ../common/RadioLink.h

SOURCES += \
    ../common/MultiAudioManager.cpp \
    ../common/JitterBuffer.cpp \
    ../common/OpusCodec.cpp \
    ../common/RadioLink.cpp

# ---- 小车端源文件 ----
HEADERS += \
    CarDaemon.h

SOURCES += \
    main.cpp \
    CarDaemon.cpp

# ---- 安装路径 ----
#target.path = /opt/qdialogue/bin
#INSTALLS   += target

# ---- 编译器选项 (RK3588 ARM64 优化) ----
# QMAKE_CXXFLAGS += -mcpu=cortex-a76 -mtune=cortex-a76
