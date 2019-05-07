#include "autoconf.h"
#include "libc/types.h"
#include "libc/stdio.h"
#include "libc/nostd.h"
#include "libc/string.h"
#include "libc/regutils.h"
#include "libc/syscall.h"

#include "libtft.h"
#include "libspi.h"

#include "font.h"

#include "generated/ili9341.h"

/*
 * This lib requires:
 * BUS   permission
 * DMA   permission
 * TIMER permission
 */

#define UP_CX sys_cfg(CFG_GPIO_SET,(uint8_t)((ili9341_dev_infos.gpios[TFT_CX].port<<4)+ ili9341_dev_infos.gpios[TFT_CX].pin),1);
#define DOWN_CX sys_cfg(CFG_GPIO_SET,(uint8_t)((ili9341_dev_infos.gpios[TFT_CX].port<<4)+ ili9341_dev_infos.gpios[TFT_CX].pin),0);

#define UP_TFT_NSS sys_cfg(CFG_GPIO_SET,(uint8_t)((ili9341_dev_infos.gpios[TFT_NSS].port<<4)+ ili9341_dev_infos.gpios[TFT_NSS].pin),1);
#define DOWN_TFT_NSS sys_cfg(CFG_GPIO_SET,(uint8_t)((ili9341_dev_infos.gpios[TFT_NSS].port<<4)+ ili9341_dev_infos.gpios[TFT_NSS].pin),0);
#define TFT_BUFSIZ 1000


static const char name[4] = "tft";

/* Variables */
static uint8_t nss=0;
static int current_posx=0;
static int current_posy=0;
static uint8_t fg_color[3]={255,255,255};
static uint8_t bg_color[3]={0,0,0};


static void power_up(void)
{
	uint64_t i,ii=0;
     sys_get_systick(&i,PREC_CYCLE);
     for (; (i+1680000)>ii; ) {
         sys_get_systick(&ii,PREC_CYCLE);
     }
}

/*****************************************
 * IRQ Handlers
 *
 * prototype for irq handlers is:
 * static void my_irq_handler(uint8_t irq, // IRQ number
 *                           uint32_t sr   // content of posthook.status,
 *                           uint32_t dr   // content of posthook.data)
 *
 *****************************************/


/*****************************************
 * Initialization functions
 *****************************************/
uint8_t tft_early_init(void)
{
    uint8_t ret = 0;
    device_t dev;
    memset((void*)&dev, 0, sizeof(device_t));
    int devdesc;

    memcpy(dev.name, name, strlen(name));
    dev.gpio_num = 2;
    /* DECLARE TFT CX GPIO_PD14 (NSS is declared in SPI...) */
        dev.gpios[0].mask         = GPIO_MASK_SET_MODE | GPIO_MASK_SET_PUPD | GPIO_MASK_SET_TYPE | GPIO_MASK_SET_SPEED;
        dev.gpios[0].kref.port    = ili9341_dev_infos.gpios[TFT_CX].port;
        dev.gpios[0].kref.pin     = ili9341_dev_infos.gpios[TFT_CX].pin;
        dev.gpios[0].mode         = GPIO_PIN_OUTPUT_MODE;
        dev.gpios[0].pupd         = GPIO_PULLUP;
        dev.gpios[0].type         = GPIO_PIN_OTYPER_OD;
        dev.gpios[0].speed        = GPIO_PIN_VERY_HIGH_SPEED;

    /* DECLARE TFT RESET GPIO_PD15  */
        dev.gpios[1].mask         = GPIO_MASK_SET_MODE | GPIO_MASK_SET_PUPD | GPIO_MASK_SET_TYPE | GPIO_MASK_SET_SPEED;
        dev.gpios[1].kref.port    = ili9341_dev_infos.gpios[TFT_RST].port;
        dev.gpios[1].kref.pin     = ili9341_dev_infos.gpios[TFT_RST].pin;
        dev.gpios[1].mode         = GPIO_PIN_OUTPUT_MODE;
        dev.gpios[1].pupd         = GPIO_PULLUP;
        dev.gpios[1].type         = GPIO_PIN_OTYPER_PP;
        dev.gpios[1].speed        = GPIO_PIN_VERY_HIGH_SPEED;
#ifndef CONFIG_WOOKEY
    /* DECLARE TFT NSS GPIO_PC4  */
        /* DO NOT USE THIS FOR WOOKEY BOARD! */
        dev.gpios[1].mask         = GPIO_MASK_SET_MODE | GPIO_MASK_SET_PUPD | GPIO_MASK_SET_TYPE | GPIO_MASK_SET_SPEED;
        dev.gpios[1].kref.port    = ili9341_dev_infos.gpios[TFT_NSS].port;
        dev.gpios[1].kref.pin     = ili9341_dev_infos.gpios[TFT_NSS].pin;
        dev.gpios[1].mode         = GPIO_PIN_OUTPUT_MODE;
        dev.gpios[1].pupd         = GPIO_PULLUP;
        dev.gpios[1].type         = GPIO_PIN_OTYPER_PP;
        dev.gpios[1].speed        = GPIO_PIN_VERY_HIGH_SPEED;
#endif
    ret = sys_init(INIT_DEVACCESS, &dev, &devdesc);
    return ret;
}

uint8_t tft_init(void)
{

  /* start RESET EVERYTHING */
    sys_cfg(CFG_GPIO_SET,(uint8_t)((ili9341_dev_infos.gpios[TFT_RST].port<<4)+
            ili9341_dev_infos.gpios[TFT_RST].pin),0);
#if CONFIG_WOOKEY_V1
    spi1_init(SPI_BAUDRATE_6MHZ);
#elif CONFIG_WOOKEY_V2
    spi2_init(SPI_BAUDRATE_6MHZ);
#endif
    sys_sleep(10, SLEEP_MODE_DEEP);
#if CONFIG_WOOKEY_V1
    spi1_enable();
#elif CONFIG_WOOKEY_V2
    spi2_enable();
#endif

  /* end RESET EVERYTHING */
    sys_cfg(CFG_GPIO_SET,(uint8_t)((ili9341_dev_infos.gpios[TFT_RST].port<<4)+
            ili9341_dev_infos.gpios[TFT_RST].pin),1);

    sys_sleep(10, SLEEP_MODE_DEEP);
    tft_send_command(0x1);//Soft RESET

    sys_sleep(10, SLEEP_MODE_DEEP);

    tft_send_command(0x28);//Display OFF

    tft_send_command(0xC0);//POWER CONTROL1
    tft_send_param(0x29);/* Adjust the GVDD, the reference level for the VCOM
    *0X23 = 5V
    *0X29 = 3.3v
    */
    tft_send_command(0xC1);//POWER CONTROL2
    tft_send_param(0x03);// STEP UP FACTOR

    tft_send_command(0xC5);//ILI9341_VCOMCONTROL1
    tft_send_param(0x2B); // vcomh => 3.775 = 2b
    // 0C = 3.00v
    tft_send_param(0x2B);//VCOML => -1.435 = 2B
    //

    tft_send_command(0xC7);//ILI9341_VCOMCONTROL2
    tft_send_param(0xC0);

    tft_send_command(0x36);//ILI9341_MEMCONTROL
    tft_send_param(0x80|0x08);//Row adress Ordre on RGB colors

    tft_send_command(0x3A);//ILI9341_PIXELFORMAT
    //tft_send_param(0x55);//0x55 = 16bits
    tft_send_param(0x66);//0x66 = 18bits

    tft_send_command(0x11);//ILI9341_SLEEPOUT
    sys_sleep(10, SLEEP_MODE_DEEP);
    tft_send_command(0x29);//ILI9341_DISPLAYON
    sys_sleep(20, SLEEP_MODE_DEEP);

    return 0;
}
/* Miscelleanous function for send commands and receiveing datas */

int tft_send_command(uint8_t command)
{
  int res;
  /* Wait for any already launched SPI transfert to complete */
#if CONFIG_WOOKEY_V1
  while(spi1_is_busy());
#elif CONFIG_WOOKEY_V2
  while(spi2_is_busy());
#endif
  /*DOWN the D/CX line */
  DOWN_TFT_NSS;
  DOWN_CX;
  /* send the command */
#if CONFIG_WOOKEY_V1
  res=spi1_master_send_byte_sync(command);
#elif CONFIG_WOOKEY_V2
  res=spi2_master_send_byte_sync(command);
#endif
  UP_TFT_NSS;
  return res;
}

int tft_send_param(uint8_t param)
{
  int res;
  /* Wait for any already launched SPI transfert to complete */
#if CONFIG_WOOKEY_V1
  while(spi1_is_busy());
#elif CONFIG_WOOKEY_V2
  while(spi2_is_busy());
#endif
  /*UP the D/CX line */
  DOWN_TFT_NSS;
  UP_CX;
  /* send the parameter */
#if CONFIG_WOOKEY_V1
  res=spi1_master_send_byte_sync(param);
#elif CONFIG_WOOKEY_V2
  res=spi2_master_send_byte_sync(param);
#endif
  UP_TFT_NSS;
  return res;
}

void tft_setxy(int x1, int x2, int y1, int y2)
{
  tft_send_command(0x2A); //column
  tft_send_param((x1>>8)&0xff);
  tft_send_param(x1&0xff);
  tft_send_param((x2>>8)&0xff);
  tft_send_param(x2&0xff);

  tft_send_command(0x2B); //page
  tft_send_param((y1>>8)&0xff);
  tft_send_param(y1&0xff);
  tft_send_param((y2>>8)&0xff);
  tft_send_param(y2&0xff);
}

void tft_fill_rectangle(int x1, int x2, int y1, int y2, uint8_t r, uint8_t g, uint8_t b)
{
//Clear B_CS dans P_CS

        tft_setxy(x1,x2,y1,y2);
//Set B_RS dans P_RS
  tft_send_command(0x2C); //write
 // set_reg_bits(GPIOA_ODR,D_CX_BIT); /* send the parameter */
 DOWN_TFT_NSS;
 UP_CX;
  {
      int i;
      for(i=0;i<(((x2-x1)+1)*((y2-y1)+1));i++)
      {
#if CONFIG_WOOKEY_V1
          spi1_master_send_byte_sync(r);
          spi1_master_send_byte_sync(g);
          spi1_master_send_byte_sync(b);
#elif CONFIG_WOOKEY_V2
          spi2_master_send_byte_sync(r);
          spi2_master_send_byte_sync(g);
          spi2_master_send_byte_sync(b);
#endif
      }
  }
UP_TFT_NSS;
}

void tft_invert_rectangle(int x1,int x2,int y1,int y2)
{
  uint8_t stock[3*TFT_BUFSIZ];
  int i,j;
 // tft_fill_rectangle_unlocked(x1,x2,y1,y2,255,0,0);
  for(j=y1;j<=y2;j++)
 {
    int sup=0;
    for(i=x1;i<=x2;i+=sup)
      {
        int k;
        sup=((i+TFT_BUFSIZ)>x2?x2-i+1:TFT_BUFSIZ);
        tft_setxy(i,i+sup,j,j+1);
        tft_send_command(0x2e);//memory read

        DOWN_TFT_NSS;
        UP_CX;
#if CONFIG_WOOKEY_V1
        spi1_master_send_byte_sync(0);//dummy READ as in spec p116
#elif CONFIG_WOOKEY_V2
        spi2_master_send_byte_sync(0);//dummy READ as in spec p116
#endif
        for(k=0;k<3*sup;k++)
          {
#if CONFIG_WOOKEY_V1
            stock[k]=spi1_master_send_byte_sync(0);//0 is dummy data
#elif CONFIG_WOOKEY_V2
            stock[k]=spi2_master_send_byte_sync(0);//0 is dummy data
#endif

          }
        tft_setxy(i,i+sup,j,j+1);
        tft_send_command(0x2c);//memory write
        DOWN_TFT_NSS;
        UP_CX;
        for(k=0;k<3*sup;k++)
#if CONFIG_WOOKEY_V1
          spi1_master_send_byte_sync((stock[k])^255);//Ensure the 3 lsb are 0
#elif CONFIG_WOOKEY_V2
          spi2_master_send_byte_sync((stock[k])^255);//Ensure the 3 lsb are 0
#endif
        UP_TFT_NSS;
        }
  }
}

void tft_send_image(int x1,int x2, int y1,int y2, uint8_t *data)
{
  int i;
  tft_setxy(x1,x2,y1,y2);
  tft_send_command(0x2c); /*Memory write */
  //set_reg_bits(GPIOA_ODR,D_CX_BIT); /* send the parameter */
  DOWN_TFT_NSS;
  UP_CX;
  for(i=0;i<(((x2-x1)+1)*((y2-y1)+1));i++)
    {
#if CONFIG_WOOKEY_V1
      spi1_master_send_byte_sync(data[3*i]);
      spi1_master_send_byte_sync(data[3*i+1]);
      spi1_master_send_byte_sync(data[3*i+2]);
#elif CONFIG_WOOKEY_V2
      spi2_master_send_byte_sync(data[3*i]);
      spi2_master_send_byte_sync(data[3*i+1]);
      spi2_master_send_byte_sync(data[3*i+2]);
#endif
    }
  UP_TFT_NSS;
}

void tft_set_cursor_pos(int x,int y)
{
  current_posx=x;
  current_posy=y;
}



void tft_putc(char c)
{
        /* The font file is organized like ASCCI 7bit on One line */
        int char_height=(font_height/2);
        int char_width=((font_width)/128);
        int start_pos_px,first_byte,offset_bit;
        int start_line;
        int nlines;
        /*
        * Sizes are in pixel
        */
        int i=0,j;
        start_pos_px=char_width*c;
        start_line=0;
        nlines=char_height+font_blankskip/2;

        first_byte=start_pos_px/8;
        offset_bit=start_pos_px&7;
        tft_setxy(current_posx,
            (current_posx+char_width>240?240:current_posx+char_width),
            current_posy,
            (current_posy+char_height>320?320:current_posy+char_height));
        //big box not obligatory fully fiiled depending on characters
        tft_send_command(0x2c);
        UP_CX;
        DOWN_TFT_NSS;
        //char_width need not be a multiple of 8 !!!
        //font_width should however
        //Le compute the start position in pixel in the line

        for(i=start_line+font_blankskip;i<(start_line+nlines);i++)
        {
          volatile int star_pos=i*(font_width/8)+first_byte;
          volatile int myoffset=offset_bit;
          volatile int size_printed=0;

          for(j=0; size_printed<=char_width; j++) {
            volatile int k;
           for(k=myoffset;(k<8) && (size_printed<=char_width);k++)
            {
              if(font[star_pos+j]&(1<<k))
              {
#if CONFIG_WOOKEY_V1
                spi1_master_send_byte_sync(fg_color[0]);
                spi1_master_send_byte_sync(fg_color[1]);
                spi1_master_send_byte_sync(fg_color[2]);
#elif CONFIG_WOOKEY_V2
                spi2_master_send_byte_sync(fg_color[0]);
                spi2_master_send_byte_sync(fg_color[1]);
                spi2_master_send_byte_sync(fg_color[2]);
#endif
              }
              else
              {
#if CONFIG_WOOKEY_V1
                spi1_master_send_byte_sync(bg_color[0]);
                spi1_master_send_byte_sync(bg_color[1]);
                spi1_master_send_byte_sync(bg_color[2]);
#else
                spi2_master_send_byte_sync(bg_color[0]);
                spi2_master_send_byte_sync(bg_color[1]);
                spi2_master_send_byte_sync(bg_color[2]);
#endif
              }
              size_printed++;
            }
            myoffset=0;
          }
        }
        UP_TFT_NSS;

        //current_posy+=char_width+char_height;
        current_posx+=char_width;

}

static bool tft_is_printable(char c)
{
    if (c> 31 && c < 127) {
        return true;
    }
    return false;
}

int tft_puts(const char *s)
{
  const char *tmp=s;
  while(*s) {
      if (tft_is_printable(*s)) {
        tft_putc(*(s));
      }
      s++;
  }
  return s-tmp;
}

void tft_setfg(uint8_t r, uint8_t g, uint8_t b)
{
  fg_color[0]=r;
  fg_color[1]=g;
  fg_color[2]=b;
}
void tft_setbg(uint8_t r, uint8_t g, uint8_t b)
{
  bg_color[0]=r;
  bg_color[1]=g;
  bg_color[2]=b;
}
void tft_rle_image(int x, int y,int width, int height, const uint8_t *colormap,
                   const uint8_t *data, int datalen)
{
   int i;
  tft_setxy(x,x+width - 1,y,y+height - 1);
  tft_send_command(0x2c); /* Meomory Write */
  UP_CX;
  DOWN_TFT_NSS;
  for(i=0;i<datalen;i+=2)
  {
        int nb;
        for(nb=0;nb<data[i+1];nb++)
        {
                /* the two least significant bits are ignored */
#if CONFIG_WOOKEY_V1
                spi1_master_send_byte_sync(colormap[3*data[i]]&~3);
                spi1_master_send_byte_sync(colormap[3*data[i]+1]&~3);
                spi1_master_send_byte_sync(colormap[3*data[i]+2]&~3);
#else
                spi2_master_send_byte_sync(colormap[3*data[i]]&~3);
                spi2_master_send_byte_sync(colormap[3*data[i]+1]&~3);
                spi2_master_send_byte_sync(colormap[3*data[i]+2]&~3);
#endif
        }
  }
  UP_TFT_NSS;
}


/* other functions  (low level commands execution) */

void screen_save_nss_status()
{
  int ret;
  ret=sys_cfg(CFG_GPIO_GET,(uint8_t)((ili9341_dev_infos.gpios[TFT_NSS].port<<4) +ili9341_dev_infos.gpios[TFT_NSS].pin),&nss);
  if (ret != SYS_E_DONE) {
    aprintf("unable to read TFT NSS pin value : %x\n", strerror(ret));
  }
  //UP_TFT_NSS;
}
void screen_restore_nss_status()
{
  if(!nss)
  {
    DOWN_TFT_NSS;
  }
  else
  {
    UP_TFT_NSS;
  }
}



