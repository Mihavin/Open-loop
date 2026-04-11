/*
 * s25fl256s.c
 *
 * STM32H7 HAL QSPI driver for Infineon/Cypress/Spansion S25FL256S Serial NOR.
 */

#include "s25fl256s.h"

/* --- Instruction set (from S25FL128S/S25FL256S datasheet) --- */
#define S25FL256S_CMD_RDID            0x9Fu
#define S25FL256S_CMD_RDSR1           0x05u
#define S25FL256S_CMD_RDCR1           0x35u
#define S25FL256S_CMD_WRR             0x01u
#define S25FL256S_CMD_WREN            0x06u
#define S25FL256S_CMD_WRDI            0x04u
#define S25FL256S_CMD_CLSR            0x30u
#define S25FL256S_CMD_RESET           0xF0u
#define S25FL256S_CMD_MBR             0xFFu

#define S25FL256S_CMD_4QIOR           0xECu  /* Quad I/O Read (4-byte address) */
#define S25FL256S_CMD_4PP             0x12u  /* Page Program (4-byte address) */
#define S25FL256S_CMD_4QPP            0x34u  /* Quad Page Program (4-byte address) */
#define S25FL256S_CMD_4P4E            0x21u  /* Parameter 4KB Erase (4-byte address) */
#define S25FL256S_CMD_4SE             0xDCu  /* 64KB/256KB Sector Erase (4-byte address) */
#define S25FL256S_CMD_BE              0x60u  /* Bulk erase */

/* --- Status bits (SR1) --- */
#define S25FL256S_SR1_WIP             0x01u
#define S25FL256S_SR1_WEL             0x02u
#define S25FL256S_SR1_E_ERR           0x20u /* SR1[5] */
#define S25FL256S_SR1_P_ERR           0x40u /* SR1[6] */

/* --- Configuration Register 1 (CR1) bits --- */
#define S25FL256S_CR1_LC_MASK         0xC0u /* CR1[7:6] */
#define S25FL256S_CR1_LC_10           0x80u /* LC = 10b */
#define S25FL256S_CR1_QUAD            0x02u /* CR1[1] */

/* Read timing used by this driver after it programs LC=10b.
 * Table 26/28 shows for Quad I/O Read (EBh/ECh) at <=104MHz, LC=10b: Mode=2 cycles, Dummy=5 cycles.
 * We send 1 byte of mode bits (8 bits) and 5 dummy cycles.
 */
#define S25FL256S_QIOR_MODE            0x00u
#define S25FL256S_QIOR_DUMMY_CYCLES    5u

/* Internal helpers */
static HAL_StatusTypeDef s25fl256s_read_reg(QSPI_HandleTypeDef *hqspi, uint8_t instr, uint8_t *val);
static HAL_StatusTypeDef s25fl256s_write_enable(QSPI_HandleTypeDef *hqspi);
static HAL_StatusTypeDef s25fl256s_wait_ready(QSPI_HandleTypeDef *hqspi, uint32_t timeout_ms);
static HAL_StatusTypeDef s25fl256s_clear_status(QSPI_HandleTypeDef *hqspi);
static HAL_StatusTypeDef s25fl256s_write_sr1_cr1(QSPI_HandleTypeDef *hqspi, uint8_t sr1, uint8_t cr1);

HAL_StatusTypeDef S25FL256S_Abort(QSPI_HandleTypeDef *hqspi)
{
    if (hqspi == NULL)
    {
        return HAL_ERROR;
    }
    return HAL_QSPI_Abort(hqspi);
}

HAL_StatusTypeDef S25FL256S_Reset(QSPI_HandleTypeDef *hqspi,
                                 GPIO_TypeDef *rst_port,
                                 uint16_t rst_pin)
{
    if (hqspi == NULL)
    {
        return HAL_ERROR;
    }

    /* Always try to exit memory-mapped mode / abort any ongoing transaction first. */
    (void)HAL_QSPI_Abort(hqspi);

    if (rst_port != NULL)
    {
        /* Hardware reset pin: active low on most designs */
        HAL_GPIO_WritePin(rst_port, rst_pin, GPIO_PIN_RESET);
        HAL_Delay(1);
        HAL_GPIO_WritePin(rst_port, rst_pin, GPIO_PIN_SET);
        HAL_Delay(2);
        return HAL_OK;
    }

    /* Software reset sequence. Also send Mode Bit Reset to ensure we exit any continuous read mode. */
    {
        QSPI_CommandTypeDef cmd = {0};

        cmd.InstructionMode   = QSPI_INSTRUCTION_1_LINE;
        cmd.AddressMode       = QSPI_ADDRESS_NONE;
        cmd.AlternateByteMode= QSPI_ALTERNATE_BYTES_NONE;
        cmd.DataMode          = QSPI_DATA_NONE;
        cmd.DummyCycles       = 0;
        cmd.DdrMode           = QSPI_DDR_MODE_DISABLE;
        cmd.DdrHoldHalfCycle  = QSPI_DDR_HHC_ANALOG_DELAY;
        cmd.SIOOMode          = QSPI_SIOO_INST_EVERY_CMD;

        cmd.Instruction = S25FL256S_CMD_MBR;
        if (HAL_QSPI_Command(hqspi, &cmd, S25FL256S_DEFAULT_TIMEOUT) != HAL_OK)
        {
            return HAL_ERROR;
        }

        cmd.Instruction = S25FL256S_CMD_RESET;
        if (HAL_QSPI_Command(hqspi, &cmd, S25FL256S_DEFAULT_TIMEOUT) != HAL_OK)
        {
            return HAL_ERROR;
        }

        HAL_Delay(1);
    }

    return HAL_OK;
}

HAL_StatusTypeDef S25FL256S_ReadJEDECID(QSPI_HandleTypeDef *hqspi,
                                       S25FL256S_JEDEC_ID *id)
{
    if ((hqspi == NULL) || (id == NULL))
    {
        return HAL_ERROR;
    }

    QSPI_CommandTypeDef cmd = {0};

    cmd.InstructionMode    = QSPI_INSTRUCTION_1_LINE;
    cmd.Instruction        = S25FL256S_CMD_RDID;
    cmd.AddressMode        = QSPI_ADDRESS_NONE;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DummyCycles        = 0;
    cmd.DataMode           = QSPI_DATA_1_LINE;
    cmd.NbData             = 3;
    cmd.DdrMode            = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle   = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode           = QSPI_SIOO_INST_EVERY_CMD;

    if (HAL_QSPI_Command(hqspi, &cmd, S25FL256S_DEFAULT_TIMEOUT) != HAL_OK)
    {
        return HAL_ERROR;
    }

    uint8_t tmp[3] = {0};
    if (HAL_QSPI_Receive(hqspi, tmp, S25FL256S_DEFAULT_TIMEOUT) != HAL_OK)
    {
        return HAL_ERROR;
    }

    id->manufacturer_id = tmp[0];
    id->memory_type     = tmp[1];
    id->capacity        = tmp[2];
    return HAL_OK;
}

HAL_StatusTypeDef S25FL256S_Init(QSPI_HandleTypeDef *hqspi)
{
    if (hqspi == NULL)
    {
        return HAL_ERROR;
    }

    /* Make sure we're not stuck in memory-mapped mode. */
    (void)HAL_QSPI_Abort(hqspi);

    /* Clear any previous error flags (E_ERR/P_ERR). */
    if (s25fl256s_clear_status(hqspi) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* Read current SR1/CR1 */
    uint8_t sr1 = 0;
    uint8_t cr1 = 0;

    if (s25fl256s_read_reg(hqspi, S25FL256S_CMD_RDSR1, &sr1) != HAL_OK)
    {
        return HAL_ERROR;
    }
    if (s25fl256s_read_reg(hqspi, S25FL256S_CMD_RDCR1, &cr1) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* Program CR1: LC = 10b and QUAD = 1. Preserve all other bits. */
    uint8_t new_cr1 = cr1;
    new_cr1 &= (uint8_t)~S25FL256S_CR1_LC_MASK;
    new_cr1 |= S25FL256S_CR1_LC_10;
    new_cr1 |= S25FL256S_CR1_QUAD;

    if (new_cr1 != cr1)
    {
        if (s25fl256s_write_enable(hqspi) != HAL_OK)
        {
            return HAL_ERROR;
        }

        /* WRR writes SR1 then CR1 in one 16-bit transfer. Keep SR1 unchanged. */
        if (s25fl256s_write_sr1_cr1(hqspi, sr1, new_cr1) != HAL_OK)
        {
            return HAL_ERROR;
        }

        if (s25fl256s_wait_ready(hqspi, S25FL256S_DEFAULT_TIMEOUT) != HAL_OK)
        {
            return HAL_ERROR;
        }
    }

    return HAL_OK;
}

HAL_StatusTypeDef S25FL256S_Read(QSPI_HandleTypeDef *hqspi,
                                uint32_t address,
                                uint8_t *data,
                                uint32_t len)
{
    if ((hqspi == NULL) || (data == NULL) || (len == 0u))
    {
        return HAL_ERROR;
    }

    QSPI_CommandTypeDef cmd = {0};

    cmd.InstructionMode    = QSPI_INSTRUCTION_1_LINE;
    cmd.Instruction        = S25FL256S_CMD_4QIOR;

    cmd.AddressMode        = QSPI_ADDRESS_4_LINES;
    cmd.AddressSize        = QSPI_ADDRESS_32_BITS;
    cmd.Address            = address;

    /* Mode bits (8 bits) transferred on 4 lines -> use alternate bytes */
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_4_LINES;
    cmd.AlternateBytesSize = QSPI_ALTERNATE_BYTES_8_BITS;
    cmd.AlternateBytes     = S25FL256S_QIOR_MODE;

    cmd.DummyCycles        = S25FL256S_QIOR_DUMMY_CYCLES;

    cmd.DataMode           = QSPI_DATA_4_LINES;
    cmd.NbData             = len;

    cmd.DdrMode            = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle   = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode           = QSPI_SIOO_INST_EVERY_CMD;

    if (HAL_QSPI_Command(hqspi, &cmd, S25FL256S_DEFAULT_TIMEOUT) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (HAL_QSPI_Receive(hqspi, data, S25FL256S_DEFAULT_TIMEOUT) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

HAL_StatusTypeDef S25FL256S_Write(QSPI_HandleTypeDef *hqspi,
                                 uint32_t address,
                                 const uint8_t *data,
                                 uint32_t len)
{
    if ((hqspi == NULL) || (data == NULL) || (len == 0u))
    {
        return HAL_ERROR;
    }

    /* If we were in memory-mapped mode, leave it before program operations. */
    (void)HAL_QSPI_Abort(hqspi);

    uint32_t remaining = len;
    uint32_t curr_addr = address;
    const uint8_t *p   = data;

    while (remaining > 0u)
    {
        uint32_t page_off = curr_addr % (uint32_t)S25FL256S_PAGE_SIZE;
        uint32_t chunk    = (uint32_t)S25FL256S_PAGE_SIZE - page_off;
        if (chunk > remaining)
        {
            chunk = remaining;
        }

        if (s25fl256s_write_enable(hqspi) != HAL_OK)
        {
            return HAL_ERROR;
        }

        QSPI_CommandTypeDef cmd = {0};

        cmd.InstructionMode    = QSPI_INSTRUCTION_1_LINE;
#if (S25FL256S_USE_QUAD_PROGRAM == 1)
        cmd.Instruction        = S25FL256S_CMD_4QPP;
#else
        cmd.Instruction        = S25FL256S_CMD_4PP;
#endif

        cmd.AddressMode        = QSPI_ADDRESS_1_LINE;
        cmd.AddressSize        = QSPI_ADDRESS_32_BITS;
        cmd.Address            = curr_addr;

        cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
        cmd.DummyCycles        = 0;

#if (S25FL256S_USE_QUAD_PROGRAM == 1)
        cmd.DataMode           = QSPI_DATA_4_LINES;
#else
        cmd.DataMode           = QSPI_DATA_1_LINE;
#endif
        cmd.NbData             = chunk;

        cmd.DdrMode            = QSPI_DDR_MODE_DISABLE;
        cmd.DdrHoldHalfCycle   = QSPI_DDR_HHC_ANALOG_DELAY;
        cmd.SIOOMode           = QSPI_SIOO_INST_EVERY_CMD;

        if (HAL_QSPI_Command(hqspi, &cmd, S25FL256S_DEFAULT_TIMEOUT) != HAL_OK)
        {
            return HAL_ERROR;
        }

        if (HAL_QSPI_Transmit(hqspi, (uint8_t *)p, S25FL256S_DEFAULT_TIMEOUT) != HAL_OK)
        {
            return HAL_ERROR;
        }

        if (s25fl256s_wait_ready(hqspi, S25FL256S_PROGRAM_TIMEOUT) != HAL_OK)
        {
            return HAL_ERROR;
        }

        curr_addr  += chunk;
        p          += chunk;
        remaining  -= chunk;
    }

    return HAL_OK;
}

HAL_StatusTypeDef S25FL256S_Erase4K(QSPI_HandleTypeDef *hqspi,
                                   uint32_t address)
{
    if (hqspi == NULL)
    {
        return HAL_ERROR;
    }

    (void)HAL_QSPI_Abort(hqspi);

    if (s25fl256s_write_enable(hqspi) != HAL_OK)
    {
        return HAL_ERROR;
    }

    QSPI_CommandTypeDef cmd = {0};
    cmd.InstructionMode    = QSPI_INSTRUCTION_1_LINE;
    cmd.Instruction        = S25FL256S_CMD_4P4E;
    cmd.AddressMode        = QSPI_ADDRESS_1_LINE;
    cmd.AddressSize        = QSPI_ADDRESS_32_BITS;
    cmd.Address            = address;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DataMode           = QSPI_DATA_NONE;
    cmd.DummyCycles        = 0;
    cmd.DdrMode            = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle   = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode           = QSPI_SIOO_INST_EVERY_CMD;

    if (HAL_QSPI_Command(hqspi, &cmd, S25FL256S_DEFAULT_TIMEOUT) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return s25fl256s_wait_ready(hqspi, S25FL256S_ERASE_4K_TIMEOUT);
}

HAL_StatusTypeDef S25FL256S_Erase64K(QSPI_HandleTypeDef *hqspi,
                                    uint32_t address)
{
    if (hqspi == NULL)
    {
        return HAL_ERROR;
    }

    (void)HAL_QSPI_Abort(hqspi);

    if (s25fl256s_write_enable(hqspi) != HAL_OK)
    {
        return HAL_ERROR;
    }

    QSPI_CommandTypeDef cmd = {0};
    cmd.InstructionMode    = QSPI_INSTRUCTION_1_LINE;
    cmd.Instruction        = S25FL256S_CMD_4SE;
    cmd.AddressMode        = QSPI_ADDRESS_1_LINE;
    cmd.AddressSize        = QSPI_ADDRESS_32_BITS;
    cmd.Address            = address;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DataMode           = QSPI_DATA_NONE;
    cmd.DummyCycles        = 0;
    cmd.DdrMode            = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle   = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode           = QSPI_SIOO_INST_EVERY_CMD;

    if (HAL_QSPI_Command(hqspi, &cmd, S25FL256S_DEFAULT_TIMEOUT) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return s25fl256s_wait_ready(hqspi, S25FL256S_ERASE_64K_TIMEOUT);
}

HAL_StatusTypeDef S25FL256S_EraseChip(QSPI_HandleTypeDef *hqspi)
{
    if (hqspi == NULL)
    {
        return HAL_ERROR;
    }

    (void)HAL_QSPI_Abort(hqspi);

    if (s25fl256s_write_enable(hqspi) != HAL_OK)
    {
        return HAL_ERROR;
    }

    QSPI_CommandTypeDef cmd = {0};
    cmd.InstructionMode    = QSPI_INSTRUCTION_1_LINE;
    cmd.Instruction        = S25FL256S_CMD_BE;
    cmd.AddressMode        = QSPI_ADDRESS_NONE;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DataMode           = QSPI_DATA_NONE;
    cmd.DummyCycles        = 0;
    cmd.DdrMode            = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle   = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode           = QSPI_SIOO_INST_EVERY_CMD;

    if (HAL_QSPI_Command(hqspi, &cmd, S25FL256S_DEFAULT_TIMEOUT) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return s25fl256s_wait_ready(hqspi, S25FL256S_CHIP_ERASE_TIMEOUT);
}

HAL_StatusTypeDef S25FL256S_EnableMemoryMappedMode(QSPI_HandleTypeDef *hqspi)
{
    if (hqspi == NULL)
    {
        return HAL_ERROR;
    }

    /* Make sure we are in indirect mode first */
    (void)HAL_QSPI_Abort(hqspi);

    QSPI_CommandTypeDef cmd = {0};

    cmd.InstructionMode    = QSPI_INSTRUCTION_1_LINE;
    cmd.Instruction        = S25FL256S_CMD_4QIOR;

    cmd.AddressMode        = QSPI_ADDRESS_4_LINES;
    cmd.AddressSize        = QSPI_ADDRESS_32_BITS;
    cmd.Address            = 0;

    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_4_LINES;
    cmd.AlternateBytesSize = QSPI_ALTERNATE_BYTES_8_BITS;
    cmd.AlternateBytes     = S25FL256S_QIOR_MODE;

    cmd.DummyCycles        = S25FL256S_QIOR_DUMMY_CYCLES;

    cmd.DataMode           = QSPI_DATA_4_LINES;
    cmd.NbData             = 0;

    cmd.DdrMode            = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle   = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode           = QSPI_SIOO_INST_EVERY_CMD;

    QSPI_MemoryMappedTypeDef mm = {0};
    mm.TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE;
    mm.TimeOutPeriod     = 0;

    return HAL_QSPI_MemoryMapped(hqspi, &cmd, &mm);
}

/* ------------------------------------------------------------------------- */

static HAL_StatusTypeDef s25fl256s_read_reg(QSPI_HandleTypeDef *hqspi, uint8_t instr, uint8_t *val)
{
    QSPI_CommandTypeDef cmd = {0};

    cmd.InstructionMode    = QSPI_INSTRUCTION_1_LINE;
    cmd.Instruction        = instr;
    cmd.AddressMode        = QSPI_ADDRESS_NONE;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DummyCycles        = 0;
    cmd.DataMode           = QSPI_DATA_1_LINE;
    cmd.NbData             = 1;
    cmd.DdrMode            = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle   = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode           = QSPI_SIOO_INST_EVERY_CMD;

    if (HAL_QSPI_Command(hqspi, &cmd, S25FL256S_DEFAULT_TIMEOUT) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (HAL_QSPI_Receive(hqspi, val, S25FL256S_DEFAULT_TIMEOUT) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

static HAL_StatusTypeDef s25fl256s_clear_status(QSPI_HandleTypeDef *hqspi)
{
    QSPI_CommandTypeDef cmd = {0};

    cmd.InstructionMode    = QSPI_INSTRUCTION_1_LINE;
    cmd.Instruction        = S25FL256S_CMD_CLSR;
    cmd.AddressMode        = QSPI_ADDRESS_NONE;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DataMode           = QSPI_DATA_NONE;
    cmd.DummyCycles        = 0;
    cmd.DdrMode            = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle   = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode           = QSPI_SIOO_INST_EVERY_CMD;

    return HAL_QSPI_Command(hqspi, &cmd, S25FL256S_DEFAULT_TIMEOUT);
}

static HAL_StatusTypeDef s25fl256s_write_enable(QSPI_HandleTypeDef *hqspi)
{
    QSPI_CommandTypeDef cmd = {0};

    cmd.InstructionMode    = QSPI_INSTRUCTION_1_LINE;
    cmd.Instruction        = S25FL256S_CMD_WREN;
    cmd.AddressMode        = QSPI_ADDRESS_NONE;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DataMode           = QSPI_DATA_NONE;
    cmd.DummyCycles        = 0;
    cmd.DdrMode            = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle   = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode           = QSPI_SIOO_INST_EVERY_CMD;

    if (HAL_QSPI_Command(hqspi, &cmd, S25FL256S_DEFAULT_TIMEOUT) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* Auto-poll WEL=1 */
    QSPI_AutoPollingTypeDef cfg = {0};

    cfg.Match           = S25FL256S_SR1_WEL;
    cfg.Mask            = S25FL256S_SR1_WEL;
    cfg.MatchMode       = QSPI_MATCH_MODE_AND;
    cfg.StatusBytesSize = 1;
    cfg.Interval        = 0x10;
    cfg.AutomaticStop   = QSPI_AUTOMATIC_STOP_ENABLE;

    cmd.Instruction     = S25FL256S_CMD_RDSR1;
    cmd.DataMode        = QSPI_DATA_1_LINE;
    cmd.NbData          = 1;

    if (HAL_QSPI_AutoPolling(hqspi, &cmd, &cfg, S25FL256S_DEFAULT_TIMEOUT) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

static HAL_StatusTypeDef s25fl256s_wait_ready(QSPI_HandleTypeDef *hqspi, uint32_t timeout_ms)
{
    /* Auto-poll WIP=0 */
    QSPI_CommandTypeDef cmd = {0};
    QSPI_AutoPollingTypeDef cfg = {0};

    cmd.InstructionMode    = QSPI_INSTRUCTION_1_LINE;
    cmd.Instruction        = S25FL256S_CMD_RDSR1;
    cmd.AddressMode        = QSPI_ADDRESS_NONE;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DummyCycles        = 0;
    cmd.DataMode           = QSPI_DATA_1_LINE;
    cmd.NbData             = 1;
    cmd.DdrMode            = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle   = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode           = QSPI_SIOO_INST_EVERY_CMD;

    cfg.Match           = 0;
    cfg.Mask            = S25FL256S_SR1_WIP;
    cfg.MatchMode       = QSPI_MATCH_MODE_AND;
    cfg.StatusBytesSize = 1;
    cfg.Interval        = 0x10;
    cfg.AutomaticStop   = QSPI_AUTOMATIC_STOP_ENABLE;

    if (HAL_QSPI_AutoPolling(hqspi, &cmd, &cfg, timeout_ms) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* Check for program/erase errors. If set, clear status and report error. */
    uint8_t sr1 = 0;
    if (s25fl256s_read_reg(hqspi, S25FL256S_CMD_RDSR1, &sr1) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if ((sr1 & (S25FL256S_SR1_E_ERR | S25FL256S_SR1_P_ERR)) != 0u)
    {
        (void)s25fl256s_clear_status(hqspi);
        return HAL_ERROR;
    }

    return HAL_OK;
}

static HAL_StatusTypeDef s25fl256s_write_sr1_cr1(QSPI_HandleTypeDef *hqspi, uint8_t sr1, uint8_t cr1)
{
    QSPI_CommandTypeDef cmd = {0};

    cmd.InstructionMode    = QSPI_INSTRUCTION_1_LINE;
    cmd.Instruction        = S25FL256S_CMD_WRR;
    cmd.AddressMode        = QSPI_ADDRESS_NONE;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DummyCycles        = 0;
    cmd.DataMode           = QSPI_DATA_1_LINE;
    cmd.NbData             = 2;
    cmd.DdrMode            = QSPI_DDR_MODE_DISABLE;
    cmd.DdrHoldHalfCycle   = QSPI_DDR_HHC_ANALOG_DELAY;
    cmd.SIOOMode           = QSPI_SIOO_INST_EVERY_CMD;

    if (HAL_QSPI_Command(hqspi, &cmd, S25FL256S_DEFAULT_TIMEOUT) != HAL_OK)
    {
        return HAL_ERROR;
    }

    uint8_t buf[2];
    buf[0] = sr1;
    buf[1] = cr1;

    if (HAL_QSPI_Transmit(hqspi, buf, S25FL256S_DEFAULT_TIMEOUT) != HAL_OK)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}
