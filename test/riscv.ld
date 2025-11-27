/*---------------------------------------------------------------------------
  Simple bare-metal RV64I linker script
  for use with: -march=rv64i -mabi=lp64 -nostdlib -T riscv.ld
---------------------------------------------------------------------------*/

OUTPUT_ARCH( "riscv" )
ENTRY(_start)

/*---------------------------------------------------------------------------
  Memory layout
---------------------------------------------------------------------------*/
MEMORY
{
    RAM (rxw) : ORIGIN = 0x00000000, LENGTH = 0x00100000  /* 1 MiB */
}

/*---------------------------------------------------------------------------
  Sections
---------------------------------------------------------------------------*/
SECTIONS
{
    /* Load code at beginning of RAM */
    . = ORIGIN(RAM);

    /* -------------------------------
       Text and read-only data
       ------------------------------- */
    .text : ALIGN(4)
    {
        PROVIDE(_text_start = .);
        *(.init)           /* startup.s */
        *(.text .text.*)
        *(.rodata .rodata.*)
        PROVIDE(_text_end = .);
    } > RAM

    /* -------------------------------
       Data (initialized globals)
       ------------------------------- */
    .data : ALIGN(8)
    {
        PROVIDE(_data_start = .);
        *(.data .data.*)
        PROVIDE(_data_end = .);
    } > RAM

    /* -------------------------------
       BSS (zero-initialized globals)
       ------------------------------- */
    .bss : ALIGN(8)
    {
        PROVIDE(_bss_start = .);
        *(.bss .bss.*)
        *(COMMON)
        PROVIDE(_bss_end = .);
    } > RAM

    /* -------------------------------
       Stack (grows downward)
       ------------------------------- */
    /* Reserve last 16 KB of RAM for stack */
    . = ORIGIN(RAM) + LENGTH(RAM);
    . -= 16K;
    PROVIDE(_stack_top = .);
}
