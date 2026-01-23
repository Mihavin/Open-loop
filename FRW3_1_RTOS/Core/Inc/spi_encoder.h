#ifndef SPI_ENCODER_H
#define SPI_ENCODER_H

/*
 * spi_encoder.h
 *
 * Minimal AS5047P SPI driver (register read/write + basic helpers).
 *
 * Requirements from AS5047P datasheet (summary):
 *  - SPI mode 1 (CPOL=0, CPHA=1), MSB first, <= 10 MHz.
 *  - 16-bit frames, even parity on bits[14:0].
 *  - Read data is returned on the *next* SPI frame (pipelined).
 *
 * This driver is platform-agnostic: you provide 3 callbacks:
 *  - spi_transfer(): full-duplex transfer of N bytes
 *  - cs_low()/cs_high(): control chip-select GPIO
 * Optionally you can provide delay_us() if you want to enforce an
 * inter-frame CS-high time (usually not needed).
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------- Register map ----------------------------- */

/* Volatile registers */
#define AS5047P_REG_NOP        0x0000u
#define AS5047P_REG_ERRFL      0x0001u
#define AS5047P_REG_PROG       0x0003u
#define AS5047P_REG_DIAAGC     0x3FFCu
#define AS5047P_REG_MAG        0x3FFDu
#define AS5047P_REG_ANGLEUNC   0x3FFEu
#define AS5047P_REG_ANGLECOM   0x3FFFu

/* Non-volatile (OTP) registers ("soft write" possible, lost on reset unless burned) */
#define AS5047P_REG_ZPOSM      0x0016u
#define AS5047P_REG_ZPOSL      0x0017u
#define AS5047P_REG_SETTINGS1  0x0018u
#define AS5047P_REG_SETTINGS2  0x0019u

/* ERRFL bits (AS5047P_REG_ERRFL) */
#define AS5047P_ERRFL_FRERR_Pos    0u
#define AS5047P_ERRFL_INVCOMM_Pos  1u
#define AS5047P_ERRFL_PARERR_Pos   2u

#define AS5047P_ERRFL_FRERR_Msk    (1u << AS5047P_ERRFL_FRERR_Pos)
#define AS5047P_ERRFL_INVCOMM_Msk  (1u << AS5047P_ERRFL_INVCOMM_Pos)
#define AS5047P_ERRFL_PARERR_Msk   (1u << AS5047P_ERRFL_PARERR_Pos)

/* SETTINGS1 bits (AS5047P_REG_SETTINGS1) */
#define AS5047P_SETTINGS1_FACTORY_Pos      0u /* read-only, factory pre-programmed */
#define AS5047P_SETTINGS1_NOTUSED_Pos      1u /* must not be overwritten */
#define AS5047P_SETTINGS1_DIR_Pos          2u
#define AS5047P_SETTINGS1_UVW_ABI_Pos      3u
#define AS5047P_SETTINGS1_DAECDIS_Pos      4u
#define AS5047P_SETTINGS1_ABIBIN_Pos       5u
#define AS5047P_SETTINGS1_DATASELECT_Pos   6u
#define AS5047P_SETTINGS1_PWMON_Pos        7u

#define AS5047P_SETTINGS1_DIR_Msk          (1u << AS5047P_SETTINGS1_DIR_Pos)
#define AS5047P_SETTINGS1_UVW_ABI_Msk      (1u << AS5047P_SETTINGS1_UVW_ABI_Pos)
#define AS5047P_SETTINGS1_DAECDIS_Msk      (1u << AS5047P_SETTINGS1_DAECDIS_Pos)
#define AS5047P_SETTINGS1_ABIBIN_Msk       (1u << AS5047P_SETTINGS1_ABIBIN_Pos)
#define AS5047P_SETTINGS1_DATASELECT_Msk   (1u << AS5047P_SETTINGS1_DATASELECT_Pos)
#define AS5047P_SETTINGS1_PWMON_Msk        (1u << AS5047P_SETTINGS1_PWMON_Pos)

/* SETTINGS2 fields (AS5047P_REG_SETTINGS2) */
#define AS5047P_SETTINGS2_UVWPP_Pos   0u
#define AS5047P_SETTINGS2_UVWPP_Msk   (0x7u << AS5047P_SETTINGS2_UVWPP_Pos)   /* bits 2:0 */

#define AS5047P_SETTINGS2_HYS_Pos     3u
#define AS5047P_SETTINGS2_HYS_Msk     (0x3u << AS5047P_SETTINGS2_HYS_Pos)     /* bits 4:3 */

#define AS5047P_SETTINGS2_ABIRES_Pos  5u
#define AS5047P_SETTINGS2_ABIRES_Msk  (0x7u << AS5047P_SETTINGS2_ABIRES_Pos)  /* bits 7:5 */

/* ZPOSL bits */
#define AS5047P_ZPOSL_LSB_Pos         0u
#define AS5047P_ZPOSL_LSB_Msk         (0x3Fu << AS5047P_ZPOSL_LSB_Pos) /* bits 5:0 */
#define AS5047P_ZPOSL_COMP_L_ERR_Pos  6u
#define AS5047P_ZPOSL_COMP_H_ERR_Pos  7u

#define AS5047P_ZPOSL_COMP_L_ERR_Msk  (1u << AS5047P_ZPOSL_COMP_L_ERR_Pos)
#define AS5047P_ZPOSL_COMP_H_ERR_Msk  (1u << AS5047P_ZPOSL_COMP_H_ERR_Pos)

/* ------------------------------- Driver API ------------------------------ */

typedef enum {
    SPI_ENCODER_OK = 0,
    SPI_ENCODER_ERR_NULL = -1,
    SPI_ENCODER_ERR_SPI = -2,
    SPI_ENCODER_ERR_PARITY = -3,
    SPI_ENCODER_ERR_EF = -4,
    SPI_ENCODER_ERR_VERIFY = -5,
    SPI_ENCODER_ERR_BAD_ARG = -6,
} spi_encoder_status_t;

/**
 * Full-duplex SPI transfer.
 *
 * Must transmit 'len' bytes from tx (can be NULL for 0x00) and read 'len' bytes into rx
 * (can be NULL to ignore).
 * Return 0 on success, non-zero on error.
 */
typedef int (*spi_encoder_spi_transfer_fn)(void *ctx, const uint8_t *tx, uint8_t *rx, size_t len);

typedef void (*spi_encoder_gpio_fn)(void *ctx);

typedef void (*spi_encoder_delay_us_fn)(void *ctx, uint32_t us);

typedef struct {
    void *ctx;
    spi_encoder_spi_transfer_fn spi_transfer;
    spi_encoder_gpio_fn cs_low;
    spi_encoder_gpio_fn cs_high;
    spi_encoder_delay_us_fn delay_us;      /* optional; can be NULL */
    uint32_t cs_high_delay_us;             /* optional; enforced between frames */
} spi_encoder_t;

typedef struct {
    uint16_t raw;        /* raw 16-bit word received */
    uint16_t data14;     /* bits[13:0] */
    bool     ef;         /* error flag bit (bit 14) */
    bool     parity_ok;  /* parity check result */
} spi_encoder_rx_t;

typedef struct {
    uint16_t errfl_before;
    uint16_t diaagc;
    uint16_t mag;
    uint16_t angle_unc;
    uint16_t angle_com;
    bool     write_test_performed;
    bool     write_test_passed;
    uint16_t settings1_before;
    uint16_t settings1_after_toggle;
    uint16_t errfl_after;
} spi_encoder_comm_test_t;

/** Initialize a driver instance. */
void spi_encoder_init(spi_encoder_t *dev,
                      void *ctx,
                      spi_encoder_spi_transfer_fn spi_transfer,
                      spi_encoder_gpio_fn cs_low,
                      spi_encoder_gpio_fn cs_high,
                      spi_encoder_delay_us_fn delay_us,
                      uint32_t cs_high_delay_us);

/**
 * Read a 14-bit register.
 *
 * Due to pipelined reads, this performs 2 SPI frames:
 *   1) READ(addr)
 *   2) READ(NOP)  -> returns DATA(addr)
 */
spi_encoder_status_t spi_encoder_read_reg(spi_encoder_t *dev, uint16_t addr, uint16_t *out_data14);

/** Same as spi_encoder_read_reg(), but returns EF/parity flags and raw word. */
spi_encoder_status_t spi_encoder_read_reg_ex(spi_encoder_t *dev, uint16_t addr, spi_encoder_rx_t *out);

/**
 * Write a 14-bit register ("soft write" for OTP registers).
 * This performs 2 SPI frames:
 *   1) WRITE(addr)
 *   2) DATA(data14)
 */
spi_encoder_status_t spi_encoder_write_reg(spi_encoder_t *dev, uint16_t addr, uint16_t data14);

/** Write then read-back verify. */
spi_encoder_status_t spi_encoder_write_reg_verify(spi_encoder_t *dev,
                                                  uint16_t addr,
                                                  uint16_t data14,
                                                  uint16_t *out_readback14);

/** Read-modify-write helper. */
spi_encoder_status_t spi_encoder_update_bits(spi_encoder_t *dev,
                                             uint16_t addr,
                                             uint16_t mask,
                                             uint16_t value);

/** Send a NOP read (useful to flush the pipeline). */
spi_encoder_status_t spi_encoder_nop(spi_encoder_t *dev);

/* Convenience getters */
spi_encoder_status_t spi_encoder_get_angle_com(spi_encoder_t *dev, uint16_t *angle14);
spi_encoder_status_t spi_encoder_get_angle_unc(spi_encoder_t *dev, uint16_t *angle14);
spi_encoder_status_t spi_encoder_get_magnitude(spi_encoder_t *dev, uint16_t *mag14);
spi_encoder_status_t spi_encoder_get_diaagc(spi_encoder_t *dev, uint16_t *diaagc);

/* Error register (ERRFL) note: reading ERRFL clears it. */
spi_encoder_status_t spi_encoder_read_errfl(spi_encoder_t *dev, uint16_t *errfl14);
spi_encoder_status_t spi_encoder_clear_errfl(spi_encoder_t *dev);

/* OTP config helpers (SOFT write only; does NOT perform OTP burn/programming). */
spi_encoder_status_t spi_encoder_get_zero_position(spi_encoder_t *dev, uint16_t *zpos14);
spi_encoder_status_t spi_encoder_set_zero_position(spi_encoder_t *dev, uint16_t zpos14);

spi_encoder_status_t spi_encoder_get_settings1(spi_encoder_t *dev, uint16_t *settings1);
spi_encoder_status_t spi_encoder_set_settings1(spi_encoder_t *dev, uint16_t settings1);

spi_encoder_status_t spi_encoder_get_settings2(spi_encoder_t *dev, uint16_t *settings2);
spi_encoder_status_t spi_encoder_set_settings2(spi_encoder_t *dev, uint16_t settings2);

/* Bit-level setters for common configuration */
spi_encoder_status_t spi_encoder_set_direction(spi_encoder_t *dev, bool dir);
spi_encoder_status_t spi_encoder_set_daec_disable(spi_encoder_t *dev, bool disable);
spi_encoder_status_t spi_encoder_set_abi_binary(spi_encoder_t *dev, bool binary);
spi_encoder_status_t spi_encoder_set_dataselect_cordic(spi_encoder_t *dev, bool cordic);
spi_encoder_status_t spi_encoder_set_pwm_on(spi_encoder_t *dev, bool on);

/**
 * Communication test:
 *  - Clears ERRFL
 *  - Reads DIAAGC, MAG, ANGLEUNC, ANGLECOM
 *  - Optional write test: toggles SETTINGS1.DATASELECT, verifies, then restores
 */
spi_encoder_status_t spi_encoder_comm_test(spi_encoder_t *dev,
                                          spi_encoder_comm_test_t *out,
                                          bool do_write_test);

/* Utility conversions */
static inline uint16_t spi_encoder_mask14(uint16_t v) { return (uint16_t)(v & 0x3FFFu); }
static inline float spi_encoder_angle14_to_deg(uint16_t angle14)
{
    return (float)(angle14 & 0x3FFFu) * (360.0f / 16384.0f);
}

#ifdef __cplusplus
}
#endif

#endif /* SPI_ENCODER_H */
