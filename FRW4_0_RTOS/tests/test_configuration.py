"""Independent electrical timing/capacity contracts from the review datasheets."""
from pathlib import Path
import math
import re
import unittest
ROOT=Path(__file__).resolve().parents[1]

class ConfigurationTests(unittest.TestCase):
    def test_sdram_minima_and_controller_constraints(self):
        source=(ROOT/'Core/Src/fmc.c').read_text()
        clock=(ROOT/'Core/Src/main.c').read_text()
        def pll(field): return int(re.search(r'PLL2\.'+field+r' = (\d+);',clock)[1])
        hz=25e6/pll('PLL2M')*pll('PLL2N')/pll('PLL2R')/2
        times={k:int(v) for k,v in re.findall(r'SdramTiming\.(\w+) = (\d+);',source)}
        # Supplied Micron Rev V, Table 12, -6A at CAS3; tWR also meets auto-precharge.
        for field,ns in [('ExitSelfRefreshDelay',67),('SelfRefreshTime',42),('RowCycleDelay',60),('RPDelay',18),('RCDDelay',18)]:
            self.assertGreaterEqual(times[field],math.ceil(ns*1e-9*hz))
        self.assertGreaterEqual(times['LoadToActiveDelay'],2)
        self.assertGreaterEqual(times['WriteRecoveryTime']/hz,1/hz+6e-9)
        self.assertGreaterEqual(times['RowCycleDelay'],times['SelfRefreshTime']+times['RPDelay'])
        self.assertGreaterEqual(times['WriteRecoveryTime'],times['SelfRefreshTime']-times['RCDDelay'])
        self.assertGreaterEqual(times['WriteRecoveryTime'],times['RowCycleDelay']-times['RCDDelay']-times['RPDelay'])
        ioc=(ROOT/'FRW4_0_RTOS.ioc').read_text()
        for name in ['SelfRefreshTime','RowCycleDelay','WriteRecoveryTime']:
            self.assertIn(f'FMC.{name}1={times[name]}',ioc)

    def test_physical_nor_extent_agrees(self):
        source=(ROOT/'Core/Src/quadspi.c').read_text()
        field=int(re.search(r'FlashSize = (\d+);',source)[1])
        self.assertEqual(2**(field+1),32*1024*1024)
        linker=(ROOT/'STM32H743IITX_FLASH.ld').read_text()
        self.assertRegex(linker,r'ExtFlash \(rx\).*LENGTH = 32M')
        main=(ROOT/'Core/Src/main.c').read_text()
        self.assertIn('MPU_REGION_SIZE_32MB',main)
        self.assertIn('QUADSPI.FlashSize=24',(ROOT/'FRW4_0_RTOS.ioc').read_text())

    def test_stepper_clock_timing_and_mode(self):
        main=(ROOT/'Core/Src/main.c').read_text()
        divider=int(re.search(r'RCC_MCO2SOURCE_HSE, RCC_MCODIV_(\d+)',main)[1])
        external_clock=25e6/divider
        self.assertTrue(4e6<=external_clock<=18e6)
        spi=(ROOT/'Core/Src/spi.c').read_text()
        baud=int(re.search(r'hspi3.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_(\d+)',spi)[1])
        serial_clock=(25e6/2*72/7)/baud
        self.assertLess(serial_clock,external_clock/2.5)
        self.assertIn('hspi3.Init.CLKPolarity = SPI_POLARITY_HIGH;',spi)
        self.assertIn('hspi3.Init.CLKPhase = SPI_PHASE_2EDGE;',spi)

    def test_output_defaults_and_hardware_break(self):
        timer=(ROOT/'Core/Src/tim.c').read_text()
        self.assertIn('sBreakDeadTimeConfig.BreakState = TIM_BREAK_ENABLE;',timer)
        self.assertIn('sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_LOW;',timer)
        self.assertIn('sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;',timer)
        self.assertIn('sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_ENABLE;',timer)
        gpio=(ROOT/'Core/Src/gpio.c').read_text()
        self.assertLess(gpio.index('HAL_GPIO_WritePin(DRV_ENN_GPIO_Port, DRV_ENN_Pin, GPIO_PIN_SET);'),
                        gpio.index('GPIO_InitStruct.Pin = DRV_ENN_Pin'))
        self.assertNotIn('GPIO_MODE_IT_FALLING',gpio[gpio.index('GPIO_InitStruct.Pin = FAN_FAULT_Pin;'):gpio.index('HAL_GPIO_Init(FAN_FAULT_GPIO_Port')])

if __name__=='__main__': unittest.main(verbosity=2)
