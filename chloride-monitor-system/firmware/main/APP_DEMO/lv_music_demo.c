/**
 ****************************************************************************************************
 * @file        lv_music.c
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

#include "lv_music_demo.h"


LV_FONT_DECLARE(myFont12)           /* 声明myFont24字体,这个字体已经烧写到SPIFLASH中 */
LV_IMG_DECLARE(ui_img_album_png);

lv_obj_t *lv_music_cont;            /* 主窗口 */
lv_obj_t *lv_music_client_cont;     /* 窗口子容器 */
lv_obj_t *lv_music_win_header;      /* 窗口标题 */

static lv_obj_t *song_play_label;
static lv_obj_t *song_last_label;
static lv_obj_t *song_next_label;

static lv_obj_t *obj_cont_4;
static lv_obj_t *obj_cont_5;
static lv_obj_t *obj_cont_6;

static lv_obj_t *now_time_label;
static lv_obj_t *total_time_label;
static lv_obj_t *time_slider;
static lv_obj_t * song_name;

__wavctrl wavctrl;                          /* WAV控制结构体 */
__audiodev g_audiodev;                      /* 音乐播放控制器 */
uint8_t song_play_state = 0;
uint16_t curindex;                          /* 当前索引 */
FILINFO *wavfileinfo;                       /* 文件信息 */
uint8_t *pname;                             /* 带路径的文件名 */
uint32_t *wavoffsettbl;                     /* 音乐offset索引表 */
FF_DIR wavdir;                              /* 目录 */
uint16_t totwavnum;                         /* 音乐文件总数 */
uint8_t music_key = 0;
esp_err_t i2s_play_end = ESP_FAIL;
esp_err_t i2s_play_next_prev = ESP_FAIL;
uint32_t nr = 0;
FSIZE_t lv_res = 0;
uint8_t n = 0;
volatile long long int i2s_table_size = 0;
extern uint8_t sd_check_en;                 /* sd卡检测标志 */
/* MUSIC 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define MUSIC_PRIO      2                   /* 任务优先级 */
#define MUSIC_STK_SIZE  5*1024              /* 任务堆栈大小 */
TaskHandle_t            MUSICTask_Handler;  /* 任务句柄 */
void music(void *pvParameters);             /* 任务函数 */


/* PLAY 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define PLAY_PRIO      10                  /* 任务优先级 */
#define PLAY_STK_SIZE  5*1024              /* 任务堆栈大小 */
TaskHandle_t           PLAYTask_Handler;   /* 任务句柄 */
void plsy(void *pvParameters);             /* 任务函数 */


/**
 * @brief       开始音频播放
 * @param       无
 * @retval      无
 */
void lv_audio_start(void)
{
    g_audiodev.status = 3 << 0; /* 开始播放+非暂停 */
    i2s_trx_start();
}

/**
 * @brief       停止音频播放
 * @param       无
 * @retval      无
 */
void lv_audio_stop(void)
{
    g_audiodev.status = 0;
    i2s_trx_stop();
}

/**
 * @brief       显示播放时间,比特率 信息
 * @param       totsec : 音频文件总时间长度
 * @param       cursec : 当前播放时间
 * @param       bitrate: 比特率(位速)
 * @retval      无
 */
void audio_msg_show(uint32_t totsec, uint32_t cursec, uint32_t bitrate)
{
    static uint16_t playtime = 0xFFFF;                                  /* 放时间标记 */
    
    if (playtime != cursec)                                             /* 需要更新显示时间 */
    {
        playtime = cursec;
        lv_slider_set_range(time_slider,0,totsec);
        lv_slider_set_value(time_slider,playtime,LV_ANIM_ON);
        lv_label_set_text_fmt(now_time_label, "%d:%d", playtime / 60,playtime % 60);  /* 获取当前值，更新显示 */
        lv_label_set_text_fmt(total_time_label, "%ld:%ld", totsec / 60,totsec % 60);  /* 获取当前值，更新显示 */
    }
}

/**
 * @brief  音乐播放事件回调
 * @param  *e ：事件相关参数的集合，它包含了该事件的所有数据
 * @return 无
 */
static void song_play_event_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);                                         /* 获取触发源 */
    lv_event_code_t code = lv_event_get_code(e);                                       /* 获取事件类型 */
    
    if (target == obj_cont_4)        /* 启动，停止 */
    {
        if (code == LV_EVENT_CLICKED)
        {
            if (song_play_state ==0)
            {
                music_key = MUSIC_PLAY;
                lv_audio_stop();
                lv_label_set_text(song_play_label, LV_SYMBOL_PLAY);
                song_play_state = 1;
            }
            else
            {
                music_key = MUSIC_PAUSE;
                lv_audio_start();
                lv_label_set_text(song_play_label, LV_SYMBOL_PAUSE);
                song_play_state = 0;
            }
        }
    }
    else if (target == obj_cont_5)       /* 下一首 */
    {
        if (code == LV_EVENT_PRESSED)
        {
            lv_obj_set_style_text_color(song_last_label, lv_color_hex(0x272727), 0);
        }
        else if (code == LV_EVENT_RELEASED)
        {
            lv_audio_stop();
            music_key = MUSIC_NEXT;
            lv_label_set_text(song_play_label, LV_SYMBOL_PAUSE);
            lv_obj_set_style_text_color(song_last_label, lv_color_hex(0xffffff), 0);
            nr = 0;
            lv_res = 0;
            n = 0;
            i2s_table_size = 0;
            song_play_state = 0;
        }
    }
    else if (target == obj_cont_6)       /* 上一首 */
    {
        if (code == LV_EVENT_PRESSED)
        {
            lv_obj_set_style_text_color(song_next_label, lv_color_hex(0x272727), 0);
        }
        else if (code == LV_EVENT_RELEASED)
        {
            lv_audio_stop();
            music_key = MUSIC_PREV;
            lv_label_set_text(song_play_label, LV_SYMBOL_PAUSE);
            lv_obj_set_style_text_color(song_next_label, lv_color_hex(0xffffff), 0);
            nr = 0;
            lv_res = 0;
            n = 0;
            i2s_table_size = 0;
            song_play_state = 0;
        }
    }
}

/**
 * @brief       WAV解析初始化
 * @param       fname : 文件路径+文件名
 * @param       wavx  : 信息存放结构体指针
 * @retval      0,打开文件成功
 *              1,打开文件失败
 *              2,非WAV文件
 *              3,DATA区域未找到
 */
uint8_t wav_decode_init(uint8_t *fname, __wavctrl *wavx)
{
    FIL *ftemp;
    uint8_t *buf; 
    uint32_t br = 0;
    uint8_t res = 0;

    ChunkRIFF *riff;
    ChunkFMT *fmt;
    ChunkFACT *fact;
    ChunkDATA *data;
    
    ftemp = (FIL*)malloc(sizeof(FIL));
    buf = malloc(512);
    
    if (ftemp && buf)                                           /* 内存申请成功 */
    {
        res = f_open(ftemp, (TCHAR*)fname, FA_READ);            /* 打开文件 */
        
        if (res == FR_OK)
        {
            f_read(ftemp, buf, 512, (UINT *)&br);               /* 读取512字节在数据 */
            riff = (ChunkRIFF *)buf;                            /* 获取RIFF块 */
            
            if (riff->Format == 0x45564157)                     /* 是WAV文件 */
            {
                fmt = (ChunkFMT *)(buf + 12);                   /* 获取FMT块 */
                fact = (ChunkFACT *)(buf + 12 + 8 + fmt->ChunkSize);                    /* 读取FACT块 */
                
                if (fact->ChunkID == 0x74636166 || fact->ChunkID == 0x5453494C)
                {
                    wavx->datastart = 12 + 8 + fmt->ChunkSize + 8 + fact->ChunkSize;    /* 具有fact/LIST块的时候(未测试) */
                }
                else
                {
                    wavx->datastart = 12 + 8 + fmt->ChunkSize;
                }
                
                data = (ChunkDATA *)(buf + wavx->datastart);    /* 读取DATA块 */
                
                if (data->ChunkID == 0x61746164)                /* 解析成功! */
                {
                    wavx->audioformat = fmt->AudioFormat;       /* 音频格式 */
                    wavx->nchannels = fmt->NumOfChannels;       /* 通道数 */
                    wavx->samplerate = fmt->SampleRate;         /* 采样率 */
                    wavx->bitrate = fmt->ByteRate * 8;          /* 得到位速 */
                    wavx->blockalign = fmt->BlockAlign;         /* 块对齐 */
                    wavx->bps = fmt->BitsPerSample;             /* 位数,16/24/32位 */
                    
                    wavx->datasize = data->ChunkSize;           /* 数据块大小 */
                    wavx->datastart = wavx->datastart + 8;      /* 数据流开始的地方. */
                     
                    printf("wavx->audioformat:%d\r\n", wavx->audioformat);
                    printf("wavx->nchannels:%d\r\n", wavx->nchannels);
                    printf("wavx->samplerate:%ld\r\n", wavx->samplerate);
                    printf("wavx->bitrate:%ld\r\n", wavx->bitrate);
                    printf("wavx->blockalign:%d\r\n", wavx->blockalign);
                    printf("wavx->bps:%d\r\n", wavx->bps);
                    printf("wavx->datasize:%ld\r\n", wavx->datasize);
                    printf("wavx->datastart:%ld\r\n", wavx->datastart);  
                }
                else
                {
                    res = 3;                                    /* data区域未找到. */
                }
            }
            else
            {
                res = 2;        /* 非wav文件 */
            }
        }
        else
        {
            res = 1;            /* 打开文件错误 */
        }
    }
    
    f_close(ftemp);             /* 关闭文件 */
    free(ftemp);                /* 释放内存 */
    free(buf); 
    
    return 0;
}


/**
  * @brief  删除音乐demo
  * @param  无
  * @retval 无
  */
void lv_music_del(void)
{
    lv_audio_stop();
    f_close(g_audiodev.file);       /* 关闭文件 */

    if (PLAYTask_Handler != NULL)
    {
        vTaskDelete(PLAYTask_Handler);
        taskYIELD();
    }

    if (wavfileinfo || pname || wavoffsettbl)
    {
        free(wavfileinfo);                                      /* 释放内存 */
        free(pname);                                            /* 释放内存 */
        free(wavoffsettbl);                                     /* 释放内存 */
    }

    if (g_audiodev.file || g_audiodev.tbuf)
    {
        free(g_audiodev.file);
        free(g_audiodev.tbuf);
    }

    if (MUSICTask_Handler != NULL)
    {
        vTaskDelete(MUSICTask_Handler);
        taskYIELD();
    }

    PLAYTask_Handler = NULL;
    MUSICTask_Handler = NULL;

    es8388_adda_cfg(0,0);                               /* 开启DAC关闭ADC */
    es8388_input_cfg(0);                                /* 关闭输入 */
    es8388_output_cfg(0,0);                             /* DAC选择通道输出 */
    i2s_deinit();
    xl9555_pin_write(SPK_EN_IO,1);                      /* 关闭喇叭 */
    lv_obj_clean(app_obj_general.current_parent);
    lv_obj_del(app_obj_general.current_parent);
    app_obj_general.current_parent = NULL;
    lv_app_show();
    xSemaphoreGive(xGuiSemaphore);                      /* 释放互斥信号量 */
}

/**
 * @brief       play
 * @param       pvParameters : 传入参数(未用到)
 * @retval      无
 */
void play(void *pvParameters)
{
    pvParameters = pvParameters;

    while(1)
    {
        if ((g_audiodev.status & 0x0F) == 0x03)
        {
            f_lseek(g_audiodev.file, n ? lv_res : wavctrl.datastart);                          /* 跳过文件头 */

            for (uint16_t readTimes = 0; readTimes < (wavctrl.datasize / WAV_TX_BUFSIZE); readTimes++)
            {
                if ((g_audiodev.status & 0x0F) == 0x00)                                     /* 暂停播放 */
                {
                    if (music_key == MUSIC_PLAY || music_key == MUSIC_PAUSE)
                    {
                        lv_res = f_tell(g_audiodev.file);                                          /* 记录暂停位置 */
                        n = 1;
                    }

                    i2s_zero_dma_buffer(I2S_NUM);

                    if (music_key == 0xFF)
                    {
                        nr = 0;
                        lv_res = 0;
                        n = 0;
                        i2s_table_size = 0;
                        song_play_state = 0;
                        break;
                    }

                    break;
                }

                if (i2s_table_size >= wavctrl.datasize || i2s_play_next_prev == ESP_OK)   /* 是否播放完成 */
                {
                    n = 0;
                    i2s_table_size = 0;
                    i2s_play_end = ESP_OK;
                    i2s_zero_dma_buffer(I2S_NUM);
                    taskYIELD();
                    break;
                }
                /* 暂停时，拔出SD卡操作 */
                if (sdmmc_get_status(card) != ESP_OK)
                {
                    i2s_zero_dma_buffer(I2S_NUM);
                    g_audiodev.status = 0;

                    while (1)
                    {
                        back_act_key = KEY1_PRES;
                        taskYIELD();
                    }
                }

                f_read(g_audiodev.file,g_audiodev.tbuf, WAV_TX_BUFSIZE, (UINT*)&nr);        /* 读文件 */
                i2s_table_size = i2s_table_size + i2s_tx_write(g_audiodev.tbuf, WAV_TX_BUFSIZE);
                vTaskDelay(10);
            }
        }
        else
        {
            /* 暂停时，拔出SD卡操作 */
            if (sdmmc_get_status(card) != ESP_OK)
            {
                while (1)
                {
                    back_act_key = KEY1_PRES;
                    vTaskDelay(10);
                }
            }

            vTaskDelay(10);
        }
    }
}

/**
 * @brief       获取当前播放时间
 * @param       fx    : 文件指针
 * @param       wavx  : wavx播放控制器
 * @retval      无
 */
void wav_get_curtime(FIL *fx, __wavctrl *wavx)
{
    long long fpos;

    wavx->totsec = wavx->datasize / (wavx->bitrate / 8);    /* 歌曲总长度(单位:秒) */
    fpos = fx->fptr-wavx->datastart;                        /* 得到当前文件播放到的地方 */
    wavx->cursec = fpos * wavx->totsec / wavx->datasize;    /* 当前播放到第多少秒了? */
}

/**
 * @brief       得到path路径下，目标文件的总数
 * @param       path : 文件路径
 * @retval      有效文件总数
 */
uint16_t audio_get_tnum(uint8_t *path)
{
    uint8_t res;
    uint16_t rval = 0;
    FF_DIR tdir;                                                /* 临时目录 */
    FILINFO *tfileinfo;                                         /* 临时文件信息 */
    
    tfileinfo = (FILINFO*)malloc(sizeof(FILINFO));              /* 申请内存 */
    
    res = f_opendir(&tdir, (const TCHAR*)path);                 /* 打开目录 */
    
    if ((res == FR_OK) && tfileinfo)
    {
        while (1)                                               /* 查询总的有效文件数 */
        {
            res = f_readdir(&tdir, tfileinfo);                  /* 读取目录下的一个文件 */
            
            if ((res != FR_OK) || (tfileinfo->fname[0] == 0))
            {
                break;                                          /* 错误了/到末尾了,退出 */
            }

            res = exfuns_file_type(tfileinfo->fname);
            
            if ((res & 0xF0) == 0x40)                           /* 取高四位,看看是不是音乐文件 */
            {
                rval++;                                         /* 有效文件数增加1 */
            }
        }
    }
    
    free(tfileinfo);                                            /* 释放内存 */
    
    return rval;
}

/**
 * @brief       music
 * @param       pvParameters : 传入参数(未用到)
 * @retval      无
 */
void music(void *pvParameters)
{
    pvParameters = pvParameters;
    uint8_t res;
    uint32_t temp;
    g_audiodev.file = (FIL*)malloc(sizeof(FIL));
    g_audiodev.tbuf = malloc(WAV_TX_BUFSIZE);

    es8388_adda_cfg(1, 0);                              /* 开启DAC关闭ADC */
    es8388_input_cfg(0);                                /* 关闭输入 */
    es8388_output_cfg(1, 1);                            /* DAC选择通道输出 */
    es8388_hpvol_set(20);                               /* 设置耳机音量 */
    es8388_spkvol_set(20);                              /* 设置喇叭音量 */
    xl9555_pin_write(SPK_EN_IO,0);                      /* 打开喇叭 */

    while(1)
    {
        /* 记录索引 */
        res = f_opendir(&wavdir, "0:/MUSIC");                       /* 打开目录 */

        if (res == FR_OK)
        {
            curindex = 0;                                           /* 当前索引为0 */
            
            while (1)                                               /* 全部查询一遍 */
            {
                temp = wavdir.dptr;                                 /* 记录当前index */

                res = f_readdir(&wavdir, wavfileinfo);              /* 读取目录下的一个文件 */
                
                if ((res != FR_OK) || (wavfileinfo->fname[0] == 0))
                {
                    break;                                          /* 错误了/到末尾了,退出 */
                }

                res = exfuns_file_type(wavfileinfo->fname);
                
                if ((res & 0xF0) == 0x40)                           /* 取高四位,看看是不是音乐文件 */
                {
                    wavoffsettbl[curindex] = temp;                   /* 记录索引 */
                    curindex++;
                }
            }
        }

        curindex = 0;                                               /* 从0开始显示 */
        res = f_opendir(&wavdir, (const TCHAR*)"0:/MUSIC");         /* 打开目录 */
        
        while (res == FR_OK)                                        /* 打开成功 */
        {
            atk_dir_sdi(&wavdir, wavoffsettbl[curindex]);               /* 改变当前目录索引 */
            res = f_readdir(&wavdir, wavfileinfo);                  /* 读取目录下的一个文件 */
            
            if ((res != FR_OK) || (wavfileinfo->fname[0] == 0))
            {
                break;                                              /* 错误了/到末尾了,退出 */
            }
            
            strcpy((char *)pname, "0:/MUSIC/");                     /* 复制路径(目录) */
            strcat((char *)pname, (const char *)wavfileinfo->fname);/* 将文件名接在后面 */
            lv_label_set_text_fmt(song_name,"%s",wavfileinfo->fname);
            i2s_play_end = ESP_FAIL;
            i2s_play_next_prev = ESP_FAIL;
            music_key = 0;
            
            if (g_audiodev.file || g_audiodev.tbuf)
            {
                res = wav_decode_init(pname, &wavctrl);     /* 得到文件的信息 */
            }

            if (res == 0)                               /* 解析文件成功 */
            {
                if (wavctrl.bps == 16)
                {
                    es8388_sai_cfg(0, 3);               /* 飞利浦标准,16位数据长度 */
                    i2s_set_samplerate_bits_sample(wavctrl.samplerate,I2S_BITS_PER_SAMPLE_16BIT);
                }
                else if (wavctrl.bps == 24)
                {
                    es8388_sai_cfg(0, 0);               /* 飞利浦标准,24位数据长度 */
                    i2s_set_samplerate_bits_sample(wavctrl.samplerate,I2S_BITS_PER_SAMPLE_24BIT);
                }
            }

            res = f_open(g_audiodev.file, (TCHAR*)pname, FA_READ);  /* 打开文件 */

            lv_audio_stop();
            vTaskDelay(100);
            lv_audio_start();

            if (PLAYTask_Handler == NULL)
            {
                /* 创建MUSIC任务 */
                xTaskCreatePinnedToCore((TaskFunction_t )play,                 /* 任务函数 */
                                        (const char*    )"play",               /* 任务名称 */
                                        (uint16_t       )PLAY_STK_SIZE,        /* 任务堆栈大小 */
                                        (void*          )NULL,                 /* 传入给任务函数的参数 */
                                        (UBaseType_t    )PLAY_PRIO,            /* 任务优先级 */
                                        (TaskHandle_t*  )&PLAYTask_Handler,    /* 任务句柄 */
                                        (BaseType_t     ) 1);                  /* 该任务哪个内核运行 */
            }

            while (1)
            {
                xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);

                if (music_key == MUSIC_PREV)                                   /* 上一曲 */
                {
                    if (curindex)
                    {
                        curindex--;
                    }
                    else
                    {
                        curindex = totwavnum - 1;
                    }

                    i2s_play_next_prev = ESP_OK;
                    break;
                }
                else if (music_key == MUSIC_NEXT || i2s_play_end == ESP_OK) /* 下一曲 */
                {
                    curindex++;

                    if (curindex >= totwavnum)
                    {
                        curindex = 0;                                   /* 到末尾的时候,自动从头开始 */
                    }

                    i2s_play_next_prev = ESP_OK;
                    break;
                }

                if ((g_audiodev.status & 0x0F) == 0x03)     /* 暂停不刷新时间 */
                {
                    wav_get_curtime(g_audiodev.file, &wavctrl); /* 得到总时间和当前播放的时间 */
                    audio_msg_show(wavctrl.totsec, wavctrl.cursec, wavctrl.bitrate);
                }

                xSemaphoreGive(xGuiSemaphore); /* 释放互斥信号量 */

                vTaskDelay(100);
            }

            i2s_zero_dma_buffer(I2S_NUM);
            g_audiodev.status = 0;
            nr = 0;
            lv_res = 0;
            n = 0;
            i2s_table_size = 0;
            song_play_state = 0;
            music_key = 0xff;
            xSemaphoreGive(xGuiSemaphore); /* 释放互斥信号量 */
        }

        free(wavfileinfo);                                          /* 释放内存 */
        free(pname);                                                /* 释放内存 */
        free(wavoffsettbl);                                         /* 释放内存 */
    }
}

/**
 * @brief  音乐演示
 * @param  无
 * @return 无
 */
void lv_music_demo(void)
{
    nr = 0;
    lv_res = 0;
    n = 0;
    i2s_table_size = 0;
    song_play_state = 0;

    /* 解决重复按下 */
    if (app_obj_general.current_parent != NULL)
    {
        lv_obj_del(app_obj_general.current_parent);
        app_obj_general.current_parent = NULL;
    }

    if (sd_check_en == 0)
    {
        lv_msgbox("SD device not detected");
    }
    else
    {
        sd_check_en = 1;        /* SD卡已插入 */
        i2s_init();
        vTaskDelay(100);
        if (f_opendir(&wavdir, "0:/MUSIC"))                      /* 打开音乐文件夹 */
        {
            lv_msgbox("MUSIC folder error");
            return ;
        }
        
        totwavnum = audio_get_tnum((uint8_t *)"0:/MUSIC");/* 得到总有效文件数 */

        if (totwavnum == 0)
        {
            lv_msgbox("No music files");
            return ;
        }

        wavfileinfo = (FILINFO*)malloc(sizeof(FILINFO));            /* 申请内存 */
        pname = malloc(255 * 2 + 1);                                /* 为带路径的文件名分配内存 */
        wavoffsettbl = malloc(4 * totwavnum);                       /* 申请4*totwavnum个字节的内存,用于存放音乐文件off block索引 */

        if (!wavfileinfo || !pname || !wavoffsettbl)                /* 内存分配出错 */
        {
            lv_msgbox("memory allocation failed");
            return ;
        }

        lv_app_del();
        /* 创建该界面的主容器 */
        lv_obj_t *music_obj = lv_obj_create(lv_scr_act());
        lv_obj_set_style_radius(music_obj, 0, LV_STATE_DEFAULT);
        lv_obj_set_size(music_obj,lv_obj_get_width(lv_scr_act()),lv_obj_get_height(lv_scr_act()));
        lv_obj_set_style_border_opa(music_obj,LV_OPA_0,LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(music_obj,lv_color_make(0,0,0),LV_STATE_DEFAULT);
        lv_obj_set_pos(music_obj,0,0);
        lv_obj_clear_flag(music_obj, LV_OBJ_FLAG_SCROLLABLE);

        app_obj_general.current_parent = music_obj;                 /* 指向当前界面容器 */
        app_obj_general.Function = lv_music_del;                    /* 删除此界面 */

        /****************************** 子容器1 ******************************/
        
        lv_obj_t *obj_cont_1 = lv_obj_create(music_obj);                                                /* 创建子容器 */
        lv_obj_set_size(obj_cont_1, lv_obj_get_width(lv_scr_act()) - 90, lv_obj_get_width(lv_scr_act()) - 90);  /* 设置大小 */
        lv_obj_set_style_bg_color(obj_cont_1, lv_color_make(50,52,67), LV_STATE_DEFAULT);               /* 设置背景颜色 */
        lv_obj_align(obj_cont_1, LV_ALIGN_TOP_MID, 0, 50);                                              /* 设置位置 */
        lv_obj_set_style_border_opa(obj_cont_1, 0, 0);                                                  /* 去除边框 */
        lv_obj_set_style_radius(obj_cont_1, 200, 0);                                                    /* 设置圆角 */

        lv_obj_t * img_obj_cont_1 = lv_img_create(obj_cont_1);
        lv_obj_set_size(img_obj_cont_1,ui_img_album_png.header.w, ui_img_album_png.header.h);           /* 设置大小 */
        lv_img_set_src(img_obj_cont_1,&ui_img_album_png);
        lv_obj_center(img_obj_cont_1);
        
        song_name = lv_label_create(music_obj);
        lv_obj_align(song_name,LV_ALIGN_TOP_MID,0,25);
        lv_obj_set_style_text_font(song_name,&myFont12,LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(song_name,lv_color_hex(0xffffff),LV_STATE_DEFAULT);
        lv_label_set_text(song_name," ");
        /****************************** 时间条 ******************************/
        
        /* 时间条 */
        time_slider = lv_slider_create(music_obj);
        lv_obj_set_size(time_slider, lv_obj_get_width(lv_scr_act())/2 + 50, 5);
        lv_obj_align_to(time_slider, obj_cont_1, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
        lv_slider_set_range(time_slider,0,100);
        lv_obj_set_style_bg_color(time_slider, lv_color_hex(0x00FF00), LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(time_slider, 255, 0);
        lv_obj_set_style_bg_color(time_slider, lv_color_make(192,192,192), 0);
        lv_obj_remove_style(time_slider, NULL, LV_PART_KNOB);

        /* 进度时间标签 */
        now_time_label = lv_label_create(music_obj);
        lv_obj_set_style_text_font(now_time_label, &lv_font_montserrat_14, 0);                          /* 设置字体 */
        lv_label_set_text(now_time_label, "0:00");
        lv_obj_set_style_text_color(now_time_label, lv_color_hex(0xffffff), 0);
        lv_obj_align_to(now_time_label, time_slider, LV_ALIGN_OUT_LEFT_MID, 0, 0);
        
        /* 总时间标签 */
        total_time_label = lv_label_create(music_obj);
        lv_obj_set_style_text_font(total_time_label, &lv_font_montserrat_14, 0);                          /* 设置字体 */
        lv_label_set_text(total_time_label, "0:00");
        lv_obj_set_style_text_color(total_time_label, lv_color_hex(0xffffff), 0);
        lv_obj_align_to(total_time_label, time_slider, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
        
        /****************************** 子容器4 ******************************/
        
        obj_cont_4 = lv_obj_create(music_obj);                                                  /* 创建子容器 */
        lv_obj_set_size(obj_cont_4, 70, 70);                                                    /* 设置大小 */
        lv_obj_set_style_bg_color(obj_cont_4, lv_color_make(20,20,20), LV_STATE_DEFAULT);       /* 设置背景颜色 */
        lv_obj_align(obj_cont_4, LV_ALIGN_BOTTOM_MID, 0, -10);                                  /* 设置位置 */
        lv_obj_set_style_border_color(obj_cont_4,lv_color_hex(0xFFFFFF),LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(obj_cont_4,lv_color_hex(0x1E90FF),LV_STATE_FOCUS_KEY);
        lv_obj_set_style_translate_y(obj_cont_4, 5, LV_STATE_PRESSED);
        lv_obj_set_style_radius(obj_cont_4, 70, 0);                                             /* 设置圆角 */
        lv_obj_set_style_border_width(obj_cont_4,2,LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_color(obj_cont_4,lv_color_hex(0x1E90FF),LV_STATE_FOCUS_KEY);
        lv_obj_set_style_shadow_width(obj_cont_4,10,LV_STATE_FOCUS_KEY);
        lv_obj_add_event_cb(obj_cont_4, song_play_event_cb, LV_EVENT_CLICKED, NULL);
        
        /* 播放、暂停标签 */
        song_play_label = lv_label_create(obj_cont_4);
        lv_obj_set_style_text_font(song_play_label, &lv_font_montserrat_14, 0);                 /* 设置字体 */
        lv_label_set_text(song_play_label, LV_SYMBOL_PAUSE);
        lv_obj_set_style_text_color(song_play_label, lv_color_hex(0xffffff), 0);
        lv_obj_align(song_play_label, LV_ALIGN_CENTER, 0, 0);

        /****************************** 子容器5 ******************************/
        
        obj_cont_5 = lv_obj_create(music_obj);                                                  /* 创建子容器 */
        lv_obj_set_size(obj_cont_5, 50, 50);                                                    /* 设置大小 */
        lv_obj_align_to(obj_cont_5, obj_cont_4, LV_ALIGN_OUT_LEFT_MID, -20, 0);
        lv_obj_set_style_bg_color(obj_cont_5,lv_color_make(20,20,20),LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(obj_cont_5,lv_color_hex(0xFFFFFF),LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(obj_cont_5,lv_color_hex(0x1E90FF),LV_STATE_FOCUS_KEY);
        lv_obj_set_style_radius(obj_cont_5, 50, 0);                                             /* 设置圆角 */
        lv_obj_set_style_border_width(obj_cont_5,2,LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_color(obj_cont_5,lv_color_hex(0x1E90FF),LV_STATE_FOCUS_KEY);
        lv_obj_set_style_shadow_width(obj_cont_5,10,LV_STATE_FOCUS_KEY);
        lv_obj_add_event_cb(obj_cont_5, song_play_event_cb, LV_EVENT_ALL, NULL);
        
        /* 上一曲标签 */
        song_last_label = lv_label_create(obj_cont_5);
        lv_obj_set_style_text_font(song_last_label, &lv_font_montserrat_14, 0);                 /* 设置字体 */
        lv_label_set_text(song_last_label, LV_SYMBOL_PREV);
        lv_obj_set_style_text_color(song_last_label, lv_color_hex(0xffffff), 0);
        lv_obj_align(song_last_label, LV_ALIGN_CENTER, 0, 0);

        /****************************** 子容器6 ******************************/
        
        obj_cont_6 = lv_obj_create(music_obj);                                                  /* 创建子容器 */
        lv_obj_set_size(obj_cont_6, 50, 50);                                                    /* 设置大小 */
        lv_obj_align_to(obj_cont_6, obj_cont_4, LV_ALIGN_OUT_RIGHT_MID, 20, 0);
        lv_obj_set_style_bg_color(obj_cont_6,lv_color_make(20,20,20),LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(obj_cont_6,lv_color_hex(0xFFFFFF),LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(obj_cont_6,lv_color_hex(0x1E90FF),LV_STATE_FOCUS_KEY);
        lv_obj_set_style_radius(obj_cont_6, 50, 0);                                             /* 设置圆角 */
        lv_obj_set_style_border_width(obj_cont_6,2,LV_STATE_DEFAULT);
        lv_obj_set_style_shadow_color(obj_cont_6,lv_color_hex(0x1E90FF),LV_STATE_FOCUS_KEY);
        lv_obj_set_style_shadow_width(obj_cont_6,10,LV_STATE_FOCUS_KEY);
        lv_obj_add_event_cb(obj_cont_6, song_play_event_cb, LV_EVENT_ALL, NULL);
        
        /* 下一曲标签 */
        song_next_label = lv_label_create(obj_cont_6);
        lv_obj_set_style_text_font(song_next_label, &lv_font_montserrat_14, 0);                 /* 设置字体 */
        lv_label_set_text(song_next_label, LV_SYMBOL_NEXT);
        lv_obj_set_style_text_color(song_next_label, lv_color_hex(0xffffff), 0);
        lv_obj_align(song_next_label, LV_ALIGN_CENTER, 0, 0);
    
        lv_group_add_obj(ctrl_g, obj_cont_5);
        lv_group_add_obj(ctrl_g, obj_cont_4);
        lv_group_add_obj(ctrl_g, obj_cont_6);
        lv_group_remove_obj(time_slider);
        lv_group_focus_obj(obj_cont_5);                                     /* 聚焦 */

        if (MUSICTask_Handler == NULL)
        {
            /* 创建MUSIC任务 */
            xTaskCreatePinnedToCore((TaskFunction_t )music,                 /* 任务函数 */
                                    (const char*    )"music",               /* 任务名称 */
                                    (uint16_t       )MUSIC_STK_SIZE,        /* 任务堆栈大小 */
                                    (void*          )NULL,                  /* 传入给任务函数的参数 */
                                    (UBaseType_t    )MUSIC_PRIO,            /* 任务优先级 */
                                    (TaskHandle_t*  )&MUSICTask_Handler,    /* 任务句柄 */
                                    (BaseType_t     ) 0);                   /* 该任务哪个内核运行 */
        }
    }
}
