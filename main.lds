OUTPUT_FORMAT("elf32-shl", "elf32-shl", "elf32-shl")
MEMORY
{
  p2ram : ORIGIN = 0xac010000, LENGTH = 0xff0000
}

SECTIONS
{
  . = ORIGIN(p2ram);

  .text ALIGN(32) :
  {
    KEEP(*(.text.start));
    KEEP(*(.text*));
  } > p2ram
}
