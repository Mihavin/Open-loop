#include "spi_encoder.h"

/* ---------------------------- Internal helpers --------------------------- */

#define SPI_ENCODER_CMD_RW_READ   1u
#define SPI_ENCODER_CMD_RW_WRITE  0u

/* Parity is even over bits[14:0]. Parity bit is bit[15]. */
static uint8_t spi_encoder_even_parity15(uint16_t v15)
{
    v15 &= 0x7FFFu;
    /* XOR-fold to compute parity (1 if odd number of ones) */
    v15 ^= v15 >> 8;
    v15 ^= v15 >> 4;
    v15 ^= v15 >> 2;
    v15 ^= v15 >> 1;
    return (uint8_t)(v15 & 1u);
}

static uint16_t spi_encoder_build_cmd(uint16_t addr, uint8_t rw)
{
    uint16_t cmd15 = (uint16_t)((addr & 0x3FFFu) | ((uint16_t)(rw & 1u) << 14));
    uint16_t par   = (uint16_t)spi_encoder_even_parity15(cmd15);
    return (uint16_t)((par << 15) | cmd15);
}

static uint16_t spi_encoder_build_data(uint16_t data14)
{
    uint16_t d15 = (uint16_t)(data14 & 0x3FFFu); /* bit14 is always 0 on MOSI data frame */
    uint16_t par = (uint16_t)spi_encoder_even_parity15(d15);
    return (uint16_t)((par << 15) | d15);
}

static void spi_encoder_parse_rx(uint16_t raw, spi_encoder_rx_t *out)
{
    if (!out) return;
    out->raw = raw;
    out->ef  = ((raw >> 14) & 1u) ? true : false;
    out->data14 = (uint16_t)(raw & 0x3FFFu);

    uint8_t p_rx  = (uint8_t)((raw >> 15) & 1u);
    uint8_t p_calc = spi_encoder_even_parity15((uint16_t)(raw & 0x7FFFu));
    out->parity_ok = (p_rx == p_calc);
}

static spi_encoder_status_t spi_encoder_xfer16(spi_encoder_t *dev, uint16_t tx_word, uint16_t *rx_word)
{
    if (!dev || !dev->spi_transfer || !dev->cs_low || !dev->cs_high) {
        return SPI_ENCODER_ERR_NULL;
    }

    uint8_t tx[2] = { (uint8_t)(tx_word >> 8), (uint8_t)(tx_word & 0xFFu) };
    uint8_t rx[2] = { 0u, 0u };

    dev->cs_low(dev->ctx);
    int rc = dev->spi_transfer(dev->ctx, tx, rx, 2);
    dev->cs_high(dev->ctx);

    if (dev->delay_us && dev->cs_high_delay_us) {
        dev->delay_us(dev->ctx, dev->cs_high_delay_us);
    }

    if (rc != 0) {
        return SPI_ENCODER_ERR_SPI;
    }

    if (rx_word) {
        *rx_word = (uint16_t)(((uint16_t)rx[0] << 8) | (uint16_t)rx[1]);
    }
    return SPI_ENCODER_OK;
}

/* ------------------------------- Public API ------------------------------ */

void spi_encoder_init(spi_encoder_t *dev,
                      void *ctx,
                      spi_encoder_spi_transfer_fn spi_transfer,
                      spi_encoder_gpio_fn cs_low,
                      spi_encoder_gpio_fn cs_high,
                      spi_encoder_delay_us_fn delay_us,
                      uint32_t cs_high_delay_us)
{
    if (!dev) return;
    dev->ctx = ctx;
    dev->spi_transfer = spi_transfer;
    dev->cs_low = cs_low;
    dev->cs_high = cs_high;
    dev->delay_us = delay_us;
    dev->cs_high_delay_us = cs_high_delay_us;
}

spi_encoder_status_t spi_encoder_nop(spi_encoder_t *dev)
{
    /* A NOP is simply a READ of address 0x0000. */
    uint16_t rx = 0;
    return spi_encoder_xfer16(dev, spi_encoder_build_cmd(AS5047P_REG_NOP, SPI_ENCODER_CMD_RW_READ), &rx);
}

spi_encoder_status_t spi_encoder_read_reg_ex(spi_encoder_t *dev, uint16_t addr, spi_encoder_rx_t *out)
{
    if (!dev || !out) return SPI_ENCODER_ERR_NULL;

    /* 1) send READ(addr) (response is for previous command; ignore) */
    uint16_t rx1 = 0;
    spi_encoder_status_t st = spi_encoder_xfer16(dev, spi_encoder_build_cmd(addr, SPI_ENCODER_CMD_RW_READ), &rx1);
    if (st != SPI_ENCODER_OK) {
        return st;
    }

    /* 2) send READ(NOP) and capture response = DATA(addr) */
    uint16_t rx2 = 0;
    st = spi_encoder_xfer16(dev, spi_encoder_build_cmd(AS5047P_REG_NOP, SPI_ENCODER_CMD_RW_READ), &rx2);
    if (st != SPI_ENCODER_OK) {
        return st;
    }

    spi_encoder_parse_rx(rx2, out);
    return SPI_ENCODER_OK;
}

spi_encoder_status_t spi_encoder_read_reg(spi_encoder_t *dev, uint16_t addr, uint16_t *out_data14)
{
    if (!dev || !out_data14) return SPI_ENCODER_ERR_NULL;

    spi_encoder_rx_t rx;
    spi_encoder_status_t st = spi_encoder_read_reg_ex(dev, addr, &rx);
    if (st != SPI_ENCODER_OK) return st;

    /*
     * Always return the received data field when the SPI transfer succeeded.
     *
     * - If parity_ok == false: the received word may be corrupted.
     * - If ef == true: the device reports an error in the *previous* command frame.
     */
    *out_data14 = rx.data14;

    if (!rx.parity_ok) {
        return SPI_ENCODER_ERR_PARITY;
    }
    if (rx.ef) {
        return SPI_ENCODER_ERR_EF;
    }

    return SPI_ENCODER_OK;
}

spi_encoder_status_t spi_encoder_write_reg(spi_encoder_t *dev, uint16_t addr, uint16_t data14)
{
    if (!dev) return SPI_ENCODER_ERR_NULL;

    /* 1) WRITE(addr) */
    uint16_t rx1 = 0;
    spi_encoder_status_t st = spi_encoder_xfer16(dev, spi_encoder_build_cmd(addr, SPI_ENCODER_CMD_RW_WRITE), &rx1);
    if (st != SPI_ENCODER_OK) return st;

    /* 2) DATA(data14) - while we send data, device returns old register content on MISO */
    uint16_t rx2 = 0;
    st = spi_encoder_xfer16(dev, spi_encoder_build_data(data14), &rx2);
    if (st != SPI_ENCODER_OK) return st;

    /* Check EF/parity on the returned word (old data frame). EF refers to previous command frame. */
    spi_encoder_rx_t r;
    spi_encoder_parse_rx(rx2, &r);

    if (!r.parity_ok) {
        return SPI_ENCODER_ERR_PARITY;
    }
    if (r.ef) {
        return SPI_ENCODER_ERR_EF;
    }

    return SPI_ENCODER_OK;
}

spi_encoder_status_t spi_encoder_write_reg_verify(spi_encoder_t *dev,
                                                  uint16_t addr,
                                                  uint16_t data14,
                                                  uint16_t *out_readback14)
{
    if (!dev) return SPI_ENCODER_ERR_NULL;

    spi_encoder_status_t st = spi_encoder_write_reg(dev, addr, data14);
    if (st != SPI_ENCODER_OK) return st;

    uint16_t rb = 0;
    st = spi_encoder_read_reg(dev, addr, &rb);
    if (st != SPI_ENCODER_OK) return st;

    if (out_readback14) {
        *out_readback14 = rb;
    }

    if ((rb & 0x3FFFu) != (data14 & 0x3FFFu)) {
        return SPI_ENCODER_ERR_VERIFY;
    }

    return SPI_ENCODER_OK;
}

spi_encoder_status_t spi_encoder_update_bits(spi_encoder_t *dev,
                                             uint16_t addr,
                                             uint16_t mask,
                                             uint16_t value)
{
    if (!dev) return SPI_ENCODER_ERR_NULL;

    mask  &= 0x3FFFu;
    value &= 0x3FFFu;

    uint16_t cur = 0;
    spi_encoder_status_t st = spi_encoder_read_reg(dev, addr, &cur);
    if (st != SPI_ENCODER_OK) return st;

    uint16_t next = (uint16_t)((cur & ~mask) | (value & mask));
    return spi_encoder_write_reg_verify(dev, addr, next, NULL);
}

spi_encoder_status_t spi_encoder_get_angle_com(spi_encoder_t *dev, uint16_t *angle14)
{
    return spi_encoder_read_reg(dev, AS5047P_REG_ANGLECOM, angle14);
}

spi_encoder_status_t spi_encoder_get_angle_unc(spi_encoder_t *dev, uint16_t *angle14)
{
    return spi_encoder_read_reg(dev, AS5047P_REG_ANGLEUNC, angle14);
}

spi_encoder_status_t spi_encoder_get_magnitude(spi_encoder_t *dev, uint16_t *mag14)
{
    return spi_encoder_read_reg(dev, AS5047P_REG_MAG, mag14);
}

spi_encoder_status_t spi_encoder_get_diaagc(spi_encoder_t *dev, uint16_t *diaagc)
{
    return spi_encoder_read_reg(dev, AS5047P_REG_DIAAGC, diaagc);
}

spi_encoder_status_t spi_encoder_read_errfl(spi_encoder_t *dev, uint16_t *errfl14)
{
    return spi_encoder_read_reg(dev, AS5047P_REG_ERRFL, errfl14);
}

spi_encoder_status_t spi_encoder_clear_errfl(spi_encoder_t *dev)
{
    uint16_t tmp = 0;
    spi_encoder_status_t st = spi_encoder_read_reg(dev, AS5047P_REG_ERRFL, &tmp);
    if (st == SPI_ENCODER_ERR_EF || st == SPI_ENCODER_ERR_PARITY) {
        /* Even if error flags occurred, the read still attempted; caller can decide what to do. */
        return st;
    }
    return st;
}

spi_encoder_status_t spi_encoder_get_zero_position(spi_encoder_t *dev, uint16_t *zpos14)
{
    if (!dev || !zpos14) return SPI_ENCODER_ERR_NULL;

    uint16_t zposm = 0;
    uint16_t zposl = 0;

    spi_encoder_status_t st = spi_encoder_read_reg(dev, AS5047P_REG_ZPOSM, &zposm);
    if (st != SPI_ENCODER_OK) return st;
    st = spi_encoder_read_reg(dev, AS5047P_REG_ZPOSL, &zposl);
    if (st != SPI_ENCODER_OK) return st;

    uint16_t msb8 = (uint16_t)(zposm & 0x00FFu);
    uint16_t lsb6 = (uint16_t)(zposl & 0x003Fu);

    *zpos14 = (uint16_t)((msb8 << 6) | lsb6);
    return SPI_ENCODER_OK;
}

spi_encoder_status_t spi_encoder_set_zero_position(spi_encoder_t *dev, uint16_t zpos14)
{
    if (!dev) return SPI_ENCODER_ERR_NULL;

    zpos14 &= 0x3FFFu;
    uint16_t msb8 = (uint16_t)((zpos14 >> 6) & 0x00FFu);
    uint16_t lsb6 = (uint16_t)(zpos14 & 0x003Fu);

    /* Preserve any upper/reserved bits by read-modify-write. */
    spi_encoder_status_t st;

    uint16_t cur_m = 0;
    st = spi_encoder_read_reg(dev, AS5047P_REG_ZPOSM, &cur_m);
    if (st != SPI_ENCODER_OK) return st;
    uint16_t next_m = (uint16_t)((cur_m & ~0x00FFu) | msb8);
    st = spi_encoder_write_reg_verify(dev, AS5047P_REG_ZPOSM, next_m, NULL);
    if (st != SPI_ENCODER_OK) return st;

    uint16_t cur_l = 0;
    st = spi_encoder_read_reg(dev, AS5047P_REG_ZPOSL, &cur_l);
    if (st != SPI_ENCODER_OK) return st;
    uint16_t next_l = (uint16_t)((cur_l & ~AS5047P_ZPOSL_LSB_Msk) | lsb6);
    st = spi_encoder_write_reg_verify(dev, AS5047P_REG_ZPOSL, next_l, NULL);
    if (st != SPI_ENCODER_OK) return st;

    return SPI_ENCODER_OK;
}

spi_encoder_status_t spi_encoder_get_settings1(spi_encoder_t *dev, uint16_t *settings1)
{
    return spi_encoder_read_reg(dev, AS5047P_REG_SETTINGS1, settings1);
}

spi_encoder_status_t spi_encoder_set_settings1(spi_encoder_t *dev, uint16_t settings1)
{
    return spi_encoder_write_reg_verify(dev, AS5047P_REG_SETTINGS1, settings1, NULL);
}

spi_encoder_status_t spi_encoder_get_settings2(spi_encoder_t *dev, uint16_t *settings2)
{
    return spi_encoder_read_reg(dev, AS5047P_REG_SETTINGS2, settings2);
}

spi_encoder_status_t spi_encoder_set_settings2(spi_encoder_t *dev, uint16_t settings2)
{
    return spi_encoder_write_reg_verify(dev, AS5047P_REG_SETTINGS2, settings2, NULL);
}

spi_encoder_status_t spi_encoder_set_direction(spi_encoder_t *dev, bool dir)
{
    return spi_encoder_update_bits(dev,
                                   AS5047P_REG_SETTINGS1,
                                   AS5047P_SETTINGS1_DIR_Msk,
                                   dir ? AS5047P_SETTINGS1_DIR_Msk : 0u);
}

spi_encoder_status_t spi_encoder_set_daec_disable(spi_encoder_t *dev, bool disable)
{
    return spi_encoder_update_bits(dev,
                                   AS5047P_REG_SETTINGS1,
                                   AS5047P_SETTINGS1_DAECDIS_Msk,
                                   disable ? AS5047P_SETTINGS1_DAECDIS_Msk : 0u);
}

spi_encoder_status_t spi_encoder_set_abi_binary(spi_encoder_t *dev, bool binary)
{
    return spi_encoder_update_bits(dev,
                                   AS5047P_REG_SETTINGS1,
                                   AS5047P_SETTINGS1_ABIBIN_Msk,
                                   binary ? AS5047P_SETTINGS1_ABIBIN_Msk : 0u);
}

spi_encoder_status_t spi_encoder_set_dataselect_cordic(spi_encoder_t *dev, bool cordic)
{
    return spi_encoder_update_bits(dev,
                                   AS5047P_REG_SETTINGS1,
                                   AS5047P_SETTINGS1_DATASELECT_Msk,
                                   cordic ? AS5047P_SETTINGS1_DATASELECT_Msk : 0u);
}

spi_encoder_status_t spi_encoder_set_pwm_on(spi_encoder_t *dev, bool on)
{
    return spi_encoder_update_bits(dev,
                                   AS5047P_REG_SETTINGS1,
                                   AS5047P_SETTINGS1_PWMON_Msk,
                                   on ? AS5047P_SETTINGS1_PWMON_Msk : 0u);
}

spi_encoder_status_t spi_encoder_comm_test(spi_encoder_t *dev,
                                          spi_encoder_comm_test_t *out,
                                          bool do_write_test)
{
    if (!dev) return SPI_ENCODER_ERR_NULL;

    spi_encoder_comm_test_t local;
    if (!out) {
        out = &local;
    }

    *out = (spi_encoder_comm_test_t){0};

    /* Flush pipeline and clear existing ERRFL */
    (void)spi_encoder_nop(dev);
    (void)spi_encoder_read_errfl(dev, &out->errfl_before);

    spi_encoder_status_t st;

    st = spi_encoder_get_diaagc(dev, &out->diaagc);
    if (st != SPI_ENCODER_OK) return st;

    st = spi_encoder_get_magnitude(dev, &out->mag);
    if (st != SPI_ENCODER_OK) return st;

    st = spi_encoder_get_angle_unc(dev, &out->angle_unc);
    if (st != SPI_ENCODER_OK) return st;

    st = spi_encoder_get_angle_com(dev, &out->angle_com);
    if (st != SPI_ENCODER_OK) return st;

    if (do_write_test) {
        out->write_test_performed = true;

        uint16_t s1 = 0;
        st = spi_encoder_get_settings1(dev, &s1);
        if (st != SPI_ENCODER_OK) return st;
        out->settings1_before = s1;

        /* Toggle DATASELECT (bit 6). */
        uint16_t toggled = (uint16_t)(s1 ^ AS5047P_SETTINGS1_DATASELECT_Msk);
        st = spi_encoder_write_reg_verify(dev, AS5047P_REG_SETTINGS1, toggled, &out->settings1_after_toggle);
        if (st != SPI_ENCODER_OK) {
            out->write_test_passed = false;
            return st;
        }

        /* Restore original */
        st = spi_encoder_write_reg_verify(dev, AS5047P_REG_SETTINGS1, s1, NULL);
        if (st != SPI_ENCODER_OK) {
            out->write_test_passed = false;
            return st;
        }

        out->write_test_passed = true;
    }

    /* Read ERRFL at the end (this also clears it) */
    (void)spi_encoder_read_errfl(dev, &out->errfl_after);

    return SPI_ENCODER_OK;
}
