#include "drv_spi.h"

// ---------- small, safe delays (for nSCS high time, etc.) ----------
static inline void drv_delay_nops(volatile uint32_t n)
{
  while (n--) { __NOP(); }
}

static inline void ncs_low(drv8323_spi_t *dev)
{
  HAL_GPIO_WritePin(dev->ncs_port, dev->ncs_pin, GPIO_PIN_RESET);
}

static inline void ncs_high(drv8323_spi_t *dev)
{
  HAL_GPIO_WritePin(dev->ncs_port, dev->ncs_pin, GPIO_PIN_SET);
}

// Build 16-bit SDI word: [15]=rw, [14:11]=addr, [10:0]=data
static inline uint16_t drv_build_frame(uint8_t rw, uint8_t addr, uint16_t data11)
{
  data11 &= 0x07FFu;
  return (uint16_t)(((rw & 0x1u) << 15) | ((addr & 0xFu) << 11) | data11);
}

// Extract 11-bit data from SDO word
static inline uint16_t drv_parse_sdo(uint16_t sdo16)
{
  return (uint16_t)(sdo16 & 0x07FFu);
}

static HAL_StatusTypeDef drv_xfer16(drv8323_spi_t *dev, uint16_t tx, uint16_t *rx)
{
  uint16_t r = 0;
  uint8_t tbuf[2] = { (uint8_t)(tx >> 8), (uint8_t)(tx & 0xFF) };
  uint8_t rbuf[2] = { 0, 0 };

  // nSCS low → shift 16 clocks → nSCS high
  ncs_low(dev);

  HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(dev->hspi, tbuf, rbuf, 2, dev->spi_timeout_ms);

  ncs_high(dev);

  // Datasheet: nSCS high at least 400 ns between words.
  // At 400MHz CPU clock, ~160 NOPs ≈ 400ns. Use a little more margin.
  drv_delay_nops(300);

  r = (uint16_t)((rbuf[0] << 8) | rbuf[1]);
  if (rx) *rx = r;

  return st;
}

void drv8323_cfg_set_defaults(drv8323_cfg_t *cfg)
{
  if (!cfg) return;
  cfg->reg2_driver_ctrl = (uint16_t)(DRV_DEFAULT_REG2 & 0x07FFu);
  cfg->reg3_gate_hs     = (uint16_t)(DRV_DEFAULT_REG3 & 0x07FFu);
  cfg->reg4_gate_ls     = (uint16_t)(DRV_DEFAULT_REG4 & 0x07FFu);
  cfg->reg5_ocp         = (uint16_t)(DRV_DEFAULT_REG5 & 0x07FFu);
  cfg->reg6_csa         = (uint16_t)(DRV_DEFAULT_REG6 & 0x07FFu);
  cfg->lock_after_init  = true;
}

HAL_StatusTypeDef drv8323_read_reg(drv8323_spi_t *dev, drv_reg_t reg, uint16_t *data11)
{
  if (!dev || !dev->hspi || !dev->ncs_port || !data11) return HAL_ERROR;

  uint16_t tx = drv_build_frame(DRV_SPI_RW_READ, (uint8_t)reg, 0);
  uint16_t rx = 0;

  HAL_StatusTypeDef st = drv_xfer16(dev, tx, &rx);
  if (st != HAL_OK) return st;

  *data11 = drv_parse_sdo(rx);
  return HAL_OK;
}

HAL_StatusTypeDef drv8323_write_reg(drv8323_spi_t *dev, drv_reg_t reg, uint16_t data11, uint16_t *prev_data11)
{
  if (!dev || !dev->hspi || !dev->ncs_port) return HAL_ERROR;

  uint16_t tx = drv_build_frame(DRV_SPI_RW_WRITE, (uint8_t)reg, data11);
  uint16_t rx = 0;

  HAL_StatusTypeDef st = drv_xfer16(dev, tx, &rx);
  if (st != HAL_OK) return st;

  if (prev_data11) *prev_data11 = drv_parse_sdo(rx);
  return HAL_OK;
}

static HAL_StatusTypeDef drv_unlock_regs(drv8323_spi_t *dev)
{
  // Write LOCK=011 to reg3 (Gate Drive HS) to unlock.
  // Keep the other fields as currently configured if possible: do a read first.
  uint16_t reg3 = 0;
  HAL_StatusTypeDef st = drv8323_read_reg(dev, DRV_REG_GATE_DRIVE_HS, &reg3);
  if (st != HAL_OK) return st;

  reg3 &= ~(0x7u << DRV_GDHS_LOCK_Pos);
  reg3 |=  (DRV_LOCK_UNLOCK_CODE << DRV_GDHS_LOCK_Pos);

  return drv8323_write_reg(dev, DRV_REG_GATE_DRIVE_HS, reg3, NULL);
}

static HAL_StatusTypeDef drv_lock_regs(drv8323_spi_t *dev)
{
  uint16_t reg3 = 0;
  HAL_StatusTypeDef st = drv8323_read_reg(dev, DRV_REG_GATE_DRIVE_HS, &reg3);
  if (st != HAL_OK) return st;

  reg3 &= ~(0x7u << DRV_GDHS_LOCK_Pos);
  reg3 |=  (DRV_LOCK_LOCK_CODE << DRV_GDHS_LOCK_Pos);

  return drv8323_write_reg(dev, DRV_REG_GATE_DRIVE_HS, reg3, NULL);
}

HAL_StatusTypeDef drv8323_clear_faults(drv8323_spi_t *dev)
{
  // CLR_FLT is bit0 in reg2 and auto-clears after being written.
  uint16_t reg2 = 0;
  HAL_StatusTypeDef st = drv8323_read_reg(dev, DRV_REG_DRIVER_CTRL, &reg2);
  if (st != HAL_OK) return st;

  reg2 |= (1u << DRV_DCTRL_CLR_FLT_Pos);
  return drv8323_write_reg(dev, DRV_REG_DRIVER_CTRL, reg2, NULL);
}

HAL_StatusTypeDef drv8323_init(drv8323_spi_t *dev, const drv8323_cfg_t *cfg)
{
  if (!dev || !cfg || !dev->hspi || !dev->ncs_port) return HAL_ERROR;

  // Optional: enable pin control
  if (dev->en_port)
  {
    // Bring ENABLE high (operating mode). Datasheet: tWAKE ~1ms typical.
    HAL_GPIO_WritePin(dev->en_port, dev->en_pin, GPIO_PIN_SET);
    HAL_Delay(2);
  }

  // Make sure nSCS starts high
  ncs_high(dev);
  drv_delay_nops(500);

  // Unlock before writing (recommended)
  HAL_StatusTypeDef st = drv_unlock_regs(dev);
  if (st != HAL_OK) return st;

  // Program registers (mask to 11 bits)
  st = drv8323_write_reg(dev, DRV_REG_DRIVER_CTRL,   (cfg->reg2_driver_ctrl & 0x07FFu), NULL);
  if (st != HAL_OK) return st;

  st = drv8323_write_reg(dev, DRV_REG_GATE_DRIVE_HS, (cfg->reg3_gate_hs     & 0x07FFu), NULL);
  if (st != HAL_OK) return st;

  st = drv8323_write_reg(dev, DRV_REG_GATE_DRIVE_LS, (cfg->reg4_gate_ls     & 0x07FFu), NULL);
  if (st != HAL_OK) return st;

  st = drv8323_write_reg(dev, DRV_REG_OCP_CTRL,      (cfg->reg5_ocp         & 0x07FFu), NULL);
  if (st != HAL_OK) return st;

  st = drv8323_write_reg(dev, DRV_REG_CSA_CTRL,      (cfg->reg6_csa         & 0x07FFu), NULL);
  if (st != HAL_OK) return st;

  // Clear any latched faults after configuration
  (void)drv8323_clear_faults(dev);

  if (cfg->lock_after_init)
  {
    st = drv_lock_regs(dev);
    if (st != HAL_OK) return st;
  }

  return HAL_OK;
}

HAL_StatusTypeDef drv8323_comm_test(drv8323_spi_t *dev, bool *pass)
{
  if (pass) *pass = false;
  if (!dev) return HAL_ERROR;

  // Read status regs (sanity; they’re RO)
  uint16_t fs1 = 0, fs2 = 0;
  HAL_StatusTypeDef st = drv8323_read_reg(dev, DRV_REG_FAULT_STATUS_1, &fs1);
  if (st != HAL_OK) return st;
  st = drv8323_read_reg(dev, DRV_REG_FAULT_STATUS_2, &fs2);
  if (st != HAL_OK) return st;

  // Pick a RW register to test: Gate Drive LS (0x04)
  // We'll:
  // 1) Read original
  // 2) Write a modified value (toggle one safe bit pattern inside 11-bit field)
  // 3) Read back and compare
  // 4) Restore original
  uint16_t orig = 0;
  st = drv8323_read_reg(dev, DRV_REG_GATE_DRIVE_LS, &orig);
  if (st != HAL_OK) return st;

  // Need unlocked regs to write (if locked)
  st = drv_unlock_regs(dev);
  if (st != HAL_OK) return st;

  // Create a test value: flip CBC bit (bit10) and also flip one IDRIVE nibble bit.
  uint16_t test = orig ^ (1u << DRV_GDLS_CBC_Pos) ^ (1u << DRV_GDLS_IDRIVEP_LS_Pos);
  test &= 0x07FFu;

  st = drv8323_write_reg(dev, DRV_REG_GATE_DRIVE_LS, test, NULL);
  if (st != HAL_OK) return st;

  uint16_t rb = 0;
  st = drv8323_read_reg(dev, DRV_REG_GATE_DRIVE_LS, &rb);
  if (st != HAL_OK) return st;

  // Restore
  (void)drv8323_write_reg(dev, DRV_REG_GATE_DRIVE_LS, orig, NULL);

  // Optional: lock back (safe)
  (void)drv_lock_regs(dev);

  bool ok = ((rb & 0x07FFu) == (test & 0x07FFu));
  if (pass) *pass = ok;

  return ok ? HAL_OK : HAL_ERROR;
}
