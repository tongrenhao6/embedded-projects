/**
 ****************************************************************************************************
 * @file        app_ui.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2023-08-01
 * @brief       ESP32-S3综合例程UI
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 ESP32-S3 开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobaco.com
 * 
 ****************************************************************************************************
 */

#include "app_ui.h"
#include "sdmmc_cmd.h"
#include "ph_time.h"

extern lv_indev_t *indev_keypad;                            /* 按键组 */
app_obj_t app_obj_btn;
lv_m_general app_obj_general;
lv_group_t *ctrl_g;
extern uint8_t sd_check_en;                 /* sd卡检测标志 */
LV_FONT_DECLARE(myFont12)

/* app图标信息默认配置 */
const app_image_info app_image[] =
{
    {"camera","摄像头",&app_obj_btn.image_bin[0]},
    {"file","文件",&app_obj_btn.image_bin[1]},
    {"video","视频",&app_obj_btn.image_bin[2]},
    {"setting","设置",&app_obj_btn.image_bin[3]},
    {"data","数据管理",&app_obj_btn.image_bin[4]},
    {"measure","数据",&app_obj_btn.image_bin[5]},
    {"photo","相册",&app_obj_btn.image_bin[6]},
    {"music","音乐",&app_obj_btn.image_bin[7]},
    {"calendar","日历",&app_obj_btn.image_bin[8]},
};

/* 获取路径的个数 */
#define image_mun (int)(sizeof(app_image)/sizeof(app_image[0]))

/* app就绪表 */
unsigned int  app_readly_list[32];

/* app触发位 */
int lv_trigger_bit = 0;
uint8_t *image_buffer[APP_NUM];
uint32_t at_cpu;
static uint8_t load_index = 0;

/**
 * @brief       转换
 * @param       fs:文件系统对象
 * @param       clst:转换
 * @retval      =0:扇区号，0:失败
 */
static LBA_t atk_clst2sect(FATFS *fs, DWORD clst)
{
    clst -= 2;  /* Cluster number is origin from 2 */

    if (clst >= fs->n_fatent - 2)
    {
        return 0;   /* Is it invalid cluster number? */
    }

    return fs->database + (LBA_t)fs->csize * clst;  /* Start sector number of the cluster */
}

/**
 * @brief       偏移
 * @param       dp:指向目录对象
 * @param       Offset:目录表的偏移量
 * @retval      FR_OK(0):成功，!=0:错误
 */
FRESULT atk_dir_sdi(FF_DIR *dp, DWORD ofs)
{
    DWORD clst;
    FATFS *fs = dp->obj.fs;

    if (ofs >= (DWORD)((FF_FS_EXFAT && fs->fs_type == FS_EXFAT) ? 0x10000000 : 0x200000) || ofs % 32)
    {
        /* 检查偏移是否有效 */
        return FR_INT_ERR;
    }

    dp->dptr = ofs;         /* Set current offset */
    clst = dp->obj.sclust;  /* Table start cluster (0:root) */

    if (clst == 0 && fs->fs_type >= FS_FAT32)
    {	/* Replace cluster# 0 with root cluster# */
        clst = (DWORD)fs->dirbase;

        if (FF_FS_EXFAT)
        {
            dp->obj.stat = 0;
        }
        /* exFAT: Root dir has an FAT chain */
    }

    if (clst == 0)
    {	/* Static table (root-directory on the FAT volume) */
        if (ofs / 32 >= fs->n_rootdir)
        {
            return FR_INT_ERR;  /* Is index out of range? */
        }

        dp->sect = fs->dirbase;

    }
    else
    {   /* Dynamic table (sub-directory or root-directory on the FAT32/exFAT volume) */
        dp->sect = atk_clst2sect(fs, clst);
    }

    dp->clust = clst;   /* Current cluster# */

    if (dp->sect == 0)
    {
        return FR_INT_ERR;
    }

    dp->sect += ofs / fs->ssize;             /* Sector# of the directory entry */
    dp->dir = fs->win + (ofs % fs->ssize);   /* Pointer to the entry in the win[] */

    return FR_OK;
}

/**
 * @brief       从SPI FLASH获取图片(BMP,JEG,PNG)
 * @param       image-存储图片信息的指针
 * @retval      0-加载成功 非 0-加载失败
 */
uint8_t lv_load_img(lv_img_dsc_t *image,uint32_t addr,uint32_t size)
{
    
    uint8_t err = 1;
    uint32_t image_header = 0;
    uint8_t *image_jpeg = NULL;

    load_index ++;
    /* 申请图片内存,图片需要一直显示,内存不用释放 */
    image_buffer[load_index - 1] = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    
    if (image_buffer[load_index - 1] != NULL)
    {
        err = 0;
    }
    
    err = images_partition_read(image_buffer[load_index - 1],addr,size);
  
    /* 获取头 */
    image_header = (uint32_t)image_buffer[load_index - 1][3] << 24;
    image_header |= (uint32_t)image_buffer[load_index - 1][2] << 16;
    image_header |= (uint32_t)image_buffer[load_index - 1][1] << 8;
    image_header |= (uint32_t)image_buffer[load_index - 1][0];
    
    /* 获取图片数据流 */
    image_jpeg = (uint8_t *)image_buffer[load_index - 1] + 4;
    
    image->header.cf = image_buffer[load_index - 1][0];
    image->header.always_zero = 0;
    image->header.w = (uint16_t)(image_header >> 10);
    image->header.h = (uint16_t)(image_header >> 21);
    image->data_size = size - 4;                        /* 去掉4字节的头,剩余的就是图片数据流长度 */
    image->data = image_jpeg;
    
    return err;
}

/**
 * @brief       消息提示删除
 * @param       无
 * @retval      无
 */
void lv_msgbox_del(void)
{
    lv_obj_del(app_obj_general.current_parent);
    app_obj_general.current_parent = NULL;
}

/**
 * @brief       消息提示
 * @param       name:消息内容
 * @retval      无
 */
void lv_msgbox(char *name)
{
    /* 消息框整体 */
    lv_obj_t *msgbox = lv_msgbox_create(lv_scr_act(),"提示",name, NULL,false);
    lv_obj_set_size(msgbox, lcd_self.width - 20, lcd_self.height / 3);   /* 设置大小 */
    lv_obj_center(msgbox); /* 设置位置 */
    lv_obj_set_style_border_width(msgbox, 0, 0);                        /* 去除边框 */
    lv_obj_set_style_shadow_width(msgbox, 20, 0);                       /* 设置阴影宽度 */
    lv_obj_set_style_shadow_color(msgbox, lv_color_hex(0xa9a9a9),LV_STATE_DEFAULT); /* 设置阴影颜色 */
    lv_obj_set_style_pad_top(msgbox,18,LV_STATE_DEFAULT);               /* 设置顶部填充 */
    lv_obj_set_style_pad_left(msgbox,20,LV_STATE_DEFAULT);              /* 设置左侧填充 */
    /* 消息框标题 */
    lv_obj_t *title = lv_msgbox_get_title(msgbox);                      /* 获取标题部分 */
    lv_obj_set_style_text_font(title, &myFont12,LV_STATE_DEFAULT);                  /* 设置字体 */
    lv_obj_set_style_text_color(title, lv_color_hex(0xff0000),LV_STATE_DEFAULT);    /* 设置文本颜色：红色 */
    /* 消息框主体 */
    lv_obj_t *content = lv_msgbox_get_content(msgbox);                  /* 获取主体部分 */
    lv_obj_set_style_text_font(content, &myFont12,LV_STATE_DEFAULT);                /* 设置字体 */
    lv_obj_set_style_text_color(content, lv_color_hex(0x6c6c6c),LV_STATE_DEFAULT);  /* 设置文本颜色：灰色 */
    lv_obj_set_style_pad_top(content,15,LV_STATE_DEFAULT);              /* 设置顶部填充 */
    app_obj_general.current_parent = msgbox;                            /* 指向当前界面容器 */
    app_obj_general.Function = lv_msgbox_del;                           /* 删除此界面 */
}

/**
 * @brief       加载图标
 * @param       无
 * @retval      无
 */
void lv_load_iamge(void)
{
    /* 遍历图标库 */
    for (int i = 0 ; i < APP_NUM ;i++)
    {
        memset(&app_obj_btn.image_bin[i],0,sizeof(lv_img_dsc_t));
        /* 根据地址偏移遍历图标库 */
        app_obj_btn.image_flag = lv_load_img(&app_obj_btn.image_bin[i],*(uint32_t*)(&g_ftinfo.lvgl_camera_addr + 2 * i),g_ftinfo.lvgl_camera_size);
        
        if (app_obj_btn.image_flag != 0)
        {
            printf("加载图标失败\r\n");
        }
    }

    load_index = 0;
}

/**
  * @brief  计算前导置零
  * @param  app_readly_list：就绪表
  * @retval 返回就绪位
  */
int lv_clz(unsigned int  app_readly_list[])
{
    int bit = 0;

    for (int i = 0; i < 32; i++)
    {
        if (app_readly_list[i] == 1)
        {
            break;
        }

        bit ++ ;
    }

    return bit;
}

lv_obj_t *status_bar_left;
lv_obj_t *usb_status_left;
lv_obj_t *sd_status_left;
lv_obj_t *cpu_status;

/**
  * @brief  APP按键回调函数
  * @param  obj  :对象
  * @param  event:事件
  * @retval 无
  */
static void lv_imgbtn_control_event_handler(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    lv_obj_t * obj = lv_event_get_target(event);

    /* 判断图标是否按下 */
    if (code == LV_EVENT_CLICKED)
    {

        for (int i = 0;i < image_mun;i ++)
        {
            if (obj == app_obj_btn.app_btn[i])
            {
                app_readly_list[i] = 1 ;                                        /* app就绪表位置1 */
                
            }
        }

        app_obj_general.fouc_parent = obj;

        lv_trigger_bit = ((unsigned int)lv_clz((app_readly_list)));             /* 计算前导指令 */
        app_readly_list[lv_trigger_bit] = 0;                                    /* 该位清零就绪表 */

        switch(lv_trigger_bit)                                                  /* 根据该位做相应的函数 */
        {
            case 0: /* 摄像头 */

                if (app_obj_general.current_parent != NULL)
                {
                    lv_obj_del(app_obj_general.current_parent);
                    app_obj_general.current_parent = NULL;
                }
                lv_msgbox("disabled");
                break;
            
            case 1: /* 文件系统 */
                lv_file_demo();
                break;
            
            case 2: /* 视频播放器 */
                if (app_obj_general.current_parent != NULL)
                {
                    lv_obj_del(app_obj_general.current_parent);
                    app_obj_general.current_parent = NULL;
                }
                lv_msgbox("disabled");
                break;
                
            case 3: /* 设置 */
                /* 解决重复按下 */
                if (app_obj_general.current_parent != NULL)
                {
                    lv_obj_del(app_obj_general.current_parent);
                    app_obj_general.current_parent = NULL;
                }
                lv_msgbox("unrealized!!!");
                break;

            case 4: /* 数据管理 */
                lv_data_demo();
                break;
            
            case 5: /* 测量 */
                lv_measure_demo();
                break;
            
            case 6: /* 相册 */
                if (app_obj_general.current_parent != NULL)
                {
                    lv_obj_del(app_obj_general.current_parent);
                    app_obj_general.current_parent = NULL;
                }
                lv_msgbox("disabled");
                break;
            
            case 7: /* 音乐 */
                if (app_obj_general.current_parent != NULL)
                {
                    lv_obj_del(app_obj_general.current_parent);
                    app_obj_general.current_parent = NULL;
                }
                lv_msgbox("disabled");
                break;

            case 8: /* 时钟 */
                lv_clock_demo();
                break;
            default:
                break;
        }
    }
}

/**
  * @brief  申请APP图标内存和设置图标
  * @param  无
  * @retval 无
  */
void lv_app_show(void)
{
    /* 重新加载图标 */
    lv_load_iamge();

    /* 设置图标 */
    for (int i = 0;i < APP_NUM;i++)
    {
        lv_imgbtn_set_src(app_obj_btn.app_btn[i], LV_IMGBTN_STATE_RELEASED, NULL, &app_obj_btn.image_bin[i], NULL);
        lv_timer_handler();
    }

    /* 显示主界面 */
    lv_obj_clear_flag(app_obj_btn.lv_main_cont,LV_OBJ_FLAG_HIDDEN);
    lv_obj_update_layout(app_obj_btn.lv_main_cont);                 /* 更新层次 */
    lv_group_focus_obj(app_obj_general.fouc_parent);                /* 聚焦当前APP */
    app_obj_general.fouc_parent = NULL;                             /* 聚焦当前APP */
}

/**
  * @brief  释放APP图标内存
  * @param  无
  * @retval 无
  */
void lv_app_del(void)
{
    for (int i = 0;i < APP_NUM;i++)
    {
        heap_caps_free(image_buffer[i]);
        lv_imgbtn_set_src(app_obj_btn.app_btn[i], LV_IMGBTN_STATE_RELEASED, NULL, NULL, NULL);
        lv_timer_handler();
        image_buffer[i] = NULL;
    }
    
    lv_obj_add_flag(app_obj_btn.lv_main_cont,LV_OBJ_FLAG_HIDDEN);
    lv_obj_update_layout(app_obj_btn.lv_main_cont);                 /* 更新层次 */
}

/**
  * @brief  APP显示
  * @param  parent:父类对象
  * @retval 无
  */
void lv_mid_cont_add_app(lv_obj_t *parent)
{
    int lv_index = 0;
    int line_feed_num = 0;
    int n = 0;

    ctrl_g = lv_group_create();
    /* 创建按键组，用来控制 */
    lv_group_set_default(ctrl_g);
    lv_indev_set_group(indev_keypad, ctrl_g);

    /* 以下是图标排版代码 */
    app_obj_btn.app_btn[lv_index] = lv_imgbtn_create(parent);
    lv_imgbtn_set_src(app_obj_btn.app_btn[lv_index], LV_IMGBTN_STATE_RELEASED, NULL, &app_obj_btn.image_bin[lv_index], NULL);
    lv_obj_set_size(app_obj_btn.app_btn[lv_index], app_obj_btn.image_bin[lv_index].header.w, app_obj_btn.image_bin[lv_index].header.h);
    lv_obj_set_pos(app_obj_btn.app_btn[lv_index], 15, 25);
    lv_obj_set_style_bg_opa(app_obj_btn.app_btn[lv_index],LV_OPA_50,LV_STATE_FOCUSED);
    unsigned int lv_width_x = app_obj_btn.image_bin[lv_index].header.w + 20;

    lv_group_add_obj(ctrl_g, app_obj_btn.app_btn[lv_index]);
    /* 第一个图标添加回调函数 */
    lv_obj_add_event_cb(app_obj_btn.app_btn[lv_index], lv_imgbtn_control_event_handler, LV_EVENT_ALL, NULL);
    
    app_obj_btn.app_name[lv_index] = lv_label_create(parent);
    lv_obj_set_style_text_color(app_obj_btn.app_name[lv_index],lv_color_hex(0xf0f0f0), LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(app_obj_btn.app_name[lv_index], &myFont12, 0);
    lv_label_set_text(app_obj_btn.app_name[lv_index], app_image[lv_index].app_text_Chinese);
    lv_obj_align_to(app_obj_btn.app_name[lv_index],app_obj_btn.app_btn[lv_index],LV_ALIGN_OUT_BOTTOM_MID,0, 0);
    lv_obj_set_style_img_recolor_opa(app_obj_btn.app_btn[lv_index], LV_OPA_30, LV_STATE_PRESSED);
    lv_obj_set_style_img_recolor(app_obj_btn.app_btn[lv_index], lv_color_black(), LV_STATE_PRESSED);
    
    for (lv_index = 1 ; lv_index < APP_NUM; lv_index ++)
    {
        app_obj_btn.app_btn[lv_index] = lv_imgbtn_create(parent);
        lv_imgbtn_set_src(app_obj_btn.app_btn[lv_index], LV_IMGBTN_STATE_RELEASED, NULL, &app_obj_btn.image_bin[lv_index], NULL);
        lv_obj_set_size(app_obj_btn.app_btn[lv_index], app_obj_btn.image_bin[lv_index].header.w, app_obj_btn.image_bin[lv_index].header.h);
        lv_obj_set_style_img_recolor_opa(app_obj_btn.app_btn[lv_index], LV_OPA_30, LV_STATE_PRESSED);
        lv_obj_set_style_img_recolor(app_obj_btn.app_btn[lv_index], lv_color_black(), LV_STATE_PRESSED);
        lv_obj_set_style_size(app_obj_btn.app_btn[lv_index], app_obj_btn.image_bin[lv_index].header.w - 2,LV_STATE_PRESSED);
        lv_width_x = lv_width_x + app_obj_btn.image_bin[lv_index].header.w + 10;
        lv_obj_set_style_bg_opa(app_obj_btn.app_btn[lv_index],LV_OPA_50,LV_STATE_FOCUSED);
        lv_group_add_obj(ctrl_g, app_obj_btn.app_btn[lv_index]);

        if (lv_width_x < lv_obj_get_width(lv_scr_act()))
        {
            lv_obj_align_to(app_obj_btn.app_btn[lv_index], app_obj_btn.app_btn[lv_index - 1], LV_ALIGN_OUT_RIGHT_MID, 10, 0);
        }
        else
        {
            line_feed_num++;

            if (line_feed_num >= 2)
            {
                n = app_obj_btn.image_bin[lv_index].header.h + 5;
            }
            
            lv_obj_set_pos(app_obj_btn.app_btn[lv_index], 15, (app_obj_btn.image_bin[lv_index].header.h * (lv_index)) - n - 80);
            lv_width_x = app_obj_btn.image_bin[lv_index].header.w + 10;
        }
        
        /* 剩下的图标添加回调函数 */
        lv_obj_add_event_cb(app_obj_btn.app_btn[lv_index], lv_imgbtn_control_event_handler, LV_EVENT_ALL, NULL);
        
        app_obj_btn.app_name[lv_index] = lv_label_create(parent);
        lv_obj_set_style_text_color(app_obj_btn.app_name[lv_index],lv_color_hex(0xf0f0f0), LV_STATE_DEFAULT);
        
        if(lv_index < APP_NUM - 3)           /* 前面的图标需要文字 */
        {
            lv_obj_set_style_text_font(app_obj_btn.app_name[lv_index], &myFont12, 0);
            lv_label_set_text(app_obj_btn.app_name[lv_index], app_image[lv_index].app_text_Chinese);
        }
        else                        /* 下方的4个图标不需要文字 */
        {
            lv_label_set_text(app_obj_btn.app_name[lv_index], " ");
        }
        
        lv_obj_align_to(app_obj_btn.app_name[lv_index],app_obj_btn.app_btn[lv_index],LV_ALIGN_OUT_BOTTOM_MID,0, 0);
    }
}

/**
  * @brief  获取RTC时间信息
  * @param  无
  * @retval 无
  */
static void lv_rtc_timer(lv_timer_t* timer)
{
    bool time_valid = ph_time_is_valid();

    /* 获取RTC时间信息 */
    if (time_valid)
    {
        rtc_get_time();
        app_obj_btn.rtc.year = calendar.year; 
        app_obj_btn.rtc.month = calendar.month; 
        app_obj_btn.rtc.date = calendar.date; 
        app_obj_btn.rtc.hour = calendar.hour; 
        app_obj_btn.rtc.minute = calendar.min; 
        app_obj_btn.rtc.second = calendar.sec; 
        app_obj_btn.rtc.week = calendar.week; 

        lv_label_set_text_fmt(status_bar_left,"%02d : %02d : %02d", app_obj_btn.rtc.hour, app_obj_btn.rtc.minute,app_obj_btn.rtc.second);
    }
    else
    {
        lv_label_set_text(status_bar_left, "-- : -- : --");
    }

    lv_label_set_text_fmt(cpu_status, "%"LV_PRIu32"%% CPU", at_cpu);

    /* 检测SD卡是否插入 */
    if (sd_check_en == 1)
    {
        lv_obj_set_style_text_color(sd_status_left,lv_color_make(255,255,255), LV_STATE_DEFAULT);

        if (sdmmc_get_status(card) != ESP_OK)
        {
            sd_check_en = 0;
        }
    }
    else
    {
        lv_obj_set_style_text_color(sd_status_left,lv_color_hex(0x292421), LV_STATE_DEFAULT);

        if (sd_spi_init() == ESP_OK)
        {
            if (sdmmc_get_status(card) == ESP_OK)
            {
                /* 解决重复按下 */
                if (app_obj_general.current_parent != NULL)
                {
                    lv_obj_del(app_obj_general.current_parent);
                    app_obj_general.current_parent = NULL;
                }
                
                sd_check_en = 1;
            }
        }
    }
}

/**
  * @brief  加载顶行图标
  * @param  无
  * @retval 无
  */
void lv_app_icon(lv_obj_t *praten)
{
    /* 左侧状态栏 */
    status_bar_left = lv_label_create(praten);                                                  /* 创建标签 */
    lv_label_set_text(status_bar_left, " " );                                                   /* 设置文本内容 */
    lv_obj_set_style_text_font(status_bar_left, &lv_font_montserrat_14, LV_STATE_DEFAULT);      /* 设置字体 */
    lv_obj_align(status_bar_left, LV_ALIGN_TOP_RIGHT, -10, 0);                                  /* 设置位置 */
    lv_obj_set_style_text_color(status_bar_left, lv_color_hex(0xf0f0f0), LV_STATE_DEFAULT);

    cpu_status = lv_label_create(praten);                                             /* 创建标签 */
    lv_label_set_text(cpu_status, " ");                                                         /* 设置文本内容 */
    lv_obj_set_style_text_font(cpu_status, &lv_font_montserrat_14, LV_STATE_DEFAULT);           /* 设置字体 */
    lv_obj_align(cpu_status, LV_ALIGN_CENTER, 0, 0);                                          /* 设置位置 */
    lv_obj_set_style_text_color(cpu_status, lv_color_hex(0xf0f0f0), LV_STATE_DEFAULT);


    usb_status_left = lv_label_create(praten);                                                  /* 创建标签 */
    lv_label_set_text(usb_status_left, LV_SYMBOL_USB);                                          /* 设置文本内容 */
    lv_obj_set_style_text_font(usb_status_left, &lv_font_montserrat_14, LV_STATE_DEFAULT);      /* 设置字体 */
    lv_obj_align(usb_status_left, LV_ALIGN_TOP_LEFT, 10, 0);                                    /* 设置位置 */
    lv_obj_set_style_bg_color(usb_status_left,lv_color_hex(0xC0C0C0), LV_STATE_DEFAULT);

    sd_status_left = lv_label_create(praten);                                                   /* 创建标签 */
    lv_label_set_text(sd_status_left, LV_SYMBOL_SD_CARD);                                       /* 设置文本内容 */
    lv_obj_set_style_text_font(sd_status_left, &lv_font_montserrat_14, LV_STATE_DEFAULT);       /* 设置字体 */
    lv_obj_align_to(sd_status_left, usb_status_left,LV_ALIGN_OUT_RIGHT_MID, 10, 0);              /* 设置位置 */
    lv_obj_set_style_bg_color(sd_status_left,lv_color_hex(0xC0C0C0), LV_STATE_DEFAULT);

    /* 刷新RTC */
    app_obj_btn.rtc.lv_rtc_timer = lv_timer_create(lv_rtc_timer, 500, NULL);
}

/**
  * @brief  加载主界面
  * @param  无
  * @retval 无
  */
void lv_load_main_window(void)
{
    app_obj_general.del_parent = NULL;
    app_obj_general.current_parent = NULL;
    app_obj_general.fouc_parent = NULL;
    app_obj_general.Function = NULL;

    memset(&app_obj_btn,0,sizeof(app_obj_btn));
    
    /* SPI flash加载图标 */
    lv_load_iamge();
    lv_obj_update_layout(lv_scr_act());                 /* 更新层次 */
    
    /* 新建主界面容器 */
    app_obj_btn.lv_main_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_style_bg_color(app_obj_btn.lv_main_cont, lv_color_make(0,0,0), LV_STATE_DEFAULT);
    lv_obj_set_size(app_obj_btn.lv_main_cont, lcd_self.width, lcd_self.height);
    lv_obj_set_style_radius(app_obj_btn.lv_main_cont, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(app_obj_btn.lv_main_cont, LV_OPA_0, LV_STATE_DEFAULT);
    lv_obj_set_pos(app_obj_btn.lv_main_cont, 0, 0);
    /* 禁止滑动 */
    lv_obj_clear_flag(app_obj_btn.lv_main_cont, LV_OBJ_FLAG_SCROLLABLE);
    /* 禁止滑动 */
    lv_obj_clear_flag(lv_scr_act(), LV_OBJ_FLAG_SCROLLABLE);

    /* 主界面下方容器 */
    lv_obj_t *down_con = lv_obj_create(app_obj_btn.lv_main_cont);
    lv_obj_set_size(down_con, lcd_self.width - 10, lcd_self.height / 4);
    lv_obj_align(down_con, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_bg_color(down_con, lv_color_hex(0xFAF0E6), 0);
    lv_obj_set_style_bg_opa(down_con, 150, 0);
    lv_obj_set_style_border_opa(down_con, 0, 0);
    lv_obj_set_style_radius(down_con, 20, 0);
    
    lv_obj_t * up_con = lv_obj_create(lv_layer_sys());
    lv_obj_set_style_bg_color(up_con,lv_color_hex(0x000000),LV_STATE_DEFAULT);
    lv_obj_set_pos(up_con, 0, 0);
    lv_obj_move_foreground(up_con);
    lv_obj_set_style_radius(up_con, 0, LV_STATE_DEFAULT);
    lv_obj_set_size(up_con,lv_obj_get_width(lv_scr_act()),lv_font_montserrat_14.line_height + 5);
    lv_obj_set_style_border_opa(up_con,LV_OPA_0,LV_STATE_DEFAULT);
    /* 加载图标 */
    lv_app_icon(up_con);
    
    /* 加载顶部控件 */
    lv_mid_cont_add_app(app_obj_btn.lv_main_cont);

}
