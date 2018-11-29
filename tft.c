#include "api/types.h"
#include "kernel/exported/devices.h"
#include "kernel/exported/dmas.h"
#include "api/libtft.h"
#include "api/libspi.h"
#include "api/print.h"
#include "api/regutils.h"
#include "font.h"
//#include "tft.h"
#include "api/syscall.h"


/*
 * This lib requires:
 * BUS   permission
 * DMA   permission
 * TIMER permission
 */
static const char name[4] = "tft";

/* Variables */
static uint8_t nss=0;
static int current_posx=0;
static int current_posy=0;
static uint8_t fg_color[3]={255,255,255};
static uint8_t bg_color[3]={0,0,0};


/***********************************************
 * SDIO block startup function
 ***********************************************/

static void power_up(void)
{
	uint64_t i,ii=0;
//	set_reg(r_CORTEX_M_SDIO_POWER, SDIO_POWER_PWRCTRL_POWER_ON, SDIO_POWER_PWRCTRL);
    /*
     * SDIO_POWER documentation says:
     * At least seven HCLK clock periods are needed between two write accesses to this register.
     * After a data write, data cannot be written to this register for three SDIOCLK (48 MHz) clock
     * periods plus two PCLK2 clock periods.
     */
     sys_get_systick(&i,PREC_CYCLE);
     printf("debut %x\n",(uint32_t)i);
     for (; (i+1680000)>ii; ) {
         sys_get_systick(&ii,PREC_CYCLE);
     }
     printf("fin %x\n",(uint32_t)ii);
	//LOG("Power up !\n");
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
    /*******************************
     * first, SDIO device declaration
     *******************************/
    device_t dev;
    memset((void*)&dev, 0, sizeof(device_t));
    int devdesc;

    /*
     * declare the SDIO device, during initialization phase
     * This function create a device_t, fullfill it, and execute a
     * sys_init(INIT_DEVACCESS) syscall.
     */
    memcpy(dev.name, name, strlen(name));
    dev.gpio_num = 2;
#if 1
    /* DECLARE TFT CX GPIO_PD14 (NSS is declared in SPI...) */
        dev.gpios[0].mask         = GPIO_MASK_SET_MODE | GPIO_MASK_SET_PUPD | GPIO_MASK_SET_TYPE | GPIO_MASK_SET_SPEED;
        dev.gpios[0].kref.port    = GPIO_PD;
        dev.gpios[0].kref.pin     = 14;
        dev.gpios[0].mode         = GPIO_PIN_OUTPUT_MODE;
        dev.gpios[0].pupd         = GPIO_PULLUP;
        dev.gpios[0].type         = GPIO_PIN_OTYPER_OD;
        dev.gpios[0].speed        = GPIO_PIN_VERY_HIGH_SPEED;
   
    /* DECLARE TFT RESET GPIO_PD15  */
        dev.gpios[1].mask         = GPIO_MASK_SET_MODE | GPIO_MASK_SET_PUPD | GPIO_MASK_SET_TYPE | GPIO_MASK_SET_SPEED;
        dev.gpios[1].kref.port    = GPIO_PD;
        dev.gpios[1].kref.pin     = 15;
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
    volatile int i;


    sys_cfg(CFG_GPIO_SET,(uint8_t)((('D' - 'A') << 4) + 15),0); 
    for(i=0;i<1000;i++);
    spi1_init();
    spi_enable(1);

    sys_cfg(CFG_GPIO_SET,(uint8_t)((('D' - 'A') << 4) + 15),1); 
    for(i=0;i<1000;i++);
    
    tft_send_command(0x1);//Soft RESET
    
    //FIXME: Should be sleep Here
    for(i=0;i<10000;i++);

    tft_send_command(0x28);//Display OFF

    tft_send_command(0xC0);//POWER CONTROL1
    tft_send_param(0x29);/* Adjust the GVDD, the reference level for the VCOM
    *0X23 = 5V
    *0X9 = 3.3v
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
    for(i=0;i<100000;i++);
    tft_send_command(0x29);//ILI9341_DISPLAYON
    for(i=0;i<200000;i++);

    return 0;
}
/* Miscelleanous function for send commands and receiveing datas */

int tft_send_command(uint8_t command)
{
  int res;
  /* Wait for any already launched SPI transfert to complete */
  while(spi1_is_busy());
  /*DOWN the D/CX line */
  DOWN_NSS;
  DOWN_CX;
  /* send the command */
  res=spi_master_send_byte_sync(1,command);
  UP_NSS;
  return res;
}

int tft_send_param(uint8_t param)
{
  int res;
  /* Wait for any already launched SPI transfert to complete */
  while(spi1_is_busy());
  /*UP the D/CX line */
  DOWN_NSS;
  UP_CX;
  /* send the parameter */
  res=spi_master_send_byte_sync(1,param);
  UP_NSS;
  return res;
}

void tft_setxy(int x1, int x2, int y1, int y2)
{
  lock_bus(1);
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
  unlock_bus();
}

void tft_setxy_unlocked(int x1, int x2, int y1, int y2)
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
lock_bus(1);
  tft_send_command(0x2C); //write
 // set_reg_bits(GPIOA_ODR,D_CX_BIT); /* send the parameter */
 DOWN_NSS;
 UP_CX;
#if 0
   {
    uint8_t rgb[300];
    int i;
    for(i=0;i<300;i+=3)
      {
    rgb[i+0]=r;
    rgb[1+i]=g;
    rgb[2+i]=b;
      }
     i=(((x2-x1)+1)*((y2-y1)+1))*3;
    spi1_master_send_bytes_async_circular(rgb,(300>i?i:300),i);
   }
#else
  {
    int i;
     for(i=0;i<(((x2-x1)+1)*((y2-y1)+1));i++)
  {
       spi_master_send_byte_sync(1,r);
       spi_master_send_byte_sync(1,g);
       spi_master_send_byte_sync(1,b);
   }
}
#endif
UP_NSS;
unlock_bus();
#if 0
tft_setxy(x1,x2,y1,y2);
  tft_send_command(0x2e); //read
  set_reg_bits(GPIOA_ODR,D_CX_BIT);
  for(i=0;i<(((x2-x1)+1)*((y2-y1)+1))*3;i++)
       spi1_master_send_byte_sync(0xca);
#endif
}

void tft_fill_rectangle_unlocked(int x1, int x2, int y1, int y2, uint8_t r, uint8_t g, uint8_t b)
{
  //int i;
//Clear B_CS dans P_CS

        tft_setxy_unlocked(x1,x2,y1,y2);
//Set B_RS dans P_RS
  tft_send_command(0x2C); //write
 // set_reg_bits(GPIOA_ODR,D_CX_BIT); /* send the parameter */
 DOWN_NSS;
 UP_CX;
#if 1
 {
 int i;
  for(i=0;i<(((x2-x1)+1)*((y2-y1)+1));i++)
  {
       spi_master_send_byte_sync(1,r);
       spi_master_send_byte_sync(1,g);
       spi_master_send_byte_sync(1,b);
   }
}
#else
        {
    uint8_t rgb[1800];
    int i;
    for(i=0;i<1800;i+=3)
      {
    rgb[i+0]=r;
    rgb[1+i]=g;
    rgb[2+i]=b;
      }
  spi1_master_send_bytes_async_circular(rgb,1800,(((x2-x1)+1)*((y2-y1)+1))*3);
  /* there is 3 byte per pixel !!! */
      }
#endif
UP_NSS;
}
void tft_invert_rectangle_unlocked(int x1,int x2,int y1,int y2)
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
        tft_setxy_unlocked(i,i+sup,j,j+1);
        tft_send_command(0x2e);//memory read

        DOWN_NSS;
        UP_CX;
        spi_master_send_byte_sync(1,0);//dummy READ as in spec p116
        for(k=0;k<3*sup;k++)
          {
            stock[k]=spi_master_send_byte_sync(1,0);//0 is dummy data
          }
        tft_setxy_unlocked(i,i+sup,j,j+1);
        tft_send_command(0x2c);//memory write
        DOWN_NSS;
        UP_CX;
        for(k=0;k<3*sup;k++)
          spi_master_send_byte_sync(1,(stock[k])^255);//Ensure the 3 lsb are 0
        UP_NSS;
        }
  }
}

void tft_send_image(int x1,int x2, int y1,int y2, uint8_t *data)
{
  int i;
  tft_setxy(x1,x2,y1,y2);
  lock_bus(1);
  tft_send_command(0x2c); /*Memory write */
  //set_reg_bits(GPIOA_ODR,D_CX_BIT); /* send the parameter */
  DOWN_NSS;
  UP_CX;
  for(i=0;i<(((x2-x1)+1)*((y2-y1)+1));i++)
    {
      spi_master_send_byte_sync(1,data[3*i]);
      spi_master_send_byte_sync(1,data[3*i+1]);
      spi_master_send_byte_sync(1,data[3*i+2]);
    }
  UP_NSS;
  unlock_bus();
  //  spi1_master_send_bytes_async(data,length);
}

void tft_send_image_unlocked(int x1,int x2, int y1,int y2, uint8_t *data)
{
  int i;
  tft_setxy_unlocked(x1,x2,y1,y2);
  tft_send_command(0x2c); /*Memory write */
  //set_reg_bits(GPIOA_ODR,D_CX_BIT); /* send the parameter */
  DOWN_NSS;
  UP_CX;
  for(i=0;i<(((x2-x1)+1)*((y2-y1)+1));i++)
    {
      spi_master_send_byte_sync(1,data[3*i]);
      spi_master_send_byte_sync(1,data[3*i+1]);
      spi_master_send_byte_sync(1,data[3*i+2]);
    }
  UP_NSS;
  //  spi1_master_send_bytes_async(data,length);
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
        //
        // Les tailles sont en pixel
        //
        int i=0,j;
        start_pos_px=char_width*c;
        start_line=0;
        nlines=char_height+font_blankskip/2;

        first_byte=start_pos_px/8;
        offset_bit=start_pos_px&7;
        lock_bus(1);
        tft_setxy_unlocked(current_posx,
            (current_posx+char_width>240?240:current_posx+char_width),
            current_posy,
            (current_posy+char_height>320?320:current_posy+char_height));
        //big box not obligatory fully fiiled depending on characters
        tft_send_command(0x2c);
        UP_CX;
        DOWN_NSS;
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
                spi_master_send_byte_sync(1,fg_color[0]);
                spi_master_send_byte_sync(1,fg_color[1]);
                spi_master_send_byte_sync(1,fg_color[2]);
              }
              else
              {
                spi_master_send_byte_sync(1,bg_color[0]);
                spi_master_send_byte_sync(1,bg_color[1]);
                spi_master_send_byte_sync(1,bg_color[2]);
              }
              size_printed++;
            }
            myoffset=0;
          }
        }
        UP_NSS;
        unlock_bus();

        //current_posy+=char_width+char_height;
        current_posx+=char_width;

}
int tft_puts(char *s)
{
  char *tmp=s;
  while(*s)
    tft_putc(*(s++));
  return s-tmp;
}
void tft_putc_unlocked(char c)
{
        /* The font file is organized like ASCCI 7bit on One line */
        int char_height=(font_height/2);
        int char_width=((font_width)/128);
        int start_pos_px,first_byte,offset_bit;
        int start_line;
        int nlines;
        //
        // Les tailles sont en pixel
        //
        int i=0,j;
        start_pos_px=char_width*c;
        start_line=0;
        nlines=char_height+font_blankskip/2;

        first_byte=start_pos_px/8;
        offset_bit=start_pos_px&7;
        tft_setxy_unlocked(current_posx,
            (current_posx+char_width>240?240:current_posx+char_width),
            current_posy,
            (current_posy+char_height>320?320:current_posy+char_height));
        //big box not obligatory fully fiiled depending on characters
        tft_send_command(0x2c);
        UP_CX;
        DOWN_NSS;
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
                spi_master_send_byte_sync(1,fg_color[0]);
                spi_master_send_byte_sync(1,fg_color[1]);
                spi_master_send_byte_sync(1,fg_color[2]);
              }
              else
              {
                spi_master_send_byte_sync(1,bg_color[0]);
                spi_master_send_byte_sync(1,bg_color[1]);
                spi_master_send_byte_sync(1,bg_color[2]);
              }
              size_printed++;
            }
            myoffset=0;
          }
        }
        UP_NSS;

        //current_posy+=char_width+char_height;
        current_posx+=char_width;

}
int tft_puts_unlocked(char *s)
{
  char *tmp=s;
  while(*s)
    tft_putc(*(s++));
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
  lock_bus(1);
  tft_setxy_unlocked(x,x+width - 1,y,y+height - 1);
  tft_send_command(0x2c);
  UP_CX;
  DOWN_NSS;
  for(i=0;i<datalen;i+=2)
  {
        int nb;
        for(nb=0;nb<data[i+1];nb++)
        {
                spi_master_send_byte_sync(1,colormap[3*data[i]]&~7);
                spi_master_send_byte_sync(1,colormap[3*data[i]+1]&~7);
                spi_master_send_byte_sync(1,colormap[3*data[i]+2]&~7);
        }
  }
  UP_NSS;
  unlock_bus();
}


/* other functions  (low level commands execution) */

void screen_save_nss_status()
{
//  nss=read_reg_value(GPIOA_ODR)&(1<<4);
  nss=sys_cfg(CFG_GPIO_GET,(uint8_t)(4),0);;
  UP_NSS;
}
void screen_restore_nss_status()
{
  if(!nss)
  {
    DOWN_NSS;
  }
  else
  {
    UP_NSS;
  }
}



