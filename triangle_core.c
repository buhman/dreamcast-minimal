#include <stdint.h>

/*
  This demo does not work in emulators:

  - Flycast does not work because it does not emulate CORE whatsoever

  - Devcast does not work because it does not perform (the equivalent of) boot
    rom initialization when loading .elf files

  In an attempt to reduce boilerplate, this demo presumes the boot rom has
  initialized Holly with the values needed to display the "PRODUCED BY OR UNDER
  LICENSE FROM SEGA ENTERPRESES, LTD." screen, and that no register values have
  been modified beyond boot rom initialization.
 */

/* Texture memory access

  texture_memory64 and texture_memory32 refer two different addressing schemes
  over the same 8MB of physical texture memory.

  Generally speaking the texture_memory64 address scheme is used for textures
  (any texture memory address referenced by `texture_control_word`), and
  texture_memory32 is used for everything else.

  E_DC_HW_outline.pdf "2.4 System memory mapping" (PDF page 10)
 */
uint32_t texture_memory32 = 0xa5000000;

/*
  You might want to at least read DCDBSysArc990907E.pdf page 168 before
  continuing.

  The "TA" is completely unused and ignored in this demo.

  The organization of this file matches the left-to-right ordering of Fig. 3-53.

  Minimally, in order to render anything, core requires valid texture memory
  pointers to:

  - a Region Array
  - an Object List
  - (polygon) ISP/TSP Parameter(s)
  - (background) ISP/TSP Parameter(s)
  - an area in texture memory for a framebuffer
 */

/******************************************************************************
 Region array
 ******************************************************************************/

/*
  These "region array entries" are briefly illustrated in DCDBSysArc990907E.pdf
  page 168, 177-180.

  The number of list pointers per region array entry is affected by
  FPU_PARAM_CFG "Region Header Type" (page 368). This struct models the
  "6 × 32bit/Tile Type 2" mode.
*/
typedef struct region_array_entry {
  uint32_t tile;
  struct {
    uint32_t opaque;
    uint32_t opaque_modifier_volume;
    uint32_t translucent;
    uint32_t translucent_modifier_volume;
    uint32_t punch_through;
  } list_pointer;
} region_array_entry;
static_assert((sizeof (struct region_array_entry)) == 4 * 6);

/*
  DCDBSysArc990907E.pdf page 216-217 describes the REGION_ARRAY__ bit fields:
 */
#define REGION_ARRAY__TILE__LAST_REGION (1 << 31)
#define REGION_ARRAY__TILE__Y_POSITION(n) (((n) & 0x3f) << 8)
#define REGION_ARRAY__TILE__X_POSITION(n) (((n) & 0x3f) << 2)

#define REGION_ARRAY__LIST_POINTER__EMPTY (1 << 31)
#define REGION_ARRAY__LIST_POINTER__OBJECT_LIST(n) (((n) & 0xfffffc) << 0)

void transfer_region_array(uint32_t region_array_start,
                           uint32_t opaque_list_pointer)
{
  /*
    Create a minimal region array with a single entry:
       - one tile at tile coordinate (0, 0) with one opaque list pointer
  */

  /*
    Holly reads the region array from "32-bit" texture memory address space,
    so the region array is correspondingly written from "32-bit" address space.
   */
  volatile region_array_entry * region_array = (volatile region_array_entry *)(texture_memory32 + region_array_start);

  region_array[0].tile
    = REGION_ARRAY__TILE__LAST_REGION
    | REGION_ARRAY__TILE__Y_POSITION(0)
    | REGION_ARRAY__TILE__X_POSITION(0);

  /*
    list pointers are offsets relative to the beginning of "32-bit" texture memory.

    Each list type uses different rasterization steps, "opaque" being the fastest and most efficient.
   */
  region_array[0].list_pointer.opaque                      = REGION_ARRAY__LIST_POINTER__OBJECT_LIST(opaque_list_pointer);
  region_array[0].list_pointer.opaque_modifier_volume      = REGION_ARRAY__LIST_POINTER__EMPTY;
  region_array[0].list_pointer.translucent                 = REGION_ARRAY__LIST_POINTER__EMPTY;
  region_array[0].list_pointer.translucent_modifier_volume = REGION_ARRAY__LIST_POINTER__EMPTY;
  region_array[0].list_pointer.punch_through               = REGION_ARRAY__LIST_POINTER__EMPTY;
}

/*****************************************************************************
 Object list
 *****************************************************************************/

#define OBJECT_LIST__POINTER_TYPE__TRIANGLE_ARRAY (0b100 << 29)
#define OBJECT_LIST__POINTER_TYPE__OBJECT_POINTER_BLOCK_LINK (0b111 << 29)

#define OBJECT_LIST__TRIANGLE_ARRAY__NUMBER_OF_TRIANGLES(n) (((n) & 0xf) << 25)
#define OBJECT_LIST__TRIANGLE_ARRAY__SKIP(n) (((n) & 0x7) << 21)
#define OBJECT_LIST__TRIANGLE_ARRAY__START(n) (((n) & 0x1fffff) << 0)

#define OBJECT_LIST__OBJECT_POINTER_BLOCK_LINK__END_OF_LIST (1 << 28)

void transfer_object_list(uint32_t object_list_start, uint32_t triangle_array_offset)
{
  /*
    Create a minimal object list with a single triangle array.

    See DCDBSysArc990907E.pdf page 218-219
   */

  volatile uint32_t * object_list = (volatile uint32_t *)(texture_memory32 + object_list_start);

  /*
    skip: the size of isp_tsp_parameter__vertex is 4 × 32-bit words
          4 - 3 = 1
    (page 218)
  */

  object_list[0] = OBJECT_LIST__POINTER_TYPE__TRIANGLE_ARRAY
                 | OBJECT_LIST__TRIANGLE_ARRAY__NUMBER_OF_TRIANGLES(0)
                 | OBJECT_LIST__TRIANGLE_ARRAY__SKIP(1)
                 | OBJECT_LIST__TRIANGLE_ARRAY__START(triangle_array_offset / 4);

  object_list[1] = OBJECT_LIST__POINTER_TYPE__OBJECT_POINTER_BLOCK_LINK
                 | OBJECT_LIST__OBJECT_POINTER_BLOCK_LINK__END_OF_LIST;
}

/******************************************************************************
 ISP/TSP Parameter
 ******************************************************************************/

/*
  Other examples of possible ISP/TSP parameter formats are shown on
  DCDBSysArc990907E.pdf page 221. Page 221 is non-exhaustive, and many
  permutations are possible.

  Parameter format selection is controlled mostly by the value of the
  `isp_tsp_instruction_word` (always present).

  This is most similar to the "2 Stripped Triangle Polygon (Non-Textured,
  Gouraud)" example (except this is for a non-strip triangle).
*/
typedef struct isp_tsp_parameter__vertex {
  float x;
  float y;
  float z;
  uint32_t color;
} isp_tsp_parameter__vertex;

typedef struct isp_tsp_parameter__polygon {
  uint32_t isp_tsp_instruction_word;
  uint32_t tsp_instruction_word;
  uint32_t texture_control_word;
  isp_tsp_parameter__vertex a;
  isp_tsp_parameter__vertex b;
  isp_tsp_parameter__vertex c;
} isp_tsp_parameter__polygon;

/*
  isp_tsp_instruction_word bits

  DCDBSysArc990907E.pdf page 222-225
 */
#define ISP_TSP_INSTRUCTION_WORD__DEPTH_COMPARE_MODE__ALWAYS (7 << 29)

#define ISP_TSP_INSTRUCTION_WORD__CULLING_MODE__NO_CULLING (0 << 27)

#define ISP_TSP_INSTRUCTION_WORD__GOURAUD_SHADING (1 << 23)

/*
  tsp_instruction_word bits

  DCDBSysArc990907E.pdf page 226-232
 */
#define TSP_INSTRUCTION_WORD__SRC_ALPHA_INSTR__ONE (1 << 29)
#define TSP_INSTRUCTION_WORD__DST_ALPHA_INSTR__ZERO (0 << 26)
#define TSP_INSTRUCTION_WORD__FOG_CONTROL__NO_FOG (0b10 << 22)

void transfer_isp_tsp_polygon_parameter(uint32_t isp_tsp_parameter_start)
{
  /*
    Create a minimal triangle polygon:
      - non-textured
      - packed color
      - gouraud shaded
      - single volume
   */

  /*
    Holly reads ISP/TSP parameters from "32-bit" texture memory address space,
    so ISP/TSP parameters are correspondingly written from "32-bit" address space.
   */
  volatile isp_tsp_parameter__polygon * params = (volatile isp_tsp_parameter__polygon *)(texture_memory32 + isp_tsp_parameter_start);

  params[0].isp_tsp_instruction_word = ISP_TSP_INSTRUCTION_WORD__DEPTH_COMPARE_MODE__ALWAYS
                                     | ISP_TSP_INSTRUCTION_WORD__CULLING_MODE__NO_CULLING
                                     | ISP_TSP_INSTRUCTION_WORD__GOURAUD_SHADING;

  params[0].tsp_instruction_word = TSP_INSTRUCTION_WORD__SRC_ALPHA_INSTR__ONE
                                 | TSP_INSTRUCTION_WORD__DST_ALPHA_INSTR__ZERO
                                 | TSP_INSTRUCTION_WORD__FOG_CONTROL__NO_FOG;

  params[0].texture_control_word = 0;

  /*
    An ~equilateral triangle, roughly centered inside the area of the 32x32 tile
    at tile coordinate (0, 0), screen space coordinates, clockwise:
   */
  // bottom left
  params[0].a.x =  1.0f;
  params[0].a.y = 29.0f;
  params[0].a.z =  0.1f;
  params[0].a.color = 0xff0000; // red

  // top center
  params[0].b.x = 16.0f;
  params[0].b.y =  3.0f;
  params[0].b.z =  0.1f;
  params[0].b.color = 0x00ff00; // green

  // bottom right
  params[0].c.x = 31.0f;
  params[0].c.y = 29.0f;
  params[0].c.z =  0.1f;
  params[0].c.color = 0x0000ff; // blue
}

void transfer_isp_tsp_background_parameter(uint32_t isp_tsp_parameter_start)
{
  /*
    Create a minimal background parameter:
      - non-textured
      - packed color
      - single volume
   */

  volatile isp_tsp_parameter__polygon * params = (volatile isp_tsp_parameter__polygon *)(texture_memory32 + isp_tsp_parameter_start);

  params[1].isp_tsp_instruction_word = ISP_TSP_INSTRUCTION_WORD__DEPTH_COMPARE_MODE__ALWAYS
                                     | ISP_TSP_INSTRUCTION_WORD__CULLING_MODE__NO_CULLING;

  params[1].tsp_instruction_word = TSP_INSTRUCTION_WORD__SRC_ALPHA_INSTR__ONE
                                 | TSP_INSTRUCTION_WORD__DST_ALPHA_INSTR__ZERO
                                 | TSP_INSTRUCTION_WORD__FOG_CONTROL__NO_FOG;

  params[1].texture_control_word = 0;

  /*
    An ~equilateral triangle, roughly centered inside the area of the 32x32 tile
    at tile coordinate (0, 0), screen space coordinates, clockwise:
   */
  // top left
  params[1].a.x =  0.0f;
  params[1].a.y =  0.0f;
  params[1].a.z =  0.00001f;
  params[1].a.color = 0xff00ff; // magenta

  // top right
  params[1].b.x = 31.0f;
  params[1].b.y =  0.0f;
  params[1].b.z =  0.00001f;
  params[1].b.color = 0xff00ff; // magenta

  // bottom right
  params[1].c.x = 31.0f;
  params[1].c.y = 31.0f;
  params[1].c.z =  0.00001f;
  params[1].c.color = 0xff00ff; // magenta

  // bottom left (implied)
}

/* background */
#define ISP_BACKGND_T__SKIP(n) (((n) & 0x7) << 24)
#define ISP_BACKGND_T__TAG_ADDRESS(n) (((n) & 0x1fffff) << 3)
#define ISP_BACKGND_T__TAG_OFFSET(n) (((n) & 0x7) << 0)

volatile uint32_t * STARTRENDER   = (volatile uint32_t *)(0xa05f8000 + 0x14);
volatile uint32_t * PARAM_BASE    = (volatile uint32_t *)(0xa05f8000 + 0x20);
volatile uint32_t * REGION_BASE   = (volatile uint32_t *)(0xa05f8000 + 0x2c);
volatile uint32_t * FB_R_SOF1     = (volatile uint32_t *)(0xa05f8000 + 0x50);
volatile uint32_t * FB_W_SOF1     = (volatile uint32_t *)(0xa05f8000 + 0x60);
volatile uint32_t * ISP_BACKGND_T = (volatile uint32_t *)(0xa05f8000 + 0x8c);

void main()
{
  /*
    a very simple memory map:

    the ordering within texture memory is not significant, and could be
    anything
  */
  uint32_t framebuffer_start       = 0x200000; // intentionally the same address that the boot rom used to draw the SEGA logo
  uint32_t isp_tsp_parameter_start = 0x400000;
  uint32_t region_array_start      = 0x500000;
  uint32_t object_list_start       = 0x100000;
  uint32_t opaque_list_pointer     = object_list_start;

  // triangle_array_offset is relative to the beginning of isp_tsp_parameter_start
  //
  // transfer_isp_tsp_polygon_parameter writes to the beginning of
  // isp_tsp_parameter start, so the value of triangle_array_offset is zero
  uint32_t triangle_array_offset = (sizeof (isp_tsp_parameter__polygon)) * 0;
  // background_offset is also relative to the beginning of
  // isp_tsp_parameter_start
  uint32_t background_offset     = (sizeof (isp_tsp_parameter__polygon)) * 1;

  transfer_region_array(region_array_start, opaque_list_pointer);

  transfer_object_list(object_list_start, triangle_array_offset);

  transfer_isp_tsp_polygon_parameter(isp_tsp_parameter_start);

  transfer_isp_tsp_background_parameter(isp_tsp_parameter_start);

  // configure CORE

  // REGION_BASE is the (texture memory-relative) address of the region array.
  *REGION_BASE = region_array_start;

  // PARAM_BASE is the (texture memory-relative) address of ISP/TSP parameters.
  // Anything that references an ISP/TSP parameter does so relative to this
  // address (and not relative to the beginning of texture memory).
  *PARAM_BASE = isp_tsp_parameter_start;

  // Set the offset of the background ISP/TSP parameter, relative to PARAM_BASE
  // SKIP is related to the size of each vertex
  *ISP_BACKGND_T = ISP_BACKGND_T__TAG_ADDRESS(background_offset / 4)
                 | ISP_BACKGND_T__TAG_OFFSET(0)
                 | ISP_BACKGND_T__SKIP(1);

  // FB_W_SOF1 is the (texture memory-relative) address of the framebuffer that
  // will be written to when a tile is rendered/flushed.
  *FB_W_SOF1 = framebuffer_start;

  // start the actual render--the rendering process begins by interpreting the
  // region array
  *STARTRENDER = 1;

  // without waiting for rendering to actually complete, immediately display the
  // framebuffer.
  *FB_R_SOF1 = framebuffer_start;

  // return from main; this will effectively jump back to the serial loader
}
