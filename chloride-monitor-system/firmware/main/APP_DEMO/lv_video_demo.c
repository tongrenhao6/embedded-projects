/**
 ****************************************************************************************************
 * @file        lv_video.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-11-04
 * @brief       视频播放器
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


static portMUX_TYPE my_spinlock = portMUX_INITIALIZER_UNLOCKED;
AVI_INFO g_avix;                                        /* avi文件相关信息 */
char *const AVI_VIDS_FLAG_TBL[2] = {"00dc", "01dc"};    /* 视频编码标志字符串,00dc/01dc */
char *const AVI_AUDS_FLAG_TBL[2] = {"00wb", "01wb"};    /* 音频编码标志字符串,00wb/01wb */

uint16_t video_curindex;                                /* 当前索引 */
FILINFO *vfileinfo;                                     /* 文件信息 */
uint8_t *video_pname;                                   /* 带路径的文件名 */
uint16_t totavinum;                                     /* 音乐文件总数 */
uint8_t *framebuf;
uint32_t *voffsettbl;
FF_DIR vdir;
FIL *video_favi;
extern uint8_t frameup;
static lv_obj_t * video_frame;
static lv_obj_t * video_time_slider;
static lv_obj_t * video_now_time_label;
static lv_obj_t * video_total_time_label;
static lv_obj_t *video_obj_cont_4;
static lv_obj_t *video_play_label;
static lv_obj_t *video_obj_cont_5;
static lv_obj_t *video_last_label;
static lv_obj_t *video_obj_cont_6;
static lv_obj_t *video_next_label;
uint8_t video_play_state = 0;
uint8_t video_key = 0;
lv_timer_t * video_timer;
extern uint8_t sd_check_en;                         /* sd卡检测标志 */
uint8_t exit_flasg = 0;
extern SemaphoreHandle_t BinarySemaphore;          /* 二值信号量句柄 */
extern uint8_t decode_en;     /* 图片解码后才能退出 */

/* VIDEO 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define VIDEO_PRIO      24                              /* 任务优先级 */
#define VIDEO_STK_SIZE  4 * 1024                        /* 任务堆栈大小 */
TaskHandle_t            VIDEOTask_Handler;              /* 任务句柄 */
void video(void *pvParameters);                         /* 任务函数 */

/**
 * @brief  视频播放事件回调
 * @param  *e ：事件相关参数的集合，它包含了该事件的所有数据
 * @return 无
 */
static void video_play_event_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);                                         /* 获取触发源 */
    lv_event_code_t code = lv_event_get_code(e);                                       /* 获取事件类型 */
    
    if (target == video_obj_cont_4)        /* 启动，停止 */
    {
        if (code == LV_EVENT_CLICKED)
        {
            if (video_play_state ==0)
            {
                video_key = VIDEO_PAUSE;
                lv_label_set_text(video_play_label, LV_SYMBOL_PLAY);
                video_play_state = 1;
            }
            else
            {
                video_key = VIDEO_PLAY;
                lv_label_set_text(video_play_label, LV_SYMBOL_PAUSE);
                video_play_state = 0;
            }
        }
    }
    else if (target == video_obj_cont_5)       /* 下一首 */
    {
        if (code == LV_EVENT_PRESSED)
        {
            lv_obj_set_style_text_color(video_last_label, lv_color_hex(0x272727), 0);
        }
        else if (code == LV_EVENT_RELEASED)
        {
            video_key = VIDEO_NEXT;
            lv_obj_set_style_text_color(video_last_label, lv_color_hex(0xffffff), 0);
            lv_label_set_text(video_play_label, LV_SYMBOL_PAUSE);
            video_play_state = 0;
        }
    }
    else if (target == video_obj_cont_6)       /* 上一首 */
    {
        if (code == LV_EVENT_PRESSED)
        {
            lv_obj_set_style_text_color(video_next_label, lv_color_hex(0x272727), 0);
        }
        else if (code == LV_EVENT_RELEASED)
        {
            video_key = VIDEO_PREV;
            lv_obj_set_style_text_color(video_next_label, lv_color_hex(0xffffff), 0);
            lv_label_set_text(video_play_label, LV_SYMBOL_PAUSE);
            video_play_state = 0;
        }
    }
}

/**
 * @brief       avi解码初始化
 * @param       buf  : 输入缓冲区
 * @param       size : 缓冲区大小
 * @retval      res
 *    @arg      OK,avi文件解析成功
 *    @arg      其他,错误代码
 */
AVISTATUS avi_init(uint8_t *buf, uint32_t size)
{
    uint16_t offset;
    uint8_t *tbuf;
    AVISTATUS res = AVI_OK;
    AVI_HEADER *aviheader;
    LIST_HEADER *listheader;
    AVIH_HEADER *avihheader;
    STRH_HEADER *strhheader;

    STRF_BMPHEADER *bmpheader;
    STRF_WAVHEADER *wavheader;

    tbuf = buf;
    aviheader = (AVI_HEADER *)buf;
    if (aviheader->RiffID != AVI_RIFF_ID)
    {
        return AVI_RIFF_ERR;        /* RIFF ID错误 */
    }

    if (aviheader->AviID != AVI_AVI_ID)
    {
        return AVI_AVI_ERR;         /* AVI ID错误 */
    }

    buf += sizeof(AVI_HEADER);      /* 偏移 */
    listheader = (LIST_HEADER *)(buf);
    if (listheader->ListID != AVI_LIST_ID)
    {
        return AVI_LIST_ERR;        /* LIST ID错误 */
    }

    if (listheader->ListType != AVI_HDRL_ID)
    {
        return AVI_HDRL_ERR;        /* HDRL ID错误 */
    }

    buf += sizeof(LIST_HEADER);     /* 偏移 */
    avihheader = (AVIH_HEADER *)(buf);
    if (avihheader->BlockID != AVI_AVIH_ID)
    {
        return AVI_AVIH_ERR;        /* AVIH ID错误 */
    }

    g_avix.SecPerFrame = avihheader->SecPerFrame;   /* 得到帧间隔时间 */
    g_avix.TotalFrame = avihheader->TotalFrame;     /* 得到总帧数 */
    buf += avihheader->BlockSize + 8;               /* 偏移 */
    listheader = (LIST_HEADER *)(buf);
    if (listheader->ListID != AVI_LIST_ID)
    {
        return AVI_LIST_ERR;        /* LIST ID错误 */
    }

    if (listheader->ListType != AVI_STRL_ID)
    {
        return AVI_STRL_ERR;        /* STRL ID错误 */
    }

    strhheader = (STRH_HEADER *)(buf + 12);
    if (strhheader->BlockID != AVI_STRH_ID)
    {
        return AVI_STRH_ERR;        /* STRH ID错误 */
    }

    if (strhheader->StreamType == AVI_VIDS_STREAM)  /* 视频帧在前 */
    {
        if (strhheader->Handler != AVI_FORMAT_MJPG)
        {
            return AVI_FORMAT_ERR;  /* 非MJPG视频流,不支持 */
        }

        g_avix.VideoFLAG = AVI_VIDS_FLAG_TBL[0];    /* 视频流标记  "00dc" */
        g_avix.AudioFLAG = AVI_AUDS_FLAG_TBL[1];    /* 音频流标记  "01wb" */
        bmpheader = (STRF_BMPHEADER *)(buf + 12 + strhheader->BlockSize + 8);   /* strf */
        if (bmpheader->BlockID != AVI_STRF_ID)
        {
            return AVI_STRF_ERR;    /* STRF ID错误 */
        }

        g_avix.Width = bmpheader->bmiHeader.Width;
        g_avix.Height = bmpheader->bmiHeader.Height;
        buf += listheader->BlockSize + 8;       /* 偏移 */
        listheader = (LIST_HEADER *)(buf);
        if (listheader->ListID != AVI_LIST_ID)  /* 是不含有音频帧的视频文件 */
        {
            g_avix.SampleRate = 0;              /* 音频采样率 */
            g_avix.Channels = 0;                /* 音频通道数 */
            g_avix.AudioType = 0;               /* 音频格式 */

        }
        else
        {
            if (listheader->ListType != AVI_STRL_ID)
            {
                return AVI_STRL_ERR;    /* STRL ID错误 */
            }

            strhheader = (STRH_HEADER *)(buf + 12);
            if (strhheader->BlockID != AVI_STRH_ID)
            {
                return AVI_STRH_ERR;    /* STRH ID错误 */
            }

            if (strhheader->StreamType != AVI_AUDS_STREAM)
            {
                return AVI_FORMAT_ERR;  /* 格式错误 */
            }

            wavheader = (STRF_WAVHEADER *)(buf + 12 + strhheader->BlockSize + 8);   /* strf */
            if (wavheader->BlockID != AVI_STRF_ID)
            {
                return AVI_STRF_ERR;    /* STRF ID错误 */
            }

            g_avix.SampleRate = wavheader->SampleRate;      /* 音频采样率 */
            g_avix.Channels = wavheader->Channels;          /* 音频通道数 */
            g_avix.AudioType = wavheader->FormatTag;        /* 音频格式 */
        }
    }
    else if (strhheader->StreamType == AVI_AUDS_STREAM)     /* 音频帧在前 */
    { 
        g_avix.VideoFLAG = AVI_VIDS_FLAG_TBL[1];            /* 视频流标记  "01dc" */
        g_avix.AudioFLAG = AVI_AUDS_FLAG_TBL[0];            /* 音频流标记  "00wb" */
        wavheader = (STRF_WAVHEADER *)(buf + 12 + strhheader->BlockSize + 8);   /* strf */
        if (wavheader->BlockID != AVI_STRF_ID)
        {
            return AVI_STRF_ERR;                            /* STRF ID错误 */
        }
 
        g_avix.SampleRate = wavheader->SampleRate;          /* 音频采样率 */
        g_avix.Channels = wavheader->Channels;              /* 音频通道数 */
        g_avix.AudioType = wavheader->FormatTag;            /* 音频格式 */
        buf += listheader->BlockSize + 8;                   /* 偏移 */
        listheader = (LIST_HEADER *)(buf);
        if (listheader->ListID != AVI_LIST_ID)
        {
            return AVI_LIST_ERR;    /* LIST ID错误 */
        }

        if (listheader->ListType != AVI_STRL_ID)
        {
            return AVI_STRL_ERR;    /* STRL ID错误 */
        }

        strhheader = (STRH_HEADER *)(buf + 12);
        if (strhheader->BlockID != AVI_STRH_ID)
        {
            return AVI_STRH_ERR;    /* STRH ID错误 */
        }

        if (strhheader->StreamType != AVI_VIDS_STREAM)
        {
            return AVI_FORMAT_ERR;  /* 格式错误 */
        }

        bmpheader = (STRF_BMPHEADER *)(buf + 12 + strhheader->BlockSize + 8);   /* strf */
        if (bmpheader->BlockID != AVI_STRF_ID)
        {
            return AVI_STRF_ERR;    /* STRF ID错误 */
        }

        if (bmpheader->bmiHeader.Compression != AVI_FORMAT_MJPG)
        {
            return AVI_FORMAT_ERR;  /* 格式错误 */
        }

        g_avix.Width = bmpheader->bmiHeader.Width;
        g_avix.Height = bmpheader->bmiHeader.Height;
    }

    offset = avi_srarch_id(tbuf, size, "movi");     /* 查找movi ID */
    if (offset == 0)
    {
        return AVI_MOVI_ERR;        /* MOVI ID错误 */
    }

    if (g_avix.SampleRate)          /* 有音频流,才查找 */
    {
        tbuf += offset;
        offset = avi_srarch_id(tbuf, size, g_avix.AudioFLAG);   /* 查找音频流标记 */
        if (offset == 0)
        {
            return AVI_STREAM_ERR;  /* 流错误 */
        }
        tbuf += offset + 4;
        g_avix.AudioBufSize = *((uint16_t *)tbuf);              /* 得到音频流buf大小. */
    }

    printf("avi init ok\r\n");
    printf("g_avix.SecPerFrame:%ld\r\n", g_avix.SecPerFrame);
    printf("g_avix.TotalFrame:%ld\r\n", g_avix.TotalFrame);
    printf("g_avix.Width:%ld\r\n", g_avix.Width);
    printf("g_avix.Height:%ld\r\n", g_avix.Height);
    printf("g_avix.AudioType:%d\r\n", g_avix.AudioType);
    printf("g_avix.SampleRate:%ld\r\n", g_avix.SampleRate);
    printf("g_avix.Channels:%d\r\n", g_avix.Channels);
    printf("g_avix.AudioBufSize:%d\r\n", g_avix.AudioBufSize);
    printf("g_avix.VideoFLAG:%s\r\n", g_avix.VideoFLAG);
    printf("g_avix.AudioFLAG:%s\r\n", g_avix.AudioFLAG);

    return res;
}

/**
 * @brief       查找 ID
 * @param       buf  : 输入缓冲区
 * @param       size : 缓冲区大小
 * @param       id   : 要查找的id, 必须是4字节长度
 * @retval      执行结果
 *   @arg       0     , 没找到
 *   @arg       其他  , movi ID偏移量
 */
uint32_t avi_srarch_id(uint8_t *buf, uint32_t size, char *id)
{
    uint32_t i;
    uint32_t idsize = 0;
    size -= 4;
    for (i = 0; i < size; i++)
    {
        if ((buf[i] == id[0]) &&
            (buf[i + 1] == id[1]) &&
            (buf[i + 2] == id[2]) &&
            (buf[i + 3] == id[3]))
        {
            idsize = MAKEDWORD(buf + i + 4);    /* 得到帧大小,必须大于16字节,才返回,否则不是有效数据 */

            if (idsize > 0X10)return i;         /* 找到"id"所在的位置 */
        }
    }

    return 0;
}

/**
 * @brief       得到stream流信息
 * @param       buf  : 流开始地址(必须是01wb/00wb/01dc/00dc开头)
 * @retval      执行结果
 *   @arg       AVI_OK, AVI文件解析成功
 *   @arg       其他  , 错误代码
 */
AVISTATUS avi_get_streaminfo(uint8_t *buf)
{
    g_avix.StreamID = MAKEWORD(buf + 2);    /* 得到流类型 */
    g_avix.StreamSize = MAKEDWORD(buf + 4); /* 得到流大小 */

    if (g_avix.StreamSize > AVI_MAX_FRAME_SIZE)   /* 帧大小太大了,直接返回错误 */
    {
        printf("FRAME SIZE OVER:%ld\r\n", g_avix.StreamSize);
        g_avix.StreamSize = 0;
        return AVI_STREAM_ERR;
    }
    
    if (g_avix.StreamSize % 2)
    {
        g_avix.StreamSize++;    /* 奇数加1(g_avix.StreamSize,必须是偶数) */
    }

    if (g_avix.StreamID == AVI_VIDS_FLAG || g_avix.StreamID == AVI_AUDS_FLAG)
    {
        return AVI_OK;
    }

    return AVI_STREAM_ERR;
}

/**
 * @brief       显示当前播放时间
 * @param       favi   : 当前播放的视频文件
 * @param       aviinfo: avi控制结构体
 * @retval      无
 */
void video_time_show(FIL *favi, AVI_INFO *aviinfo)
{
    static uint32_t oldsec;                                         /* 上一次的播放时间 */
    
    uint32_t totsec = 0;                                            /* video文件总时间 */
    uint32_t cursec;                                                /* 当前播放时间 */
    
    totsec = (aviinfo->SecPerFrame / 1000) * aviinfo->TotalFrame;   /* 歌曲总长度(单位:ms) */
    totsec /= 1000;                                                 /* 秒钟数. */
    cursec = ((double)favi->fptr / favi->obj.objsize) * totsec;     /* 获取当前播放到多少秒 */
    
    if (oldsec != cursec)                                           /* 需要更新显示时间 */
    {
        oldsec = cursec;
        lv_slider_set_range(video_time_slider,0,totsec);
        lv_slider_set_value(video_time_slider,cursec,LV_ANIM_ON);
        lv_label_set_text_fmt(video_now_time_label, "%02ld:%02ld:%02ld", cursec / 3600, (cursec % 3600) / 60, cursec % 60);  /* 获取当前值，更新显示 */
        lv_label_set_text_fmt(video_total_time_label, "%02ld:%02ld:%02ld", totsec / 3600, (totsec % 3600) / 60, totsec % 60);  /* 获取当前值，更新显示 */
    }
}

extern uint8_t * video_buf;
extern struct jpeg_decompress_struct *cinfo;
extern struct my_error_mgr *jerr;


lv_img_dsc_t video_img_dsc = {
    .header.always_zero = 0,
    .header.cf = LV_IMG_CF_TRUE_COLOR,
    .data = NULL,
};

/**
  * @brief  删除视频demo
  * @param  无
  * @retval 无
  */
void lv_video_del(void)
{
    int i = 10;
    exit_flasg = 1;
    vTaskDelete(VIDEOTask_Handler);
    taskYIELD();
    VIDEOTask_Handler = NULL;
    esp_timer_delete(esp_tim_handle);
    mjpegdec_free();                                                /* 释放内存 */

    if ((framebuf != NULL))
    {
        free(framebuf);
        framebuf = NULL;
    }

    if ((vfileinfo != NULL) || (video_pname != NULL) || (voffsettbl != NULL)) /* 内存分配出错 */
    {
        free(vfileinfo);
        free(video_pname);
        free(voffsettbl);
    }

    lv_obj_del(app_obj_general.current_parent);
    app_obj_general.current_parent = NULL;

    lv_app_show();
    xSemaphoreGive(xGuiSemaphore);
}

/**
 * @brief       video显示视频
 * @param       无
 * @retval      无
 */
void video_show(uint32_t w,uint32_t h,uint8_t * video_buf)
{
    video_img_dsc.header.w = w;
    video_img_dsc.header.h = h;
    video_img_dsc.data_size = w * h * 2;
    video_img_dsc.data = (const uint8_t *)video_buf;
    lv_img_set_src(video_frame,&video_img_dsc);
    video_time_show(video_favi, &g_avix);
}

/**
 * @brief       video
 * @param       pvParameters : 传入参数(未用到)
 * @retval      无
 */
void video(void *pvParameters)
{
    pvParameters = pvParameters;
    uint8_t res = 0;
    uint8_t key = 0;
    uint8_t temp = 0;
    uint8_t *pbuf = NULL;
    uint32_t nr = 0;
    uint16_t offset = 0;

    res = (uint8_t)f_opendir(&vdir, "0:/VIDEO");                                /* 打开目录 */

    if (res == FR_OK)
    {
        video_curindex = 0;

        while (1)
        {
            temp = vdir.dptr;                                                   /* 记录当前dptr偏移 */
            res = (uint8_t)f_readdir(&vdir, vfileinfo);                         /* 读取下一个文件 */
            
            if ((res != 0) || (vfileinfo->fname[0] == 0))                       /* 错误或到末尾，退出 */
            {
                break;
            }
            
            res = exfuns_file_type(vfileinfo->fname);
            
            if ((res & 0xF0) == 0x60)                                           /* 是视频文件 */
            {
                voffsettbl[video_curindex] = temp;                                  /* 记录索引 */
                video_curindex++;
            }
        }

        video_curindex = 0;
        res = (uint8_t)f_opendir(&vdir, "0:/VIDEO");                                /* 打开目录 */

        while (1)
        {
            atk_dir_sdi(&vdir, voffsettbl[video_curindex]);                             /* 改变当前目录索引 */
            res = (uint8_t)f_readdir(&vdir, vfileinfo);                             /* 读取目录的下一个文件 */
            
            if ((res != 0) || (vfileinfo->fname[0] == 0))                           /* 错误或到末尾，退出 */
            {
                printf("err\r\n");
                break;
            }
            
            strcpy((char *)video_pname, "0:/VIDEO/");                               /* 复制路径（目录） */
            strcat((char *)video_pname, (const char *)vfileinfo->fname);            /* 将文件名接在后面 */
            memset(framebuf, 0, AVI_MAX_FRAME_SIZE);
            
            res = (uint8_t)f_open(video_favi, (const TCHAR *)video_pname, FA_READ);

            if (res == FR_OK)
            {
                pbuf = framebuf;
                res = (uint8_t)f_read(video_favi, pbuf, AVI_MAX_FRAME_SIZE, (UINT*)&nr);  /* 开始读取 */

                if (res != 0)
                {
                    printf("fread error:%d\r\n", res);
                    break;
                }

                res = avi_init(pbuf, AVI_MAX_FRAME_SIZE);                           /* AVI解析 */

                if (res != 0)
                {
                    printf("avi error:%d\r\n", res);
                    break;
                }

                esptim_int_init(g_avix.SecPerFrame / 1000, 1000);
                offset = avi_srarch_id(pbuf, AVI_MAX_FRAME_SIZE, "movi");   /* 寻找movi ID */
                avi_get_streaminfo(pbuf + offset + 4);                      /* 获取流信息 */
                f_lseek(video_favi, offset + 12);                           /* 跳过标志ID，读地址偏移到流数据开始处 */
                res = mjpegdec_init(0,0);                                   /* 初始化JPG解码 */

                if (res != 0)
                {
                    printf("mjpegdec Fail\r\n");
                    break;
                }

                if (Windows_Width * Windows_Height == g_avix.Width * g_avix.Height)
                {
                    continue;
                }
                else
                {
                    /* 定义图像的宽高 */
                    Windows_Width = g_avix.Width;
                    Windows_Height = g_avix.Height;

                    if (video_buf == NULL)
                    {
                        mjpegdec_malloc();
                    }
                }

                if (g_avix.SampleRate)                                                      /* 有音频信息,才初始化 */
                {

                }

                while (1)
                {
                    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
                    if (g_avix.StreamID == AVI_VIDS_FLAG)                                   /* 视频流 */
                    {
                        pbuf = framebuf;
                        f_read(video_favi, pbuf, g_avix.StreamSize + 8, (UINT*)&nr);        /* 读取整帧+下一帧数据流ID信息 */
                        res = mjpegdec_decode(pbuf, g_avix.StreamSize,video_show);
                        
                        if (res != 0)
                        {
                            printf("decode error!\r\n");
                        }

                        while (frameup == 0);                                   /* 等待播放时间到达 */
                        frameup = 0;
                    }
                    else
                    {
                        f_read(video_favi, framebuf, g_avix.StreamSize + 8, &nr);     /* 填充psaibuf */
                        pbuf = framebuf;
                    }

                    /* 播放视频过程中拔出SD卡的操作 */
                    if (sdmmc_get_status(card) != ESP_OK)
                    {
                        xSemaphoreGive(xGuiSemaphore); /* 释放互斥信号量 */

                        while (1)
                        {
                            back_act_key = KEY1_PRES;
                            vTaskDelay(10);
                        }
                    }

                    if (video_key == VIDEO_NEXT || video_key == VIDEO_PREV)                   /* KEY0/KEY2按下,播放下一个/上一个视频 */
                    {
                        key = video_key;
                        video_key = VIDEO_NULL;
                        break;
                    }
                    else if (video_key == VIDEO_PAUSE || video_key == VIDEO_PLAY)
                    {
                        xSemaphoreGive(xGuiSemaphore); /* 释放互斥信号量 */

                        while (1)
                        {
                            if (video_key == VIDEO_PLAY || video_key == VIDEO_NEXT || video_key == VIDEO_PREV)
                            {
                                break;
                            }
                            /* 暂停视频时，拔出SD卡操作 */
                            if (sdmmc_get_status(card) != ESP_OK)
                            {
                                xSemaphoreGive(xGuiSemaphore); /* 释放互斥信号量 */

                                while (1)
                                {
                                    back_act_key = KEY1_PRES;
                                    vTaskDelay(10);
                                }
                            }

                            vTaskDelay(10);
                        }

                        xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
                    }

                    if (avi_get_streaminfo(pbuf + g_avix.StreamSize) != 0)      /* 读取下一帧流标志 */
                    {
                        key = VIDEO_NEXT;
                        video_key = VIDEO_NULL;
                        break;
                    }

                    xSemaphoreGive(xGuiSemaphore); /* 释放互斥信号量 */
                    vTaskDelay(pdMS_TO_TICKS(5));                               /* 延时5毫秒 */
                }

                if (key == VIDEO_PREV)                                   /* 上一曲 */
                {
                    if (video_curindex)
                    {
                        video_curindex--;
                    }
                    else
                    {
                        video_curindex = totavinum - 1;
                    }
                }
                else if (key == VIDEO_NEXT) /* 下一曲 */
                {
                    video_curindex++;

                    if (video_curindex >= totavinum)
                    {
                        video_curindex = 0;                                   /* 到末尾的时候,自动从头开始 */
                    }

                }

                esp_timer_delete(esp_tim_handle);
                mjpegdec_free();                                                /* 释放内存 */
                f_close(video_favi);                                            /* 关闭文件 */
                xSemaphoreGive(xGuiSemaphore); /* 释放互斥信号量 */
            }
        }
    }
}

/**
 * @brief       获取指定路径下有效视频文件的数量
 * @param       path: 指定路径
 * @retval      有效视频文件的数量
 */
static uint16_t video_get_tnum(char *path)
{
    uint8_t res;
    uint16_t rval = 0;
    FF_DIR tdir;
    FILINFO *tfileinfo;
    
    tfileinfo = (FILINFO *)malloc(sizeof(FILINFO));             /* 申请内存 */
    res = (uint8_t)f_opendir(&tdir, (const TCHAR *)path);       /* 打开目录 */
    
    if ((res == 0) && tfileinfo)
    {
        while (1)                                               /* 查询总的有效文件数 */
        {
            res = (uint8_t)f_readdir(&tdir, tfileinfo);         /* 读取目录下的一个文件 */
            
            if ((res != 0) || (tfileinfo->fname[0] == 0))       /* 错误或到末尾，退出 */
            {
                break;
            }
            
            res = exfuns_file_type(tfileinfo->fname);
            
            if ((res & 0xF0) == 0x60)                           /* 是视频文件 */
            {
                rval++;
            }
        }
    }
    
    free(tfileinfo);                                            /* 释放内存 */
    
    return rval;
}

/**
 * @brief  视频演示
 * @param  无
 * @return 无
 */
void lv_video_demo(void)
{
    video_curindex = 0;                                /* 当前索引 */
    vfileinfo = 0;                                     /* 文件信息 */
    video_pname = 0;                                   /* 带路径的文件名 */
    totavinum = 0;                                     /* 音乐文件总数 */
    framebuf = NULL;
    voffsettbl = 0;
    video_favi = NULL;
    video_key = VIDEO_NULL;
    video_play_state = 0;
    exit_flasg = 0;

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

        if (f_opendir(&vdir, "0:/VIDEO"))                       /* 打开音乐文件夹 */
        {
            lv_msgbox("MUSIC folder error");
            return ;
        }
        
        totavinum = video_get_tnum("0:/VIDEO");                 /* 得到总有效文件数 */

        if (totavinum == 0)
        {
            lv_msgbox("No music files");
            return ;
        }

        vfileinfo = (FILINFO*)malloc(sizeof(FILINFO));          /* 申请内存 */
        video_pname = (uint8_t *)malloc(255 * 2 + 1);           /* 为带路径的文件名分配内存 */
        voffsettbl = (uint32_t *)malloc(4 * totavinum);         /* 申请4*totavinum个字节的内存,用于存放音乐文件off block索引 */

        if ((vfileinfo == NULL) || (video_pname == NULL) || (voffsettbl == NULL)) /* 内存分配出错 */
        {
            lv_msgbox("memory allocation failed");
            return ;
        }

        framebuf = (uint8_t *)malloc(AVI_MAX_FRAME_SIZE);       /* 申请内存 */
        video_favi = (FIL *)malloc(sizeof(FIL));

        if ((framebuf == NULL) || (video_favi == NULL))
        {
            lv_msgbox("memory error!");
            return ;
        }

        memset(framebuf, 0, AVI_MAX_FRAME_SIZE);
        memset(video_pname, 0, 255 * 2 + 1);
        memset(voffsettbl, 0, 4 * totavinum);

        lv_app_del();
        /* 创建该界面的主容器 */
        lv_obj_t *video_obj = lv_obj_create(lv_scr_act());
        lv_obj_set_style_radius(video_obj, 0, LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(video_obj, lv_color_make(0,0,0), LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(video_obj,LV_OPA_100,LV_STATE_DEFAULT);
        lv_obj_set_size(video_obj,lv_obj_get_width(lv_scr_act()),lv_obj_get_height(lv_scr_act()));
        lv_obj_set_style_border_opa(video_obj,LV_OPA_0,LV_STATE_DEFAULT);
        lv_obj_set_pos(video_obj,0,0);
        lv_obj_clear_flag(video_obj, LV_OBJ_FLAG_SCROLLABLE);

        app_obj_general.current_parent = video_obj;                 /* 指向当前界面容器 */
        app_obj_general.Function = lv_video_del;                    /* 删除此界面 */

        video_frame = lv_img_create(video_obj);
        lv_obj_remove_style_all(video_frame);
        lv_obj_set_style_outline_color(video_frame,lv_color_hex(0xC0C0C0),LV_STATE_DEFAULT);
        lv_obj_set_style_outline_width(video_frame,2,LV_STATE_DEFAULT);
        lv_obj_align(video_frame,LV_ALIGN_TOP_MID,0,21);

        /* 时间条 */
        video_time_slider = lv_slider_create(video_obj);
        lv_obj_set_size(video_time_slider, lv_obj_get_width(lv_scr_act()) - 20, 5);
        lv_slider_set_range(video_time_slider,0,100);
        lv_obj_align_to(video_time_slider, video_frame, LV_ALIGN_OUT_BOTTOM_MID, 0, 200);
        lv_obj_set_style_bg_color(video_time_slider, lv_color_hex(0x00FF00), LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(video_time_slider, 255, 0);
        lv_obj_set_style_bg_color(video_time_slider, lv_color_make(192,192,192), LV_STATE_DEFAULT);
        lv_obj_remove_style(video_time_slider, NULL, LV_PART_KNOB);

        /* 进度时间标签 */
        video_now_time_label = lv_label_create(video_obj);
        lv_obj_set_style_text_font(video_now_time_label, &lv_font_montserrat_14, 0);                          /* 设置字体 */
        lv_label_set_text(video_now_time_label, "0:00");
        lv_obj_set_style_text_color(video_now_time_label, lv_color_hex(0xffffff), 0);
        lv_obj_align_to(video_now_time_label, video_time_slider, LV_ALIGN_OUT_TOP_LEFT, 0, -10);
        
        /* 总时间标签 */
        video_total_time_label = lv_label_create(video_obj);
        lv_obj_set_style_text_font(video_total_time_label, &lv_font_montserrat_14, 0);                          /* 设置字体 */
        lv_label_set_text(video_total_time_label, "0:00");
        lv_obj_set_style_text_color(video_total_time_label, lv_color_hex(0xffffff), 0);
        lv_obj_align_to(video_total_time_label, video_time_slider, LV_ALIGN_OUT_TOP_RIGHT, -20, -10);

        /****************************** 子容器4 ******************************/
        
        video_obj_cont_4 = lv_obj_create(video_obj);                                            /* 创建子容器 */
        lv_obj_remove_style_all(video_obj_cont_4);
        lv_obj_set_size(video_obj_cont_4, 70, 70);                                              /* 设置大小 */
        lv_obj_set_style_bg_color(video_obj_cont_4, lv_color_make(58,58,70), LV_PART_MAIN);      /* 设置背景颜色 */
        lv_obj_align(video_obj_cont_4, LV_ALIGN_BOTTOM_MID, 0, -10);                            /* 设置位置 */
        lv_obj_set_style_border_color(video_obj_cont_4,lv_color_hex(0xFFFFFF),LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(video_obj_cont_4,lv_color_hex(0x1E90FF),LV_STATE_FOCUS_KEY);
        lv_obj_set_style_border_width(video_obj_cont_4,2,LV_STATE_DEFAULT);
        lv_obj_set_style_translate_y(video_obj_cont_4, 5, LV_STATE_PRESSED);
        lv_obj_set_style_shadow_color(video_obj_cont_4,lv_color_hex(0x1E90FF),LV_STATE_FOCUS_KEY);
        lv_obj_set_style_shadow_width(video_obj_cont_4,10,LV_STATE_FOCUS_KEY);
        lv_obj_set_style_radius(video_obj_cont_4, 70, 0);                                       /* 设置圆角 */
        lv_obj_add_event_cb(video_obj_cont_4, video_play_event_cb, LV_EVENT_CLICKED, NULL);
        
        /* 播放、暂停标签 */
        video_play_label = lv_label_create(video_obj_cont_4);
        lv_obj_set_style_text_font(video_play_label, &lv_font_montserrat_14, 0);                /* 设置字体 */
        lv_label_set_text(video_play_label, LV_SYMBOL_PAUSE);
        lv_obj_set_style_text_color(video_play_label, lv_color_hex(0xffffff), 0);
        lv_obj_align(video_play_label, LV_ALIGN_CENTER, 0, 0);

        /****************************** 子容器5 ******************************/
        
        video_obj_cont_5 = lv_obj_create(video_obj);                                            /* 创建子容器 */
        lv_obj_remove_style_all(video_obj_cont_5);
        lv_obj_set_size(video_obj_cont_5, 50, 50);                                              /* 设置大小 */
        lv_obj_set_style_border_color(video_obj_cont_5,lv_color_hex(0xFFFFFF),LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(video_obj_cont_5,lv_color_hex(0x1E90FF),LV_STATE_FOCUS_KEY);
        lv_obj_align_to(video_obj_cont_5, video_obj_cont_4, LV_ALIGN_OUT_LEFT_MID, -20, 0);
        lv_obj_set_style_border_width(video_obj_cont_5,2,LV_STATE_DEFAULT);
        lv_obj_set_style_translate_y(video_obj_cont_5, 5, LV_STATE_PRESSED);
        lv_obj_set_style_shadow_color(video_obj_cont_5,lv_color_hex(0x1E90FF),LV_STATE_FOCUS_KEY);
        lv_obj_set_style_shadow_width(video_obj_cont_5,10,LV_STATE_FOCUS_KEY);
        lv_obj_set_style_radius(video_obj_cont_5, 50, 0);                                       /* 设置圆角 */
        lv_obj_add_event_cb(video_obj_cont_5, video_play_event_cb, LV_EVENT_ALL, NULL);
        
        /* 上一曲标签 */
        video_last_label = lv_label_create(video_obj_cont_5);
        lv_obj_set_style_text_font(video_last_label, &lv_font_montserrat_14, 0);                /* 设置字体 */
        lv_label_set_text(video_last_label, LV_SYMBOL_PREV);
        lv_obj_set_style_text_color(video_last_label, lv_color_hex(0xffffff), 0);
        lv_obj_align(video_last_label, LV_ALIGN_CENTER, 0, 0);

        /****************************** 子容器6 ******************************/
        
        video_obj_cont_6 = lv_obj_create(video_obj);                                            /* 创建子容器 */
        lv_obj_remove_style_all(video_obj_cont_6);
        lv_obj_set_size(video_obj_cont_6, 50, 50);                                              /* 设置大小 */
        lv_obj_set_style_border_color(video_obj_cont_6,lv_color_hex(0xFFFFFF),LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(video_obj_cont_6,lv_color_hex(0x1E90FF),LV_STATE_FOCUS_KEY);
        lv_obj_align_to(video_obj_cont_6, video_obj_cont_4, LV_ALIGN_OUT_RIGHT_MID, 20, 0);
        lv_obj_set_style_border_width(video_obj_cont_6,2,LV_STATE_DEFAULT);
        lv_obj_set_style_translate_y(video_obj_cont_6, 5, LV_STATE_PRESSED);
        lv_obj_set_style_shadow_color(video_obj_cont_6,lv_color_hex(0x1E90FF),LV_STATE_FOCUS_KEY);
        lv_obj_set_style_shadow_width(video_obj_cont_6,10,LV_STATE_FOCUS_KEY);
        lv_obj_set_style_radius(video_obj_cont_6, 50, 0);                                       /* 设置圆角 */
        lv_obj_add_event_cb(video_obj_cont_6, video_play_event_cb, LV_EVENT_ALL, NULL);

        /* 下一曲标签 */
        video_next_label = lv_label_create(video_obj_cont_6);
        lv_obj_set_style_text_font(video_next_label, &lv_font_montserrat_14, 0);                /* 设置字体 */
        lv_label_set_text(video_next_label, LV_SYMBOL_NEXT);
        lv_obj_set_style_text_color(video_next_label, lv_color_hex(0xffffff), 0);
        lv_obj_align(video_next_label, LV_ALIGN_CENTER, 0, 0);

        lv_group_add_obj(ctrl_g, video_obj_cont_5);
        lv_group_add_obj(ctrl_g, video_obj_cont_4);
        lv_group_add_obj(ctrl_g, video_obj_cont_6);
        lv_group_remove_obj(video_time_slider);
        lv_group_focus_obj(video_obj_cont_5);                               /* 聚焦 */

        if (VIDEOTask_Handler == NULL)
        {
            /* 创建VIDEO任务 */
            xTaskCreatePinnedToCore((TaskFunction_t )video,                 /* 任务函数 */
                                    (const char*    )"video",               /* 任务名称 */
                                    (uint16_t       )VIDEO_STK_SIZE,        /* 任务堆栈大小 */
                                    (void*          )NULL,                  /* 传入给任务函数的参数 */
                                    (UBaseType_t    )VIDEO_PRIO,            /* 任务优先级 */
                                    (TaskHandle_t*  )&VIDEOTask_Handler,    /* 任务句柄 */
                                    (BaseType_t     ) 1);                   /* 该任务哪个内核运行 */
        }
    }
}
