#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "stm32h7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================ DRV8323S SPI basics ============================
// SPI: 16 clocks per frame, MSB first.
// SDI frame: [15]=R/W, [14:11]=ADDR, [10:0]=DATA
// SDO frame: [15:11]=don't care, [10:0]=DATA (content of accessed register)
// Note: For a WRITE, the *response* contains the previous register contents.

// R/W bit
#define DRV_SPI_RW_WRITE          (0u)
#define DRV_SPI_RW_READ           (1u)

// Addresses (DRV8323S / DRV8323RS)
typedef enum
{
  DRV_REG_FAULT_STATUS_1   = 0x0, // RO
  DRV_REG_FAULT_STATUS_2   = 0x1, // RO
  DRV_REG_DRIVER_CTRL      = 0x2, // RW
  DRV_REG_GATE_DRIVE_HS    = 0x3, // RW
  DRV_REG_GATE_DRIVE_LS    = 0x4, // RW
  DRV_REG_OCP_CTRL         = 0x5, // RW
  DRV_REG_CSA_CTRL         = 0x6, // RW (DRV8323x only)
  DRV_REG_RSVD_7           = 0x7  // RW but "Reserved" (avoid writing)
} drv_reg_t;

// ============================ Bitfields (11-bit regs) ============================
// ---- REG 0x00 Fault Status 1 (RO) ----
#define DRV_FAULT1_VDS_LC_Pos     0
#define DRV_FAULT1_VDS_HC_Pos     1
#define DRV_FAULT1_VDS_LB_Pos     2
#define DRV_FAULT1_VDS_HB_Pos     3
#define DRV_FAULT1_VDS_LA_Pos     4
#define DRV_FAULT1_VDS_HA_Pos     5
#define DRV_FAULT1_OTSD_Pos       6
#define DRV_FAULT1_UVLO_Pos       7
#define DRV_FAULT1_GDF_Pos        8
#define DRV_FAULT1_VDS_OCP_Pos    9
#define DRV_FAULT1_FAULT_Pos      10

// ---- REG 0x01 Fault Status 2 (RO) ----
#define DRV_FAULT2_VGS_LC_Pos     0
#define DRV_FAULT2_VGS_HC_Pos     1
#define DRV_FAULT2_VGS_LB_Pos     2
#define DRV_FAULT2_VGS_HB_Pos     3
#define DRV_FAULT2_VGS_LA_Pos     4
#define DRV_FAULT2_VGS_HA_Pos     5
#define DRV_FAULT2_CPUV_Pos       6
#define DRV_FAULT2_OTW_Pos        7
#define DRV_FAULT2_SC_OC_Pos      8
#define DRV_FAULT2_SB_OC_Pos      9
#define DRV_FAULT2_SA_OC_Pos      10

// ---- REG 0x02 Driver Control (RW) ----
#define DRV_DCTRL_CLR_FLT_Pos     0
#define DRV_DCTRL_BRAKE_Pos       1
#define DRV_DCTRL_COAST_Pos       2
#define DRV_DCTRL_1PWM_DIR_Pos    3
#define DRV_DCTRL_1PWM_COM_Pos    4
#define DRV_DCTRL_PWM_MODE_Pos    5  // [6:5]
#define DRV_DCTRL_OTW_REP_Pos     7
#define DRV_DCTRL_DIS_GDF_Pos     8
#define DRV_DCTRL_DIS_CPUV_Pos    9
// bit10 reserved

typedef enum
{
  DRV_PWM_MODE_6X   = 0,
  DRV_PWM_MODE_3X   = 1,
  DRV_PWM_MODE_1X   = 2,
  DRV_PWM_MODE_INDEP= 3
} drv_pwm_mode_t;

// ---- REG 0x03 Gate Drive HS (RW) ----
#define DRV_GDHS_IDRIVEN_HS_Pos   0  // [3:0]
#define DRV_GDHS_IDRIVEP_HS_Pos   4  // [7:4]
#define DRV_GDHS_LOCK_Pos         8  // [10:8]

#define DRV_LOCK_UNLOCK_CODE      (0x3u) // 011b (unlocks all regs)
#define DRV_LOCK_LOCK_CODE        (0x6u) // 110b (locks; only LOCK bits + reg2[2:0] writable)

// ---- REG 0x04 Gate Drive LS (RW) ----
#define DRV_GDLS_IDRIVEN_LS_Pos   0  // [3:0]
#define DRV_GDLS_IDRIVEP_LS_Pos   4  // [7:4]
#define DRV_GDLS_TDRIVE_Pos       8  // [9:8]
#define DRV_GDLS_CBC_Pos          10

typedef enum { DRV_TDRIVE_500NS=0, DRV_TDRIVE_1000NS=1, DRV_TDRIVE_2000NS=2, DRV_TDRIVE_4000NS=3 } drv_tdrive_t;

// ---- REG 0x05 OCP Control (RW) ----
#define DRV_OCP_VDS_LVL_Pos       0  // [3:0]
#define DRV_OCP_OCP_DEG_Pos       4  // [5:4]
#define DRV_OCP_OCP_MODE_Pos      6  // [7:6]
#define DRV_OCP_DEAD_TIME_Pos     8  // [9:8]
#define DRV_OCP_TRETRY_Pos        10

typedef enum { DRV_OCP_DEG_2US=0, DRV_OCP_DEG_4US=1, DRV_OCP_DEG_6US=2, DRV_OCP_DEG_8US=3 } drv_ocp_deg_t;
typedef enum { DRV_OCP_MODE_LATCH=0, DRV_OCP_MODE_RETRY=1, DRV_OCP_MODE_REPORT=2, DRV_OCP_MODE_DISABLED=3 } drv_ocp_mode_t;
typedef enum { DRV_DEADTIME_50NS=0, DRV_DEADTIME_100NS=1, DRV_DEADTIME_200NS=2, DRV_DEADTIME_400NS=3 } drv_deadtime_t;

// ---- REG 0x06 CSA Control (RW) ----
#define DRV_CSA_SEN_LVL_Pos       0  // [1:0]
#define DRV_CSA_CSA_CAL_C_Pos     2
#define DRV_CSA_CSA_CAL_B_Pos     3
#define DRV_CSA_CSA_CAL_A_Pos     4
#define DRV_CSA_DIS_SEN_Pos       5
#define DRV_CSA_CSA_GAIN_Pos      6  // [7:6]
#define DRV_CSA_LS_REF_Pos        8
#define DRV_CSA_VREF_DIV_Pos      9
#define DRV_CSA_CSA_FET_Pos       10

typedef enum { DRV_CSA_GAIN_5=0, DRV_CSA_GAIN_10=1, DRV_CSA_GAIN_20=2, DRV_CSA_GAIN_40=3 } drv_csa_gain_t;
typedef enum { DRV_SEN_LVL_0P25=0, DRV_SEN_LVL_0P5=1, DRV_SEN_LVL_0P75=2, DRV_SEN_LVL_1P0=3 } drv_sen_lvl_t;

// ============================ Default register values ============================
// Conservative board bring-up policy, not the silicon reset defaults.
// 120 mA source / 240 mA sink; qualify switching and fault waveforms on hardware.
#define DRV_DEFAULT_REG2  (0u)                                           // Driver Control: all 0, PWM_MODE=00
#define DRV_DEFAULT_REG3  ((DRV_LOCK_UNLOCK_CODE<<DRV_GDHS_LOCK_Pos) |   \
                           (0x4u<<DRV_GDHS_IDRIVEP_HS_Pos) |             \
                           (0x4u<<DRV_GDHS_IDRIVEN_HS_Pos))
#define DRV_DEFAULT_REG4  ((0u<<DRV_GDLS_CBC_Pos) |                      \
                           (DRV_TDRIVE_4000NS<<DRV_GDLS_TDRIVE_Pos) |    \
                           (0x4u<<DRV_GDLS_IDRIVEP_LS_Pos) |             \
                           (0x4u<<DRV_GDLS_IDRIVEN_LS_Pos))
#define DRV_DEFAULT_REG5  ((0u<<DRV_OCP_TRETRY_Pos) |                    \
                           (DRV_DEADTIME_200NS<<DRV_OCP_DEAD_TIME_Pos) | \
                           (DRV_OCP_MODE_LATCH<<DRV_OCP_OCP_MODE_Pos) |  \
                           (DRV_OCP_DEG_2US<<DRV_OCP_OCP_DEG_Pos) |      \
                           (0x0u<<DRV_OCP_VDS_LVL_Pos))                  // 60 mV: qualify for switching false trips
#define DRV_DEFAULT_REG6  ((0u<<DRV_CSA_CSA_FET_Pos) |                   \
                           (1u<<DRV_CSA_VREF_DIV_Pos) |                  \
                           (0u<<DRV_CSA_LS_REF_Pos) |                    \
                           (DRV_CSA_GAIN_40<<DRV_CSA_CSA_GAIN_Pos) |     \
                           (0u<<DRV_CSA_DIS_SEN_Pos) |                   \
                           (0u<<DRV_CSA_CSA_CAL_A_Pos) |                 \
                           (0u<<DRV_CSA_CSA_CAL_B_Pos) |                 \
                           (0u<<DRV_CSA_CSA_CAL_C_Pos) |                 \
                           (DRV_SEN_LVL_0P25<<DRV_CSA_SEN_LVL_Pos))

// ============================ Driver object / config ============================
typedef struct
{
  SPI_HandleTypeDef *hspi;

  GPIO_TypeDef *ncs_port;
  uint16_t      ncs_pin;

  // Optional: if you want driver to control ENABLE pin (else leave NULL/0)
  GPIO_TypeDef *en_port;
  uint16_t      en_pin;

  uint32_t      spi_timeout_ms;
} drv8323_spi_t;

typedef struct
{
  uint16_t reg2_driver_ctrl;   // 11-bit payload
  uint16_t reg3_gate_hs;       // 11-bit payload
  uint16_t reg4_gate_ls;       // 11-bit payload
  uint16_t reg5_ocp;           // 11-bit payload
  uint16_t reg6_csa;           // 11-bit payload
  bool     lock_after_init;
} drv8323_cfg_t;

// ============================ API ============================
void     drv8323_cfg_set_defaults(drv8323_cfg_t *cfg);

HAL_StatusTypeDef drv8323_init(drv8323_spi_t *dev, const drv8323_cfg_t *cfg);
HAL_StatusTypeDef drv8323_verify_config(drv8323_spi_t *dev, const drv8323_cfg_t *cfg);

HAL_StatusTypeDef drv8323_read_reg (drv8323_spi_t *dev, drv_reg_t reg, uint16_t *data11);
HAL_StatusTypeDef drv8323_write_reg(drv8323_spi_t *dev, drv_reg_t reg, uint16_t data11, uint16_t *prev_data11);

HAL_StatusTypeDef drv8323_clear_faults(drv8323_spi_t *dev);

// Non-mutating readback of the board defaults and fault status.
HAL_StatusTypeDef drv8323_comm_test(drv8323_spi_t *dev, bool *pass);

// Helper: decode “any fault active” from status1 bit10
static inline bool drv8323_fault_active(uint16_t fault_status1_11b)
{
  return ((fault_status1_11b >> DRV_FAULT1_FAULT_Pos) & 0x1u) != 0u;
}

#ifdef __cplusplus
}
#endif
