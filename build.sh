set -eux
sh4-none-elf-as --isa=sh4 --little start.s -o start.o
sh4-none-elf-gcc -std=gnu23 -mfsrra -mfsca -ffast-math -Og -m4-single-only -ml -ffreestanding -nostdlib -c "${1}.c"
sh4-none-elf-ld -T main.lds -o "${1}.elf" start.o "${1}.o"
sh4-none-elf-objcopy -O binary "${1}.elf" "${1}.bin"
