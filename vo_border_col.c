#include <stdint.h>

typedef volatile uint32_t reg32;

void main()
{
  reg32 * VO_BORDER_COL = (reg32 *)(0x005F8040 | 0xA0000000);
  reg32 * FB_R_CTRL = (reg32 *)(0x005F8044 | 0xA0000000);

  int vclk_div = 1;
  int fb_enable = 0;
  *FB_R_CTRL
    = (vclk_div  << 23)
    | (fb_enable <<  0);

  int green = 0x00ff00;
  *VO_BORDER_COL = green;
}
