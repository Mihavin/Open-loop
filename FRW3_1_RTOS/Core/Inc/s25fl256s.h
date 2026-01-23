/*
 * s25fl256s.h
 *
 * Minimal STM32H7 (HAL QSPI) driver for Infineon/Cypress/Spansion S25FL256S
 * (256 Mbit / 32 MByte) Serial NOR Flash.
 *
 * Target MCU: STM32H743IITx (QUADSPI peripheral)
 *
 * Notes:
 * - This driver uses 4-byte (32-bit) address opcodes (e.g. 0xEC, 0x12, 0x21, 0xDC)
 *   so you do NOT need to manage bank registers or EXTADD.
 * - Page size may be 256 or 512 bytes depending on the Ordering Part Number (OPN).
 *   For maximum compatibility the default is 256. If you know your device is 512-byte
 *   pages, you can change S25FL256S_PAGE_SIZE to 512.
 */

#pragma once

#include <stdint.h>
#include "stm32h7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Device geometry */
#define S25FL256S_FLASH_SIZE_BYTES      (32u * 1024u * 1024u) /* 256 Mbit */
#define S25FL256S_SUBSECTOR_SIZE        (4u * 1024u)          /* 4 KB */
#define S25FL256S_SECTOR_SIZE           (64u * 1024u)         /* 64 KB */

/* Default to 256 for maximum compatibility across OPNs */
#ifndef S25FL256S_PAGE_SIZE
#define S25FL256S_PAGE_SIZE             256u
#endif

/* STM32H7 QSPI memory-mapped base address */
#define S25FL256S_QSPI_BASE_ADDR        0x90000000u

/* If you want quad-input page program (0x34) set this to 1.
 * IMPORTANT: The QPP/4QPP commands are specified up to 80 MHz.
 * If your QSPI clock is higher, keep this disabled (0) or lower the QSPI prescaler.
 */
#ifndef S25FL256S_USE_QUAD_PROGRAM
#define S25FL256S_USE_QUAD_PROGRAM      1
#endif

/* Timeouts (ms) */
#ifndef S25FL256S_DEFAULT_TIMEOUT
#define S25FL256S_DEFAULT_TIMEOUT       1000u
#endif

#ifndef S25FL256S_PROGRAM_TIMEOUT
#define S25FL256S_PROGRAM_TIMEOUT       5000u
#endif

#ifndef S25FL256S_ERASE_4K_TIMEOUT
#define S25FL256S_ERASE_4K_TIMEOUT      20000u
#endif

#ifndef S25FL256S_ERASE_64K_TIMEOUT
#define S25FL256S_ERASE_64K_TIMEOUT     120000u
#endif

#ifndef S25FL256S_CHIP_ERASE_TIMEOUT
#define S25FL256S_CHIP_ERASE_TIMEOUT    600000u
#endif

typedef struct
{
    uint8_t manufacturer_id;
    uint8_t memory_type;
    uint8_t capacity;
} S25FL256S_JEDEC_ID;

/**
 * @brief Optional hardware reset (if you have the reset pin connected).
 *        If rst_port == NULL, a software reset sequence is used.
 */
HAL_StatusTypeDef S25FL256S_Reset(QSPI_HandleTypeDef *hqspi,
                                 GPIO_TypeDef *rst_port,
                                 uint16_t rst_pin);

/**
 * @brief Read JEDEC ID (RDID 0x9F)
 */
HAL_StatusTypeDef S25FL256S_ReadJEDECID(QSPI_HandleTypeDef *hqspi,
                                       S25FL256S_JEDEC_ID *id);

/**
 * @brief Initialize the flash for Quad I/O reads:
 *        - Clears error flags
 *        - Sets CR1.QUAD = 1
 *        - Sets CR1.LC   = 10b (safe for up to 104/133 MHz read modes)
 */
HAL_StatusTypeDef S25FL256S_Init(QSPI_HandleTypeDef *hqspi);

/**
 * @brief Indirect read using Quad I/O Read (4QIOR 0xEC).
 */
HAL_StatusTypeDef S25FL256S_Read(QSPI_HandleTypeDef *hqspi,
                                uint32_t address,
                                uint8_t *data,
                                uint32_t len);

/**
 * @brief Program (write) data. Splits transfers on page boundaries.
 *        Default uses 4PP (0x12, 1-1-1) for spec compliance at higher QSPI clocks.
 *        If S25FL256S_USE_QUAD_PROGRAM=1, uses 4QPP (0x34, 1-1-4).
 */
HAL_StatusTypeDef S25FL256S_Write(QSPI_HandleTypeDef *hqspi,
                                 uint32_t address,
                                 const uint8_t *data,
                                 uint32_t len);

/**
 * @brief Erase a 4KB parameter sector (4P4E 0x21).
 *        Note: On hybrid-sector devices, 4KB sectors exist only in the parameter region.
 */
HAL_StatusTypeDef S25FL256S_Erase4K(QSPI_HandleTypeDef *hqspi,
                                   uint32_t address);

/**
 * @brief Erase a 64KB sector (4SE 0xDC).
 */
HAL_StatusTypeDef S25FL256S_Erase64K(QSPI_HandleTypeDef *hqspi,
                                    uint32_t address);

/**
 * @brief Bulk erase (chip erase) (BE 0x60).
 */
HAL_StatusTypeDef S25FL256S_EraseChip(QSPI_HandleTypeDef *hqspi);

/**
 * @brief Enable QSPI memory-mapped mode using Quad I/O Read (4QIOR 0xEC).
 *        After this, the flash can be read at S25FL256S_QSPI_BASE_ADDR.
 */
HAL_StatusTypeDef S25FL256S_EnableMemoryMappedMode(QSPI_HandleTypeDef *hqspi);

/**
 * @brief Abort current QSPI operation and exit memory-mapped mode (if active).
 */
HAL_StatusTypeDef S25FL256S_Abort(QSPI_HandleTypeDef *hqspi);

/**
 * @brief Convenience accessor for the memory mapped address.
 */
static inline uint8_t *S25FL256S_MemoryMappedPtr(void)
{
    return (uint8_t *)S25FL256S_QSPI_BASE_ADDR;
}

#ifdef __cplusplus
}
#endif
