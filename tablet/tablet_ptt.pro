#-------------------------------------------------
# tablet_ptt.pro - 平板端 PTT 对讲程序
# Qt 5.14.2 + RK3588
# 支持 1对1 / 1对多 两种模式
#-------------------------------------------------

QT       += core gui multimedia widgets network
TEMPLATE  = app

CONFIG   += c++11

OBJECTS_DIR = ./tmp
MOC_DIR     = ./tmp
UI_DIR      = ./tmp
RCC_DIR     = ./tmp

TARGET    = ./bin/tablet_ptt

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

# ---- 平板端源文件 ----
HEADERS += \
    ModeSelectWidget.h \
    SinglePttWidget.h \
    MultiPttWidget.h

SOURCES += \
    main.cpp \
    ModeSelectWidget.cpp \
    SinglePttWidget.cpp \
    MultiPttWidget.cpp

# ---- 安装路径 (交叉编译时修改) ----
#target.path = /opt/qdialogue/bin
#INSTALLS   += target

# ---- 编译器选项 (RK3588 ARM64 优化) ----
# QMAKE_CXXFLAGS += -mcpu=cortex-a76 -mtune=cortex-a76

RESOURCES += \
    Resources.qrc
