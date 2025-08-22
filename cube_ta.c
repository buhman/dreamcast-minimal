#include <stdint.h>

/*
  This demo does not work in emulators:

  - Flycast does not work because it strangely is incapable of displaying a
    single rendered frame.

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
const uint32_t texture_memory32 = 0xa5000000;

/* The "TA Polygon Converter FIFO" is a Holly functional unit. */
const uint32_t ta_polygon_converter_fifo = 0x10000000;

/* The "Store Queue" is a SH4 functional unit. */
const uint32_t store_queue = 0xe0000000;

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
#define ISP_TSP_INSTRUCTION_WORD__DEPTH_COMPARE_MODE__GREATER (4 << 29)

#define ISP_TSP_INSTRUCTION_WORD__CULLING_MODE__NO_CULLING (0 << 27)

#define ISP_TSP_INSTRUCTION_WORD__GOURAUD_SHADING (1 << 23)

/*
  tsp_instruction_word bits

  DCDBSysArc990907E.pdf page 226-232
 */
#define TSP_INSTRUCTION_WORD__SRC_ALPHA_INSTR__ONE (1 << 29)
#define TSP_INSTRUCTION_WORD__DST_ALPHA_INSTR__ZERO (0 << 26)
#define TSP_INSTRUCTION_WORD__FOG_CONTROL__NO_FOG (0b10 << 22)

void transfer_isp_tsp_background_parameter(uint32_t isp_tsp_parameter_start)
{
  /*
    Create a minimal background parameter:
      - non-textured
      - packed color
      - single volume
   */

  volatile isp_tsp_parameter__polygon * params = (volatile isp_tsp_parameter__polygon *)(texture_memory32 + isp_tsp_parameter_start);

  params[0].isp_tsp_instruction_word = ISP_TSP_INSTRUCTION_WORD__DEPTH_COMPARE_MODE__ALWAYS
                                     | ISP_TSP_INSTRUCTION_WORD__CULLING_MODE__NO_CULLING;

  params[0].tsp_instruction_word = TSP_INSTRUCTION_WORD__SRC_ALPHA_INSTR__ONE
                                 | TSP_INSTRUCTION_WORD__DST_ALPHA_INSTR__ZERO
                                 | TSP_INSTRUCTION_WORD__FOG_CONTROL__NO_FOG;

  params[0].texture_control_word = 0;

  // top left
  params[0].a.x =  0.0f;
  params[0].a.y =  0.0f;
  params[0].a.z =  0.00001f;
  params[0].a.color = 0xff00ff; // magenta

  // top right
  params[0].b.x = 32.0f;
  params[0].b.y =  0.0f;
  params[0].b.z =  0.00001f;
  params[0].b.color = 0xff00ff; // magenta

  // bottom right
  params[0].c.x = 32.0f;
  params[0].c.y = 32.0f;
  params[0].c.z =  0.00001f;
  params[0].c.color = 0xff00ff; // magenta

  // bottom left (implied)
}

/* background */
#define ISP_BACKGND_T__SKIP(n) (((n) & 0x7) << 24)
#define ISP_BACKGND_T__TAG_ADDRESS(n) (((n) & 0x1fffff) << 3)
#define ISP_BACKGND_T__TAG_OFFSET(n) (((n) & 0x7) << 0)

/******************************************************************************
 SH4 store queue
 ******************************************************************************/

/*
  The TA polygon converter FIFO requires 32-byte bus access. Attempts to access
  the TA with smaller bus accesses will result in incorrect TA operation. The
  Dreamcast has three mechanisms that can generate 32-byte writes:

  - SH4 store queue (commonly used)

  - Holly CH2-DMA (commonly used)

  - meticulous and clever use of SH4 cache writeback (esoteric forbidden technique)

  Of these, the mechanism that requires the least code is the SH4 store queue,
  so this demo will also use the SH4 store queue for that reason.

  The SH4 store queue is described in sh7091pm_e.pdf printed page 61-64 and
  79-81.
*/

// sh7091pm_e.pdf:
//  > Issuing a PREF instruction for P4 area H'E000 0000 to H'E3FF FFFC starts a
//  > burst transfer from the SQs to external memory.
#define pref(address) \
  { asm volatile ("pref @%0" : : "r" (address) : "memory"); }

volatile uint32_t * SH7091__CCN__QACR0 = (volatile uint32_t *)(0xff000000 + 0x38);
volatile uint32_t * SH7091__CCN__QACR1 = (volatile uint32_t *)(0xff000000 + 0x3c);

/******************************************************************************
 TA Parameters
 ******************************************************************************/

/*
  The primary advantage of using the TA: it will generate object lists on your
  behalf, and does a reasonable job of excluding object list entries from tiles
  that are entirely outside the area of that triangle.

  In addition, the TA can be used to perform floating point to integer color
  packing, including color component clamping. On the SH4, each floating point
  to integer color conversion requires at least 50-60 clock cycles, whereas the
  TA can do the same conversion much more quickly (~1 clock cycle).

  Floating point color is typical when performing (colored) lighting/shading
  calculations.
 */

/*
  TA parameters are roughly superset of CORE ISP/TSP parameters.

  There are a few differences:

  - the TA overwrites certain ISP/TSP Instruction Word bits, based on duplicated
    values in the TA Parameter Control Word (DCDBSysArc990907E.pdf page 200)

  - the TA supports several (floating point) vertex color formats, whereas CORE
    exclusively supports 32-bit packed integer ARGB color.
 */

typedef struct ta_global_parameter__polygon_type_0 {
  uint32_t parameter_control_word;
  uint32_t isp_tsp_instruction_word;
  uint32_t tsp_instruction_word;
  uint32_t texture_control_word;
  uint32_t _res0;
  uint32_t _res1;
  uint32_t data_size_for_sort_dma;
  uint32_t next_address_for_sort_dma;
} ta_global_parameter__polygon_type_0;
static_assert((sizeof (struct ta_global_parameter__polygon_type_0)) == 32);

typedef struct ta_global_parameter__end_of_list {
  uint32_t parameter_control_word;
  uint32_t _res0;
  uint32_t _res1;
  uint32_t _res2;
  uint32_t _res3;
  uint32_t _res4;
  uint32_t _res5;
  uint32_t _res6;
} ta_global_parameter__end_of_list;
static_assert((sizeof (struct ta_global_parameter__end_of_list)) == 32);

/*
  The TA only supports polygon/triangle vertex input represented as a triangle
  strip. TA triangle strips can be any length between 1 and infinity (or the end
  of texture memory, whichever comes first). CORE triangle strips can be any
  length between 1 and 6. The TA automatically splits infinite-length strips
  into strip lengths that CORE supports.

  See DCDBSysArc990907E.pdf page 181.
 */
typedef struct ta_vertex_parameter__polygon_type_0 {
  uint32_t parameter_control_word;
  float x;
  float y;
  float z;
  uint32_t _res0;
  uint32_t _res1;
  uint32_t base_color;
  uint32_t _res2;
} ta_vertex_parameter__polygon_type_0;
static_assert((sizeof (struct ta_vertex_parameter__polygon_type_0)) == 32);

#define PARAMETER_CONTROL_WORD__PARA_CONTROL__PARA_TYPE__END_OF_LIST (0 << 29)
#define PARAMETER_CONTROL_WORD__PARA_CONTROL__PARA_TYPE__POLYGON_OR_MODIFIER_VOLUME (4 << 29)
#define PARAMETER_CONTROL_WORD__PARA_CONTROL__PARA_TYPE__VERTEX_PARAMETER (7 << 29)
#define PARAMETER_CONTROL_WORD__PARA_CONTROL__END_OF_STRIP (1 << 28)
#define PARAMETER_CONTROL_WORD__PARA_CONTROL__LIST_TYPE__OPAQUE (0 << 24)
#define PARAMETER_CONTROL_WORD__OBJ_CONTROL__COL_TYPE__PACKED_COLOR (0 << 4)
#define PARAMETER_CONTROL_WORD__OBJ_CONTROL__GOURAUD (1 << 1)

static inline uint32_t transfer_ta_global_end_of_list(uint32_t store_queue_ix)
{
  //
  // TA "end of list" global transfer
  //

  volatile ta_global_parameter__end_of_list * end_of_list = (volatile ta_global_parameter__end_of_list *)store_queue_ix;

  end_of_list->parameter_control_word = PARAMETER_CONTROL_WORD__PARA_CONTROL__PARA_TYPE__END_OF_LIST;

  // start store queue transfer of `end_of_list` to the TA
  pref(store_queue_ix);

  store_queue_ix += (sizeof (ta_global_parameter__end_of_list));

  return store_queue_ix;
}

static inline uint32_t transfer_ta_vertex_triangle(uint32_t store_queue_ix,
                                                   float ax, float ay, float az, uint32_t ac,
                                                   float bx, float by, float bz, uint32_t bc,
                                                   float cx, float cy, float cz, uint32_t cc)
{
  //
  // TA polygon vertex transfer
  //

  volatile ta_vertex_parameter__polygon_type_0 * vertex = (volatile ta_vertex_parameter__polygon_type_0 *)store_queue_ix;

  // bottom left
  vertex[0].parameter_control_word = PARAMETER_CONTROL_WORD__PARA_CONTROL__PARA_TYPE__VERTEX_PARAMETER;
  vertex[0].x = ax;
  vertex[0].y = ay;
  vertex[0].z = az;
  vertex[0].base_color = ac;

  // start store queue transfer of `vertex[0]` to the TA
  pref(store_queue_ix + 32 * 0);

  // top center
  vertex[1].parameter_control_word = PARAMETER_CONTROL_WORD__PARA_CONTROL__PARA_TYPE__VERTEX_PARAMETER;
  vertex[1].x = bx;
  vertex[1].y = by;
  vertex[1].z = bz;
  vertex[1].base_color = bc;

  // start store queue transfer of `vertex[1]` to the TA
  pref(store_queue_ix + 32 * 1);

  // bottom right
  vertex[2].parameter_control_word = PARAMETER_CONTROL_WORD__PARA_CONTROL__PARA_TYPE__VERTEX_PARAMETER
                                   | PARAMETER_CONTROL_WORD__PARA_CONTROL__END_OF_STRIP;
  vertex[2].x = cx;
  vertex[2].y = cy;
  vertex[2].z = cz;
  vertex[2].base_color = cc;

  // start store queue transfer of `params[2]` to the TA
  pref(store_queue_ix + 32 * 2);

  store_queue_ix += (sizeof (ta_vertex_parameter__polygon_type_0)) * 3;

  return store_queue_ix;
}

static inline uint32_t transfer_ta_global_polygon(uint32_t store_queue_ix)
{
  //
  // TA polygon global transfer
  //

  volatile ta_global_parameter__polygon_type_0 * polygon = (volatile ta_global_parameter__polygon_type_0 *)store_queue_ix;

  polygon->parameter_control_word = PARAMETER_CONTROL_WORD__PARA_CONTROL__PARA_TYPE__POLYGON_OR_MODIFIER_VOLUME
                                  | PARAMETER_CONTROL_WORD__PARA_CONTROL__LIST_TYPE__OPAQUE
                                  | PARAMETER_CONTROL_WORD__OBJ_CONTROL__COL_TYPE__PACKED_COLOR
                                  | PARAMETER_CONTROL_WORD__OBJ_CONTROL__GOURAUD;

  polygon->isp_tsp_instruction_word = ISP_TSP_INSTRUCTION_WORD__DEPTH_COMPARE_MODE__GREATER
                                    | ISP_TSP_INSTRUCTION_WORD__CULLING_MODE__NO_CULLING;
  // Note that it is not possible to use
  // ISP_TSP_INSTRUCTION_WORD__GOURAUD_SHADING in this isp_tsp_instruction_word,
  // because `gouraud` is one of the bits overwritten by the value in
  // parameter_control_word. See DCDBSysArc990907E.pdf page 200.

  polygon->tsp_instruction_word = TSP_INSTRUCTION_WORD__SRC_ALPHA_INSTR__ONE
                                | TSP_INSTRUCTION_WORD__DST_ALPHA_INSTR__ZERO
                                | TSP_INSTRUCTION_WORD__FOG_CONTROL__NO_FOG;

  polygon->texture_control_word = 0;

  polygon->data_size_for_sort_dma = 0;
  polygon->next_address_for_sort_dma = 0;

  // start store queue transfer of `polygon` to the TA
  pref(store_queue_ix);

  store_queue_ix += (sizeof (ta_global_parameter__polygon_type_0));

  return store_queue_ix;
}

/*
  These vertex and face definitions are a trivial transformation of the default
  Blender cube, as exported by the .obj exporter (with triangulation enabled).
 */
typedef struct vec3 {
  float x;
  float y;
  float z;
} vec3;

static const vec3 cube_vertex_position[] = {
  {  1.0f,  1.0f, -1.0f },
  {  1.0f, -1.0f, -1.0f },
  {  1.0f,  1.0f,  1.0f },
  {  1.0f, -1.0f,  1.0f },
  { -1.0f,  1.0f, -1.0f },
  { -1.0f, -1.0f, -1.0f },
  { -1.0f,  1.0f,  1.0f },
  { -1.0f, -1.0f,  1.0f },
};

static const uint32_t cube_vertex_color[] = {
  0xff0000, // red
  0x00ff00, // green
  0x0000ff, // blue
  0xffff00, // yellow
  0x00ffff, // cyan
  0xff00ff, // magenta
  0xff7f00, // orange
  0x7f00ff, // violet
};

typedef struct face {
  int a;
  int b;
  int c;
} face;

/*
  It is also possible to submit each cube face as a 4-vertex triangle strip, or
  submit the entire cube as a single triangle strip.

  Separate 3-vertex triangles are chosen to make this example more
  straightforward, but this is not the best approach if high performance is
  desired.
 */

static const face cube_faces[] = {
  {4, 2, 0},
  {2, 7, 3},
  {6, 5, 7},
  {1, 7, 5},
  {0, 3, 1},
  {4, 1, 5},
  {4, 6, 2},
  {2, 6, 7},
  {6, 4, 5},
  {1, 3, 7},
  {0, 2, 3},
  {4, 0, 1},
};
static const int cube_faces_length = (sizeof (cube_faces)) / (sizeof (cube_faces[0]));

#define cos(n) __builtin_cosf(n)
#define sin(n) __builtin_sinf(n)

float theta = 0.7853981633974483f; // pi / 4

static inline vec3 vertex_rotate(vec3 v)
{
  // to make the cube's appearance more interesting, rotate the vertex on two
  // axes

  float x0 = v.x;
  float y0 = v.y;
  float z0 = v.z;

  float x1 = x0 * cos(theta) - z0 * sin(theta);
  float y1 = y0;
  float z1 = x0 * sin(theta) + z0 * cos(theta);

  float x2 = x1;
  float y2 = y1 * cos(theta) - z1 * sin(theta);
  float z2 = y1 * sin(theta) + z1 * cos(theta);

  return (vec3){x2, y2, z2};
}

static inline vec3 vertex_perspective_divide(vec3 v)
{
  float w = 1.0f / (v.z + 3.0f);
  return (vec3){v.x * w, v.y * w, w};
}

static inline vec3 vertex_screen_space(vec3 v)
{
  return (vec3){
    v.x * 16.f + 16.f,
    v.y * 16.f + 16.f,
    v.z,
  };
}

void transfer_ta_cube()
{
  // set the store queue destination address to the TA Polygon Converter FIFO
  *SH7091__CCN__QACR0 = ((ta_polygon_converter_fifo >> 24) & 0b11100);
  *SH7091__CCN__QACR1 = ((ta_polygon_converter_fifo >> 24) & 0b11100);

  uint32_t store_queue_ix = store_queue;

  // See sh7091pm_e.pdf, printed page 79:
  //
  // > While the contents of one SQ are being transferred to external memory,
  // > the other SQ can be written to without a penalty cycle, but writing to
  // > the SQ involved in the transfer to external memory is deferred until the
  // > transfer is completed.
  //
  // The reason for incrementing store_queue_ix is that it is a cheap way to
  // track which store queue is the most/least recently used--encoded in bit 5
  // of the store queue address.

  store_queue_ix = transfer_ta_global_polygon(store_queue_ix);

  for (int face_ix = 0; face_ix < cube_faces_length; face_ix++) {
    int ia = cube_faces[face_ix].a;
    int ib = cube_faces[face_ix].b;
    int ic = cube_faces[face_ix].c;

    vec3 va = vertex_screen_space(
                vertex_perspective_divide(
                  vertex_rotate(cube_vertex_position[ia])));

    vec3 vb = vertex_screen_space(
                vertex_perspective_divide(
                  vertex_rotate(cube_vertex_position[ib])));

    vec3 vc = vertex_screen_space(
                vertex_perspective_divide(
                  vertex_rotate(cube_vertex_position[ic])));

    uint32_t va_color = cube_vertex_color[ia];
    uint32_t vb_color = cube_vertex_color[ib];
    uint32_t vc_color = cube_vertex_color[ic];

    store_queue_ix = transfer_ta_vertex_triangle(store_queue_ix,
                                                 va.x, va.y, va.z, va_color,
                                                 vb.x, vb.y, vb.z, vb_color,
                                                 vc.x, vc.y, vc.z, vc_color);
  }

  store_queue_ix = transfer_ta_global_end_of_list(store_queue_ix);
}

/******************************************************************************
 Holly register definitions
 ******************************************************************************/

volatile uint32_t * SOFTRESET     = (volatile uint32_t *)(0xa05f8000 + 0x08);
volatile uint32_t * STARTRENDER   = (volatile uint32_t *)(0xa05f8000 + 0x14);
volatile uint32_t * PARAM_BASE    = (volatile uint32_t *)(0xa05f8000 + 0x20);
volatile uint32_t * REGION_BASE   = (volatile uint32_t *)(0xa05f8000 + 0x2c);
volatile uint32_t * FB_R_SOF1     = (volatile uint32_t *)(0xa05f8000 + 0x50);
volatile uint32_t * FB_W_SOF1     = (volatile uint32_t *)(0xa05f8000 + 0x60);
volatile uint32_t * ISP_BACKGND_T = (volatile uint32_t *)(0xa05f8000 + 0x8c);

volatile uint32_t * SPG_STATUS = (volatile uint32_t *)(0xa05f8000 + 0x10c);

#define SPG_STATUS__VSYNC (1 << 13)

volatile uint32_t * TA_OL_BASE        = (volatile uint32_t *)(0xa05f8000 + 0x124);
volatile uint32_t * TA_ISP_BASE       = (volatile uint32_t *)(0xa05f8000 + 0x128);
volatile uint32_t * TA_OL_LIMIT       = (volatile uint32_t *)(0xa05f8000 + 0x12c);
volatile uint32_t * TA_ISP_LIMIT      = (volatile uint32_t *)(0xa05f8000 + 0x130);
volatile uint32_t * TA_GLOB_TILE_CLIP = (volatile uint32_t *)(0xa05f8000 + 0x13c);
volatile uint32_t * TA_ALLOC_CTRL     = (volatile uint32_t *)(0xa05f8000 + 0x140);
volatile uint32_t * TA_LIST_INIT      = (volatile uint32_t *)(0xa05f8000 + 0x144);

#define TA_GLOB_TILE_CLIP__TILE_Y_NUM(n) (((n) & 0xf) << 16)
#define TA_GLOB_TILE_CLIP__TILE_X_NUM(n) (((n) & 0x1f) << 0)
#define TA_ALLOC_CTRL__OPB_MODE__INCREASING_ADDRESSES (0 << 20)
#define TA_ALLOC_CTRL__O_OPB__8X4BYTE (1 << 0)
#define TA_LIST_INIT__LIST_INIT (1 << 31)

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

  // background_offset is relative to the beginning of isp_tsp_parameter_start
  uint32_t background_offset     = (sizeof (isp_tsp_parameter__polygon)) * 0;

  transfer_region_array(region_array_start, opaque_list_pointer);

  transfer_isp_tsp_background_parameter(isp_tsp_parameter_start);

  //////////////////////////////////////////////////////////////////////////////
  // configure the TA
  //////////////////////////////////////////////////////////////////////////////

  const int tile_y_num = 1;
  const int tile_x_num = 1;

  // TA_GLOB_TILE_CLIP restricts which "object pointer blocks" are written
  // to.
  //
  // This can also be used to implement "windowing", as long as the desired
  // window size happens to be a multiple of 32 pixels. The "User Tile Clip" TA
  // control parameter can also ~equivalently be used as many times as desired
  // within a single TA initialization to produce an identical effect.
  //
  // See DCDBSysArc990907E.pdf page 183.
  *TA_GLOB_TILE_CLIP = TA_GLOB_TILE_CLIP__TILE_Y_NUM(tile_y_num - 1)
                     | TA_GLOB_TILE_CLIP__TILE_X_NUM(tile_x_num - 1);

  // While CORE supports arbitrary-length object lists, the TA uses "object
  // pointer blocks" as a memory allocation strategy. These fixed-length blocks
  // can still have infinite length via "object pointer block links". This
  // mechanism is illustrated in DCDBSysArc990907E.pdf page 188.
  *TA_ALLOC_CTRL = TA_ALLOC_CTRL__OPB_MODE__INCREASING_ADDRESSES
                 | TA_ALLOC_CTRL__O_OPB__8X4BYTE;

  // While building object lists, the TA contains an internal index (exposed as
  // the read-only TA_ITP_CURRENT) for the next address that new ISP/TSP will be
  // stored at. The initial value of this index is TA_ISP_BASE.
  *TA_ISP_BASE = isp_tsp_parameter_start + (sizeof (isp_tsp_parameter__polygon)) * 1;
  *TA_ISP_LIMIT = isp_tsp_parameter_start + 0x100000;

  // Similarly, the TA also contains, for up to 600 tiles, an internal index for
  // the next address that an object list entry will be stored for each
  // tile. These internal indicies are partially exposed via the read-only
  // TA_OL_POINTERS.
  *TA_OL_BASE = object_list_start;

  // TA_OL_LIMIT, DCDBSysArc990907E.pdf page 385:
  //
  // >   Because the TA may automatically store data in the address that is
  // >   specified by this register, it must not be used for other data.  For
  // >   example, the address specified here must not be the same as the address
  // >   in the TA_ISP_BASE register.
  *TA_OL_LIMIT = object_list_start + 0x100000 - 32;

  //////////////////////////////////////////////////////////////////////////////
  // configure CORE
  //////////////////////////////////////////////////////////////////////////////

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

  // without waiting for rendering to actually complete, immediately display the
  // framebuffer.
  *FB_R_SOF1 = framebuffer_start;

  //////////////////////////////////////////////////////////////////////////////
  // animated drawing
  //////////////////////////////////////////////////////////////////////////////

  // draw 500 frames of cube rotation
  for (int i = 0; i < 500; i++) {
    //////////////////////////////////////////////////////////////////////////////
    // transfer cube to texture memory via the TA polygon converter FIFO
    //////////////////////////////////////////////////////////////////////////////

    // TA_LIST_INIT needs to be written (every frame) prior to the first FIFO
    // write.
    *TA_LIST_INIT = TA_LIST_INIT__LIST_INIT;

    // dummy TA_LIST_INIT read; DCDBSysArc990907E.pdf in multiple places says this
    // step is required.
    (void)*TA_LIST_INIT;

    transfer_ta_cube();

    //////////////////////////////////////////////////////////////////////////////
    // start the actual rasterization
    //////////////////////////////////////////////////////////////////////////////

    // start the actual render--the rendering process begins by interpreting the
    // region array
    *STARTRENDER = 1;

    // wait for vertical synchronization
    while (!((*SPG_STATUS) & SPG_STATUS__VSYNC));
    while (((*SPG_STATUS) & SPG_STATUS__VSYNC));

    // increment theta for the cube rotation animation
    // (used by the `vertex_rotate` function)
    theta += 0.01f;
  }

  // return from main; this will effectively jump back to the serial loader
}
