/**
 ****************************************************************************************************
 * @file        lv_music.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-11-04
 * @brief       音乐播放器
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台: 正点原子 ESP32-S3 开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 ****************************************************************************************************
 */

#ifndef __LV_MUSIC_H
#define __LV_MUSIC_H

#include "lvgl.h"
#include "ff.h"
#include "lcd.h"
#include "app_ui.h"
#include "exfuns.h"
#include "ff.h"
#include "driver/i2s.h"
#include "driver/i2s_std.h"
#include "i2s.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "lvgl_demo.h"
#include "sdmmc_cmd.h"


#define WAV_TX_BUFSIZE    8192  /* 定义WAV TX DMA 数组大小(播放192Kbps@24bit的时候,需要设置8192大才不会卡) */

typedef struct
{
    uint32_t ChunkID;           /* chunk id;这里固定为"RIFF",即0X46464952 */
    uint32_t ChunkSize ;        /* 集合大小;文件总大小-8 */
    uint32_t Format;            /* 格式;WAVE,即0X45564157 */
}ChunkRIFF;     /* RIFF块 */

typedef struct
{
    uint32_t ChunkID;           /* chunk id;这里固定为"fmt ",即0X20746D66 */
    uint32_t ChunkSize ;        /* 子集合大小(不包括ID和Size);这里为:20. */
    uint16_t AudioFormat;       /* 音频格式;0X01,表示线性PCM;0X11表示IMA ADPCM */
    uint16_t NumOfChannels;     /* 通道数量;1,表示单声道;2,表示双声道; */
    uint32_t SampleRate;        /* 采样率;0X1F40,表示8Khz */
    uint32_t ByteRate;          /* /字节速率; */
    uint16_t BlockAlign;        /* 块对齐(字节); */
    uint16_t BitsPerSample;     /* 单个采样数据大小;4位ADPCM,设置为4 */
//    uint16_t ByteExtraData;   /* 附加的数据字节;2个; 线性PCM,没有这个参数 */
}ChunkFMT;      /* fmt块 */

typedef struct 
{
    uint32_t ChunkID;           /* chunk id;这里固定为"fact",即0X74636166; */
    uint32_t ChunkSize ;        /* 子集合大小(不包括ID和Size);这里为:4. */
    uint32_t NumOfSamples;      /* 采样的数量; */
}ChunkFACT;     /* fact块 */

typedef struct 
{
    uint32_t ChunkID;           /* chunk id;这里固定为"LIST",即0X74636166; */
    uint32_t ChunkSize ;        /* 子集合大小(不包括ID和Size);这里为:4. */
}ChunkLIST;     /* LIST块 */

typedef struct
{
    uint32_t ChunkID;           /* chunk id;这里固定为"data",即0X5453494C */
    uint32_t ChunkSize ;        /* 子集合大小(不包括ID和Size) */
}ChunkDATA;     /* data块 */

typedef struct
{ 
    ChunkRIFF riff;             /* riff块 */
    ChunkFMT fmt;               /* fmt块 */
//    ChunkFACT fact;           /* fact块 线性PCM,没有这个结构体 */
    ChunkDATA data;             /* data块 */
}__WaveHeader;  /* wav头 */

typedef struct
{ 
    uint16_t audioformat;       /* 音频格式;0X01,表示线性PCM;0X11表示IMA ADPCM */
    uint16_t nchannels;         /* 通道数量;1,表示单声道;2,表示双声道; */
    uint16_t blockalign;        /* 块对齐(字节); */
    uint32_t datasize;          /* WAV数据大小 */

    uint32_t totsec ;           /* 整首歌时长,单位:秒 */
    uint32_t cursec ;           /* 当前播放时长 */

    uint32_t bitrate;           /* 比特率(位速) */
    uint32_t samplerate;        /* 采样率 */
    uint16_t bps;               /* 位数,比如16bit,24bit,32bit */

    uint32_t datastart;         /* 数据帧开始的位置(在文件里面的偏移) */
}__wavctrl;                     /* wav 播放控制结构体 */ 

/* 音乐播放控制器 */
typedef struct
{
    uint8_t *tbuf;                          /* 临时数组,仅在24bit解码的时候需要用到 */
    FIL *file;                              /* 音频文件指针 */

    uint8_t status;                         /* bit0:0,暂停播放;1,继续播放 */
                                            /* bit1:0,结束播放;1,开启播放 */
}__audiodev;

/* 音乐播放状态 */
enum MUSIC_STATE
{
    MUSIC_NULL,
    MUSIC_PAUSE,
    MUSIC_PLAY,
    MUSIC_NEXT,
    MUSIC_PREV
};

/* 函数声明 */
void lv_music_demo(void);

#endif