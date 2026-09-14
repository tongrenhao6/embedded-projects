/**
 ****************************************************************************************************
 * @file        lv_pic_demo.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-11-04
 * @brief       相册
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

#include "lv_pic_demo.h"


LV_FONT_DECLARE(myFont12)       /* 声明myFont24字体,这个字体已经烧写到SPIFLASH中 */
FF_DIR picdir;                  /* 图片目录 */
FILINFO *pic_picfileinfo;       /* 文件信息 */
char *pic_pname;                /* 带路径的文件名 */
uint16_t pic_totpicnum;         /* 图片文件总数 */
uint16_t pic_curindex = 0;      /* 图片当前索引 */
uint32_t *pic_picoffsettbl;     /* 图片文件offset索引表 */
uint8_t pic_key = PIC_NULL;
static lv_obj_t *pic_frame;
static lv_obj_t *pic_last_label;
static lv_obj_t *pic_next_label;
static lv_obj_t *pic_name;
extern uint8_t sd_check_en;                 /* sd卡检测标志 */
extern uint8_t decode_en;

/* PIC 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define PIC_PRIO      10                                /* 任务优先级 */
#define PIC_STK_SIZE  5 * 1024                          /* 任务堆栈大小 */
TaskHandle_t          PICTask_Handler;                  /* 任务句柄 */
void pic(void *pvParameters);                           /* 任务函数 */

/**
 * @brief       得到path路径下,目标文件的总个数
 * @param       path : 路径
 * @retval      总有效文件数
 */
uint16_t pic_get_tnum(char *path)
{
    uint8_t res;
    uint16_t rval = 0;
    FF_DIR tdir;                                    /* 临时目录 */
    FILINFO *tfileinfo;                             /* 临时文件信息 */
    tfileinfo = (FILINFO *)malloc(sizeof(FILINFO)); /* 申请内存 */
    res = f_opendir(&tdir, (const TCHAR *)path);    /* 打开目录 */

    if (res == FR_OK && tfileinfo)
    {
        while (1)                                   /* 查询总的有效文件数 */
        {
            res = f_readdir(&tdir, tfileinfo);      /* 读取目录下的一个文件 */

            if (res != FR_OK || tfileinfo->fname[0] == 0)break; /* 错误了/到末尾了,退出 */
            res = exfuns_file_type(tfileinfo->fname);

            if ((res & 0X0F) != 0X00)               /* 取低四位,看看是不是图片文件 */
            {
                rval++;                             /* 有效文件数增加1 */
            }
        }
    }

    free(tfileinfo);                                /* 释放内存 */
    return rval;
}

lv_img_dsc_t img_pic_dsc = {
    .header.always_zero = 0,
    .header.cf = LV_IMG_CF_TRUE_COLOR,
    .data = NULL,
};

/**
 * @brief       PNG/BMPJPEG/JPG解码
 * @param       filename:文件名称
 * @param       width   :宽度
 * @param       height  :高度
 * @retval      无
 */
void lv_pic_png_bmp_jpeg_decode(uint16_t w,uint16_t h,uint8_t * pic_buf)
{
    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
    img_pic_dsc.header.w = w;
    img_pic_dsc.header.h = h;
    img_pic_dsc.data_size = w * h * 2;
    img_pic_dsc.data = (const uint8_t *)pic_buf;
    lv_img_set_src(pic_frame,&img_pic_dsc);
    xSemaphoreGive(xGuiSemaphore);                  /* 释放互斥信号量 */
}

/**
 * @brief       pic
 * @param       pvParameters : 传入参数(未用到)
 * @retval      无
 */
void pic(void *pvParameters)
{
    pvParameters = pvParameters;
    uint8_t res = 0;
    uint16_t temp = 0;

    while(1)
    {
        /* 记录索引 */
        res = f_opendir(&picdir, "0:/PICTURE");                             /* 打开目录 */

        if (res == FR_OK)
        {
            pic_curindex = 0;                                               /* 当前索引为0 */

            while (1)                                                       /* 全部查询一遍 */
            {
                temp = picdir.dptr;                                         /* 记录当前dptr偏移 */
                res = f_readdir(&picdir, pic_picfileinfo);                  /* 读取目录下的一个文件 */
                if (res != FR_OK || pic_picfileinfo->fname[0] == 0)break;   /* 错误了/到末尾了,退出 */

                res = exfuns_file_type(pic_picfileinfo->fname);

                if ((res & 0X0F) != 0X00)                                   /* 取高四位,看看是不是图片文件 */
                {
                    pic_picoffsettbl[pic_curindex] = temp;                  /* 记录索引 */
                    pic_curindex++;
                }
            }
        }

        pic_curindex = 0;                                                   /* 从0开始显示 */
        res = f_opendir(&picdir, (const TCHAR *)"0:/PICTURE");              /* 打开目录 */

        while (res == FR_OK)                                                /* 打开成功 */
        {
            atk_dir_sdi(&picdir, pic_picoffsettbl[pic_curindex]);               /* 改变当前目录索引 */
            res = f_readdir(&picdir, pic_picfileinfo);                      /* 读取目录下的一个文件 */

            if (res != FR_OK || pic_picfileinfo->fname[0] == 0)break;       /* 错误了/到末尾了,退出 */

            strcpy((char *)pic_pname, "0:/PICTURE/");                       /* 复制路径(目录) */
            strcat((char *)pic_pname, (const char *)pic_picfileinfo->fname);/* 将文件名接在后面 */
            /* 文件名传递 */
            temp = exfuns_file_type(pic_pname);                             /* 得到文件的类型 */

            decode_en = ESP_FAIL;                                           /* 准备解码 */

            switch (temp)
            {
                case T_BMP:
                    bmp_decode(pic_pname,lcd_self.width,lcd_self.height,lv_pic_png_bmp_jpeg_decode);    /* 解码BMP */
                    break;
                case T_JPG:
                case T_JPEG:
                    jpeg_decode(pic_pname,lcd_self.width,lcd_self.height,lv_pic_png_bmp_jpeg_decode);   /* 解码JPG/JPEG */
                    break;
                case T_PNG:
                    png_decode(pic_pname,lcd_self.width,lcd_self.height,lv_pic_png_bmp_jpeg_decode);    /* 解码PNG */
                    break;
                default:
                    pic_key = PIC_NEXT;                                                                 /* 非图片格式!!! */
                    break;
            }

            decode_en = ESP_OK;

            lv_label_set_text_fmt(pic_name,"%s",pic_pname);

            while (1)
            {
                xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);

                if (sd_check_en == 0)
                {
                    back_act_key = KEY1_PRES;
                    xSemaphoreGive(xGuiSemaphore);                      /* 释放互斥信号量 */
                }

                if (pic_key == PIC_PREV)                                    /* 上一张 */
                {
                    if (pic_curindex)
                    {
                        pic_curindex--;
                    }
                    else
                    {
                        pic_curindex = pic_totpicnum - 1;
                    }

                    pic_key = PIC_NULL;
                    break;
                }
                else if (pic_key == PIC_NEXT)                               /* 下一张 */
                {
                    pic_curindex++;

                    if (pic_curindex >= pic_totpicnum)
                    {
                        pic_curindex = 0;                                   /* 到末尾的时候,自动从头开始 */
                    }

                    pic_key = PIC_NULL;
                    break;
                }

                xSemaphoreGive(xGuiSemaphore);                              /* 释放互斥信号量 */

                vTaskDelay(10);
            }

            xSemaphoreGive(xGuiSemaphore);                              /* 释放互斥信号量 */
        }
    }
}

/**
  * @brief  删除相册demo
  * @param  无
  * @retval 无
  */
void lv_pic_del(void)
{
    if (PICTask_Handler != NULL)
    {
        vTaskDelete(PICTask_Handler);
        vTaskDelay(10);
    }

    if (pic_picfileinfo || pic_pname || pic_picoffsettbl)           /* 释放内存 */
    {
        free(pic_picfileinfo);                                      /* 释放内存 */
        free(pic_pname);                                            /* 释放内存 */
        free(pic_picoffsettbl);                                     /* 释放内存 */
    }
    decode_en = ESP_OK;
    
    lv_obj_clean(app_obj_general.current_parent);
    lv_obj_del(app_obj_general.current_parent);
    app_obj_general.current_parent = NULL;
    lv_app_show();
    xSemaphoreGive(xGuiSemaphore);                                  /* 释放互斥信号量 */
    PICTask_Handler = NULL;
}

/**
 * @brief  相册播放事件回调
 * @param  *e ：事件相关参数的集合，它包含了该事件的所有数据
 * @return 无
 */
static void pic_play_event_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);      /* 获取触发源 */
    lv_event_code_t code = lv_event_get_code(e);    /* 获取事件类型 */
    
    if (target == pic_last_label)                   /* 下一张 */
    {
        if (code == LV_EVENT_PRESSED)
        {
            lv_obj_set_style_text_color(pic_last_label, lv_color_hex(0x272727), 0);
        }
        else if (code == LV_EVENT_RELEASED)
        {
            pic_key = PIC_PREV;
            lv_obj_set_style_text_color(pic_last_label, lv_color_make(103,116,190), 0);
        }
    }
    else if (target == pic_next_label)              /* 上一张 */
    {
        if (code == LV_EVENT_PRESSED)
        {
            lv_obj_set_style_text_color(pic_next_label, lv_color_hex(0x272727), 0);
        }
        else if (code == LV_EVENT_RELEASED)
        {
            pic_key = PIC_NEXT;
            lv_obj_set_style_text_color(pic_next_label, lv_color_make(103,116,190), 0);
        }
    }
}

/**
 * @brief  相册
 * @param  无
 * @return 无
 */
void lv_pic_demo(void)
{
    pic_key = PIC_NULL;
    pic_frame = NULL;

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
        sd_check_en = 1;

        if (f_opendir(&picdir, "0:/PICTURE"))                       /* 打开音乐文件夹 */
        {
            lv_msgbox("PICTURE folder error");
            return ;
        }
        
        pic_totpicnum = pic_get_tnum("0:/PICTURE");                 /* 得到总有效文件数 */

        if (pic_totpicnum == 0)
        {
            lv_msgbox("No pic files");
            return ;
        }

        pic_picfileinfo = (FILINFO *)malloc(sizeof(FILINFO));       /* 申请内存 */
        pic_pname = malloc(255 * 2 + 1);                            /* 为带路径的文件名分配内存 */
        pic_picoffsettbl = malloc(4 * pic_totpicnum);               /* 申请4*totpicnum个字节的内存,用于存放图片索引 */

        if (!pic_picfileinfo || !pic_pname || !pic_picoffsettbl)    /* 内存分配出错 */
        {
            lv_msgbox("memory allocation failed");
            return ;
        }

        lv_app_del();
        /* 创建该界面的主容器 */
        lv_obj_t *pic_obj = lv_obj_create(lv_scr_act());
        lv_obj_set_size(pic_obj,lv_obj_get_width(lv_scr_act()),lv_obj_get_height(lv_scr_act()));
        lv_obj_set_style_bg_color(pic_obj, lv_color_make(0,0,0), LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(pic_obj,LV_OPA_100,LV_STATE_DEFAULT);
        lv_obj_set_style_border_opa(pic_obj,LV_OPA_0,LV_STATE_DEFAULT);
        lv_obj_set_pos(pic_obj,0,0);
        lv_obj_clear_flag(pic_obj, LV_OBJ_FLAG_SCROLLABLE);
        lv_page_tile(pic_obj,"图片浏览器");                       /* 设置页面标题 */
        
        pic_name = lv_label_create(pic_obj);
        lv_obj_align(pic_name,LV_ALIGN_OUT_TOP_LEFT,0,42);
        lv_obj_set_width(pic_name,lv_obj_get_width(lv_scr_act()));
        lv_obj_set_style_text_font(pic_name,&myFont12,LV_STATE_DEFAULT);
        lv_label_set_long_mode(pic_name,LV_LABEL_LONG_SCROLL_CIRCULAR);
        lv_obj_set_style_text_color(pic_name,lv_color_hex(0xff0000),LV_STATE_DEFAULT);
        lv_label_set_text(pic_name," ");

        pic_frame = lv_img_create(pic_obj);                         /* 创建图像 */
        lv_obj_set_style_bg_color(pic_frame, lv_color_make(50,52,67), LV_STATE_DEFAULT);
        lv_obj_align(pic_frame,LV_ALIGN_CENTER,0,20);               /* 居中对齐 */
        lv_obj_move_background(pic_frame);

        app_obj_general.current_parent = pic_obj;                   /* 指向当前界面容器 */
        app_obj_general.Function = lv_pic_del;                      /* 删除此界面 */

        /* 上一曲标签 */
        pic_last_label = lv_label_create(pic_obj);
        lv_obj_set_style_text_font(pic_last_label, &lv_font_montserrat_28, 0);  /* 设置字体 */
        lv_label_set_text(pic_last_label, LV_SYMBOL_PREV);
        lv_obj_set_style_text_color(pic_last_label,lv_palette_main(LV_PALETTE_RED),LV_STATE_FOCUSED);
        lv_obj_align(pic_last_label,LV_ALIGN_LEFT_MID,10,0);
        lv_obj_set_style_text_color(pic_last_label, lv_color_make(103,116,190), 0);
        lv_obj_add_event_cb(pic_last_label, pic_play_event_cb, LV_EVENT_ALL, NULL);

        /* 下一曲标签 */
        pic_next_label = lv_label_create(pic_obj);
        lv_obj_set_style_text_font(pic_next_label, &lv_font_montserrat_28, 0);  /* 设置字体 */
        lv_obj_set_style_text_color(pic_next_label,lv_palette_main(LV_PALETTE_RED),LV_STATE_FOCUSED);
        lv_label_set_text(pic_next_label, LV_SYMBOL_NEXT);
        lv_obj_align(pic_next_label,LV_ALIGN_RIGHT_MID,-10,0);
        lv_obj_set_style_text_color(pic_next_label, lv_color_make(103,116,190), 0);
        lv_obj_add_event_cb(pic_next_label, pic_play_event_cb, LV_EVENT_ALL, NULL);

        lv_group_add_obj(ctrl_g, pic_last_label);
        lv_group_add_obj(ctrl_g, pic_next_label);
        lv_group_focus_obj(pic_last_label);                                     /* 聚焦 */

        if (PICTask_Handler == NULL)
        {
            /* 创建PIC任务 */
            xTaskCreatePinnedToCore((TaskFunction_t )pic,                 /* 任务函数 */
                                    (const char*    )"pic",               /* 任务名称 */
                                    (uint16_t       )PIC_STK_SIZE,        /* 任务堆栈大小 */
                                    (void*          )NULL,                /* 传入给任务函数的参数 */
                                    (UBaseType_t    )PIC_PRIO,            /* 任务优先级 */
                                    (TaskHandle_t*  )&PICTask_Handler,    /* 任务句柄 */
                                    (BaseType_t     ) 1);                 /* 该任务哪个内核运行 */
        }
    }
}