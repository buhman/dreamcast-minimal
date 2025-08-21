#include <stdint.h>

void main()
{
  volatile uint32_t * texture_memory32 = (volatile uint32_t *)(0x05000000 | 0xA0000000);

  int blue = 0x0000ff;
  for (int i = 0; i < 640 * 480; i++) {
    texture_memory32[i] = blue;
  }

  volatile uint32_t * FB_R_CTRL = (volatile uint32_t*)(0x005F8044 | 0xA0000000);

  int vclk_div = 1;
  int fb_depth = 0x3; // 0888 RGB 32 bit
  int fb_enable = 1;
  *FB_R_CTRL
    = (vclk_div  << 23)
    | (fb_depth  << 2 )
    | (fb_enable << 0 );

  volatile uint32_t * FB_R_SOF1 = (volatile uint32_t*)(0x005F8050 | 0xA0000000);
  // the framebuffer is at the start of texture memory (texture memory address 0)
  *FB_R_SOF1 = 0;

  volatile uint32_t * FB_R_SIZE = (volatile uint32_t*)(0x005F805C | 0xA0000000);

  int fb_modulus = 1;
  int fb_y_size = 480 - 1;
  int bytes_per_pixel = 4;
  int fb_x_size = ((640 * bytes_per_pixel) / 4) - 1;
  *FB_R_SIZE
    = (fb_modulus << 20)
    | (fb_y_size  << 10)
    | (fb_x_size  << 0 );
}
