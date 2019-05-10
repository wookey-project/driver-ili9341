#ifndef LIBTFT_H_
#define LIBTFT_H_

#include "libc/types.h"


uint8_t tft_early_init(void);
uint8_t tft_init(void);

int tft_send_command(uint8_t command);
int tft_send_param(uint8_t param);
void tft_setxy(int x1, int x2, int y1, int y2);
void tft_fill_rectangle(int x1, int x2, int y1, int y2, uint8_t r, uint8_t g, uint8_t b);
void tft_send_image(int x1,int x2, int y1,int y2, uint8_t *data);
void tft_set_cursor_pos(int x,int y);
void tft_putc(char c);
int tft_puts(const char *s);
void tft_setfg(uint8_t r, uint8_t g, uint8_t b);
void tft_setbg(uint8_t r, uint8_t g, uint8_t b);
void tft_rle_image(int x, int y,int width, int height, const uint8_t *colormap,
                   const uint8_t *data, int datalen);
void screen_save_nss_status();
void screen_restore_nss_status();
void tft_flush_fifo();


#endif
