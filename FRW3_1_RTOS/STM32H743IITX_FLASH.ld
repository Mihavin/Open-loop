/*
******************************************************************************
**
**  Linker script for STM32H7 + TouchGFX with external QSPI assets
**
**  - Code and normal constants:     internal FLASH (0x08000000)
**  - TouchGFX bitmaps/fonts/texts:  external QSPI (0x90000000)
**
**  NOTE:
**    This expects TouchGFX Config.hpp to use sections:
**      "FontFlashSection", "FontSearchFlashSection",
**      "TextFlashSection", "ExtFlashSection"
**
******************************************************************************
*/

ENTRY(Reset_Handler)

/* Highest address of the user mode stack (end of AXI SRAM / RAM_D1) */
_estack = ORIGIN(RAM_D1) + LENGTH(RAM_D1);

/* Minimal heap/stack (you can tune these) */
_Min_Heap_Size = 0x200;  /* 512 bytes  */
_Min_Stack_Size = 0x400;  /* 1   KB     */

/* Memory regions */
MEMORY
{
  ITCMRAM (xrw) : ORIGIN = 0x00000000, LENGTH = 64K
  DTCMRAM (xrw) : ORIGIN = 0x20000000, LENGTH = 128K

  FLASH   (rx)  : ORIGIN = 0x08000000, LENGTH = 2048K  /* internal flash */
  RAM_D1  (xrw) : ORIGIN = 0x24000000, LENGTH = 320K   /* AXI SRAM */
  RAM_D2  (xrw) : ORIGIN = 0x30000000, LENGTH = 32K
  RAM_D3  (xrw) : ORIGIN = 0x38000000, LENGTH = 16K

  ExtFlash (rx) : ORIGIN = 0x90000000, LENGTH = 64M    /* QSPI mapped */
  SDRAM (xrw)   : ORIGIN = 0xC0000000, LENGTH = 8M
}

/* Sections */
SECTIONS
{
  /* -----------------------------------------------------------------------
   *  TouchGFX assets in external QSPI
   * -----------------------------------------------------------------------
   * These must match the section names used by Config.hpp:
   *   LOCATION_ATTRIBUTE("FontFlashSection")
   *   LOCATION_ATTRIBUTE("FontSearchFlashSection")
   *   LOCATION_ATTRIBUTE("TextFlashSection")
   *   LOCATION_ATTRIBUTE("ExtFlashSection")
   *
   * Each block also accepts optional names with a leading '.' just in case.
   */

  /* Generic external assets (bitmaps, etc.) */
  ExtFlashSection :
  {
    . = ALIGN(4);
    KEEP(*(ExtFlashSection))
    KEEP(*(ExtFlashSection.*))
    KEEP(*(.ExtFlashSection))
    KEEP(*(.ExtFlashSection.*))
    . = ALIGN(4);
  } >ExtFlash

  /* Fonts: glyph bitmap data + glyph tables */
  FontFlashSection :
  {
    . = ALIGN(4);
    KEEP(*(FontFlashSection))
    KEEP(*(FontFlashSection.*))
    KEEP(*(.FontFlashSection))
    KEEP(*(.FontFlashSection.*))
    . = ALIGN(4);
  } >ExtFlash

  /* Text database (multi-language string tables) */
  TextFlashSection :
  {
    . = ALIGN(4);
    KEEP(*(TextFlashSection))
    KEEP(*(TextFlashSection.*))
    KEEP(*(.TextFlashSection))
    KEEP(*(.TextFlashSection.*))
    . = ALIGN(4);
  } >ExtFlash

  /* Font search / kerning tables */
  FontSearchFlashSection :
  {
    . = ALIGN(4);
    KEEP(*(FontSearchFlashSection))
    KEEP(*(FontSearchFlashSection.*))
    KEEP(*(.FontSearchFlashSection))
    KEEP(*(.FontSearchFlashSection.*))
    . = ALIGN(4);
  } >ExtFlash

  /* -----------------------------------------------------------------------
   *  Internal FLASH (code, normal consts, C++ runtime tables)
   * ----------------------------------------------------------------------- */

  /* Vector table */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))   /* Startup code / interrupt vectors */
    . = ALIGN(4);
  } >FLASH

  /* Program code */
  .text :
  {
    . = ALIGN(4);
    *(.text)               /* .text sections (code) */
    *(.text*)              /* .text* sections (code) */
    *(.glue_7)             /* ARM <-> Thumb interwork */
    *(.glue_7t)
    *(.eh_frame)

    KEEP(*(.init))
    KEEP(*(.fini))

    . = ALIGN(4);
    _etext = .;            /* End of code */
  } >FLASH

  /* Read‑only data (constants, string literals, etc.) */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  /* ARM exception unwinding tables */
  .ARM.extab (READONLY) :
  {
    *(.ARM.extab* .gnu.linkonce.armextab.*)
  } >FLASH

  .ARM (READONLY) :
  {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  /* C/C++ runtime init arrays */
  .preinit_array (READONLY) :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH

  .init_array (READONLY) :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH

  .fini_array (READONLY) :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH

  /* -----------------------------------------------------------------------
   *  Initialized data in RAM (with LMA in internal FLASH)
   * ----------------------------------------------------------------------- */

  /* Used by startup to copy .data from FLASH to RAM */
  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;            /* Start of .data in RAM */

    *(.data)
    *(.data*)
    *(.RamFunc)
    *(.RamFunc*)

    . = ALIGN(4);
    _edata = .;            /* End of .data in RAM */
  } >RAM_D1 AT> FLASH      /* VMA in RAM_D1, LMA in FLASH */

  /* -----------------------------------------------------------------------
   *  Zero‑initialized data (BSS)
   * ----------------------------------------------------------------------- */

  . = ALIGN(4);
  .bss :
  {
    _sbss = .;             /* Start of .bss */
    __bss_start__ = _sbss;

    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(4);
    _ebss = .;             /* End of .bss */
    __bss_end__ = _ebss;
  } >RAM_D1

/* D2 SRAM (DMA1 target) */
.RAM_D2 (NOLOAD) :
{
    . = ALIGN(32);
    *(.RAM_D2)
    *(.RAM_D2*)
    . = ALIGN(32);
} >RAM_D2

/* D3 SRAM (BDMA target) */
.RAM_D3 (NOLOAD) :
{
    . = ALIGN(32);
    *(.RAM_D3)
    *(.RAM_D3*)
    . = ALIGN(32);
} >RAM_D3

  /* -----------------------------------------------------------------------
   *  Heap + stack reservation
   * ----------------------------------------------------------------------- */

  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE(end  = .);
    PROVIDE(_end = .);
    . = . + _Min_Heap_Size;
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM_D1

  /* -----------------------------------------------------------------------
   *  Discard unused lib sections and attributes
   * ----------------------------------------------------------------------- */

  /DISCARD/ :
  {
    libc.a  (*)
    libm.a  (*)
    libgcc.a (*)
  }

  .ARM.attributes 0 :
  {
    *(.ARM.attributes)
  }

  /* LTDC framebuffer in SDRAM */
  .framebuffer (NOLOAD) :
  {
    . = ALIGN(32);
    *(.framebuffer*)
    . = ALIGN(32);
  } >SDRAM
  
/* TouchGFX framebuffer in SDRAM */
.TouchGFX_Framebuffer (NOLOAD) :
{
  . = ALIGN(32);
  *(.TouchGFX_Framebuffer)
  *(.TouchGFX_Framebuffer.*)
  *(TouchGFX_Framebuffer)
  *(TouchGFX_Framebuffer.*)
  . = ALIGN(32);
} >SDRAM
}
