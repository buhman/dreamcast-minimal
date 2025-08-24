#include <stdint.h>

/*
  This example does not configure the SCIF, and presumes it is already
  configured for UART transmission.

  This is *not* the default SCIF state as initialized by the Dreamcast boot
  rom. However, the serial loader does not attempt to un-initialize the SCIF
  prior to jumping to system memory.
 */

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

void main()
{
  // set the transmit trigger to `1 byte`--this changes the behavior of TDFE
  *SCFCR2 = SCFCR2__TTRG(0b11);

  string("hello\n");

  // hack: "flush" the `hello` characters before the serial loader resets the
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
