#include <stdint.h>

//
// SH4 SCIF definitions
//

// SH4 on-chip peripheral module control
volatile uint8_t * SCFTDR2 = (volatile uint8_t *)(0xffe80000 + 0x0c);
volatile uint16_t * SCFSR2 = (volatile uint16_t *)(0xffe80000 + 0x10);
volatile uint16_t * SCFCR2 = (volatile uint16_t *)(0xffe80000 + 0x18);

#define SCFSR2__TDFE (1 << 5)
#define SCFSR2__TEND (1 << 6)

#define SCFCR2__TTRG(n) (((n) & 0b11) << 4)

static inline void character(const char c)
{
  // wait for transmit fifo to become partially empty
  while ((*SCFSR2 & SCFSR2__TDFE) == 0);

  // unset TDFE bit
  *SCFSR2 &= ~SCFSR2__TDFE;

  *SCFTDR2 = c;
}

static inline void string(const char * s)
{
  while (*s != 0) {
    character(*s++);
  }
}

void print_base16(uint32_t n, int len)
{
  char buf[len];
  char * bufi = &buf[len - 1];

  while (bufi >= buf) {
    uint32_t nib = n & 0xf;
    n = n >> 4;
    if (nib > 9) {
      nib += (97 - 10);
    } else {
      nib += (48 - 0);
    }

    *bufi = nib;
    bufi -= 1;
  }

  for (int i = 0; i < len; i++) {
    character(buf[i]);
  }
}


//
// Maple protocol definitions
//

/*
  The Maple bus controller is a functional unit inside Holly. The only way to
  interact with it is via "Maple DMA"--a special DMA unit only used for
  interactions with the Maple controller.

  The "Maple DMA" unit is really more of a "FIFO", similar to the TA, except the
  Maple controller has a hard dependency on SH4 DMA (unlike the TA which also
  allows non-DMA transfers).

  The structure of the transmit buffer given to the Maple DMA unit is always:

    [ host instruction ]
      [ recieve data address ]
      [ protocol data ]
        [ command code ]
        [ destination ap ]
        [ source ap ]
        [ data size ]
        [ (variable-length data) ]
    [ host instruction ]
      [ recieve data address ]
      [ protocol data ]
        ...

    Each "host instruction" identifies the Maple port/destination that the
    following "Maple protocol data" is sent to. There can be multiple host
    instructions in a single Maple DMA buffer.

    The "recieve data address" is the System Memory address where you want the
    reply to your Maple command to be stored.

    See DCDBSysArc990907E.pdf page 276.

    See also MAPLE82E.pdf page 20 -- this shows the Maple bus from the
    perspective of (e.g) a Dreamcast Controller.
 */

typedef struct maple_protocol_header {
    uint8_t command_code;
    uint8_t destination_ap;
    uint8_t source_ap;
    uint8_t data_size;
} maple_protocol_header;

typedef struct maple_host_command {
  uint32_t host_instruction;     // interpreted by the Maple DMA controller
  uint32_t receive_data_address; // interpreted by the Maple DMA controller

  maple_protocol_header protocol_header;

  // variable-length "parameter" data follows
} maple_host_command;

// Maple Host Instruction : DCDBSysArc990907E.pdf page 269
#define MAPLE__HOST_INSTRUCTION__END_FLAG (1 << 31)
#define MAPLE__HOST_INSTRUCTION__PORT_SELECT__A (0 << 16)
#define MAPLE__HOST_INSTRUCTION__PORT_SELECT__B (1 << 16)
#define MAPLE__HOST_INSTRUCTION__PORT_SELECT__C (2 << 16)
#define MAPLE__HOST_INSTRUCTION__PORT_SELECT__D (3 << 16)
#define MAPLE__HOST_INSTRUCTION__PATTERN__NORMAL (0b000 << 8)
#define MAPLE__HOST_INSTRUCTION__TRANSFER_LENGTH(n) (((n) & 0xff) << 0)

// (supported) command codes are defined per-peripheral; there is also general
// information about command codes in MAPLE82E.pdf page 72.

// "AP" is defined in MAPLE82E.pdf page 21

#define MAPLE__AP__PORT_SELECT__A (0b00 << 6)
#define MAPLE__AP__PORT_SELECT__B (0b01 << 6)
#define MAPLE__AP__PORT_SELECT__C (0b10 << 6)
#define MAPLE__AP__PORT_SELECT__D (0b11 << 6)

// "device" roughly means "the thing directly connected to the Dreamcast"
//
// "expansion device" roughly means "the thing with some indirect connection to
// the Dreamcast (e.g: a VMU)"
//
// "port" is used to indicate that the AP is the Dreamcast itself (via that
// port)
#define MAPLE__AP__DE__DEVICE (1 << 5)
#define MAPLE__AP__DE__EXPANSION_DEVICE (0 << 5)
#define MAPLE__AP__DE__PORT (0 << 5)

// Ft0_090e.pdf describes the commands supported by Dreamcast Controllers
// specifically.
//
// The simplest command supported by a Dreamcast Controller is the "Device
// Request" command. The reply to a "Device Request" command is "Device Status".
#define MAPLE__COMMAND_CODE__DEVICE_REQUEST (0x01)
#define MAPLE__COMMAND_CODE__DEVICE_STATUS (0x05)

// A Maple DMA response is simply the Maple protocol data sent by the target/AP in
// response to the command

typedef struct maple_host_reply {
  maple_protocol_header protocol_header;

  // variable-length "parameter" data follows
} maple_host_reply;

// the Device Request command has no fields

// the Device Status reply has these fields:

typedef struct maple_device_id {
  uint32_t ft;
  uint32_t fd[3];
} maple_device_id;

typedef struct maple_device_status {
  maple_device_id device_id;
  uint8_t destination_code;
  uint8_t connection_direction;
  uint8_t product_name[30];
  uint8_t license[60];
  uint16_t low_consumption_standby_current;
  uint16_t maximum_current_consumption;
} maple_device_status;

/*
  maple_device_status is concatenated after maple_host_reply, as in:

  struct {
    maple_host_reply host_reply;
    maple_device_status device_status;
  };

  This ^ is the structure that will be stored at `recieve_data_address`.
*/

//
// Maple interface definitions
//

// system bus interface
volatile uint32_t * ISTNRM = (volatile uint32_t *)(0xa05f6800 + 0x100);

#define ISTNRM__END_OF_DMA_MAPLE_DMA (1 << 12)

// Maple interface
volatile uint32_t * MDSTAR = (volatile uint32_t *)(0xa05f6c00 + 0x04);
volatile uint32_t * MDTSEL = (volatile uint32_t *)(0xa05f6c00 + 0x10);
volatile uint32_t * MDEN   = (volatile uint32_t *)(0xa05f6c00 + 0x14);
volatile uint32_t * MDST   = (volatile uint32_t *)(0xa05f6c00 + 0x18);
volatile uint32_t * MSYS   = (volatile uint32_t *)(0xa05f6c00 + 0x80);
volatile uint32_t * MDAPRO = (volatile uint32_t *)(0xa05f6c00 + 0x8c);

#define MDEN__DMA_ENABLE__ABORT (0 << 0)
#define MDEN__DMA_ENABLE__ENABLE (1 << 0)
#define MDST__START_STATUS (1 << 0)

#define MSYS__TIME_OUT_COUNTER(n) (((n) & 0xffff) << 16)
#define MSYS__SENDING_RATE___2M (0 << 8)

#define MDAPRO__SECURITY_CODE (0x6155 << 16)
#define MDAPRO__TOP_ADDRESS(n) (((n) & 0x7f) << 8)
#define MDAPRO__BOTTOM_ADDRESS(n) (((n) & 0x7f) << 0)

#define MDTSEL__TRIGGER_SELECT__SOFTWARE_INITIATION (0 << 0)

#define MDSTAR__TABLE_ADDRESS(n) (((n) & 0xfffffe0) << 0)

/*
  See DCDBSysArc990907E.pdf page 274.

  Maple DMA uses SH4 DMA channel 0 in "DDT" mode. This means that Holly's Maple
  DMA functional unit is externally controlling the SH4's DMA unit.

  The Dreamcast boot rom leaves channel 0 already configured for Maple DMA,
  though if you felt rebellious and wished to used channel 0 for something else,
  you would need to reconfigure channel 0 for Maple/DDT afterwards.

  Note that this `maple_dma_start` function does not configure SH4 DMA channel
  0, and presumes it is already in the correct state.
 */
void maple_dma_start(void * command_buf)
{
  // if SH4 cache were enabled, it would be necessary to use the `ocbwb` sh4
  // instruction to write back the cache to system memory, prior to continuing.

  // if SH4 cache were enabled, it would be necessary to use the `ocbi` sh4
  // instruction to invalidate any possibly-cached areas of the recieve buffer,
  // as these are imminently going to be rewritten by the DMA unit independently
  // of cache access.

  // disable Maple DMA and abort any possibly-in-progress transfers
  *MDEN = MDEN__DMA_ENABLE__ABORT;
  while ((*MDST & MDST__START_STATUS) != 0);

  // clear Maple DMA end status
  *ISTNRM = ISTNRM__END_OF_DMA_MAPLE_DMA;

  // 20nsec * 0xc350 = 1ms
  uint32_t one_msec = 0xc350;
  // set the Maple bus controller timeout and transfer rate
  *MSYS = MSYS__TIME_OUT_COUNTER(one_msec)
        | MSYS__SENDING_RATE___2M;

  // MDAPRO controls which (system memory) addresses are considered valid for
  // Maple DMA. An attempt to use an address outside of this range will cause an
  // error interrupt in from the Maple DMA unit.
  //
  // 0x40 through 0x7F allows for transfers from 0x0c000000 to 0x0fffffff
  // System Memory is 0x0c000000 through 0x0cffffff (16MB)
  //
  // It is probably possible to do strange things such as use texture memory as
  // a Maple DMA recieve buffer, but TOP_ADDRESS would need to be lowered
  // accordingly.
  *MDAPRO = MDAPRO__SECURITY_CODE
          | MDAPRO__TOP_ADDRESS(0x40)
          | MDAPRO__BOTTOM_ADDRESS(0x7f);

  // SOFTWARE_INITIATION allows a Maple DMA transfer to be initiated by a write
  // to the MDST register.
  *MDTSEL = MDTSEL__TRIGGER_SELECT__SOFTWARE_INITIATION;

  // the Maple DMA start address must be 32-byte aligned
  *MDSTAR = MDSTAR__TABLE_ADDRESS((uint32_t)command_buf);

  // re-enable Maple DMA
  *MDEN = MDEN__DMA_ENABLE__ENABLE;

  // start Maple DMA (completes asynchronously)
  *MDST = MDST__START_STATUS;
}

void maple_dma_wait_complete()
{
  // wait for Maple DMA completion
  while ((*ISTNRM & ISTNRM__END_OF_DMA_MAPLE_DMA) == 0);
  // clear Maple DMA end status
  *ISTNRM = ISTNRM__END_OF_DMA_MAPLE_DMA;
}

//
// Maple command buffer construction
//

void maple_device_request(uint8_t * send_buf, uint8_t * recv_buf)
{
  maple_host_command * host_command = (maple_host_command *)send_buf;

  host_command->host_instruction = MAPLE__HOST_INSTRUCTION__END_FLAG
                                 | MAPLE__HOST_INSTRUCTION__PORT_SELECT__A
                                 | MAPLE__HOST_INSTRUCTION__PATTERN__NORMAL
                                 | MAPLE__HOST_INSTRUCTION__TRANSFER_LENGTH(0);

  host_command->receive_data_address = (uint32_t)recv_buf;

  host_command->protocol_header.command_code = MAPLE__COMMAND_CODE__DEVICE_REQUEST;

  host_command->protocol_header.destination_ap = MAPLE__AP__PORT_SELECT__A
                                               | MAPLE__AP__DE__DEVICE;

  host_command->protocol_header.source_ap = MAPLE__AP__PORT_SELECT__A
                                          | MAPLE__AP__DE__PORT;

  host_command->protocol_header.data_size = 0 / 4;
}

void main()
{
  // set the transmit trigger to `1 byte`--this changes the behavior of TDFE
  *SCFCR2 = SCFCR2__TTRG(0b11);

  // Maple DMA buffers must be 32-byte aligned
  uint8_t send_buf[1024] __attribute__((aligned(32)));
  uint8_t recv_buf[1024] __attribute__((aligned(32)));

  // fill send_buf with a "device request" command
  // recv_buf is reply destination address
  maple_device_request(send_buf, recv_buf);

  maple_dma_start(send_buf);

  maple_dma_wait_complete();

  // decode the reply in recv_buf

  maple_host_reply * host_reply = (maple_host_reply *)recv_buf;

  if (host_reply->protocol_header.command_code != MAPLE__COMMAND_CODE__DEVICE_STATUS) {
    string("maple port A: invalid response or disconnected\n");
    return;
  }

  string("host_reply:\n");
  string("  protocol_header:\n");
  string("    command_code   : ");
  print_base16(host_reply->protocol_header.command_code, 2); character('\n');
  string("    destination_ap : ");
  print_base16(host_reply->protocol_header.destination_ap, 2); character('\n');
  string("    source_ap      : ");
  print_base16(host_reply->protocol_header.source_ap, 2); character('\n');
  string("    data_size      : ");
  print_base16(host_reply->protocol_header.data_size, 2); character('\n');

  maple_device_status * device_status = (maple_device_status *)(recv_buf + (sizeof (maple_host_reply)));

  // the Maple bus protocol is big endian, but the SH4 is running in little endian mode
  #define bswap32 __builtin_bswap32
  #define bswap16 __builtin_bswap16

  string("  device_status:\n");
  string("    device_id:\n");
  string("      ft   : ");
  print_base16(bswap32(device_status->device_id.ft), 8); character('\n');
  string("      fd[0]: ");
  print_base16(bswap32(device_status->device_id.fd[0]), 8); character('\n');
  string("      fd[1]: ");
  print_base16(bswap32(device_status->device_id.fd[1]), 8); character('\n');
  string("      fd[2]: ");
  print_base16(bswap32(device_status->device_id.fd[2]), 8); character('\n');
  string("    destination_code: ");
  print_base16(bswap32(device_status->destination_code), 2); character('\n');
  string("    connection_direction: ");
  print_base16(bswap32(device_status->connection_direction), 2); character('\n');
  string("    product_name: \"");
  for (int i = 0; i < 30; i++)
    character(device_status->product_name[i]);
  string("\"\n");
  string("    license: \"");
  for (int i = 0; i < 60; i++)
    character(device_status->license[i]);
  string("\"\n");
  string("    low_consumption_standby_current: ");
  print_base16(bswap16(device_status->low_consumption_standby_current), 4); character('\n');
  string("    maximum_current_consumption: ");
  print_base16(bswap16(device_status->maximum_current_consumption), 4); character('\n');

  // hack: "flush" the above characters before the serial loader resets the
  // serial interface
  //
  // Other methods to detect "transmit fifo is empty" appear to be
  // non-functional (including polling SCFDR2 or SCFSR2)
  //
  // It would also be appropriate to use an SH7091 timer here, to delay a
  // certain number of milliseconds, though that would require defining more
  // registers.
  string("  ");
}
