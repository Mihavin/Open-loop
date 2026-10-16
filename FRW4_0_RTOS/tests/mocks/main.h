#ifndef MOCK_MAIN_H
#define MOCK_MAIN_H
#include "stm32h7xx_hal.h"
extern GPIO_TypeDef mock_gpio_f, mock_gpio_g, mock_gpio_b, mock_gpio_h, mock_gpio_i, mock_gpio_a;
#define DRV_EN_GPIO_Port (&mock_gpio_f)
#define DRV_EN_Pin 64u
#define DRV_ENN_GPIO_Port (&mock_gpio_g)
#define DRV_ENN_Pin 8u
#define DRV_NFAULT_GPIO_Port (&mock_gpio_b)
#define DRV_NFAULT_Pin 4096u
#define DRV_CAL_GPIO_Port (&mock_gpio_h)
#define DRV_CAL_Pin 32u
#define LIMIT_BUTTON_GPIO_Port (&mock_gpio_b)
#define LIMIT_BUTTON_Pin 16384u
#define DIR_GPIO_Port (&mock_gpio_h)
#define DIR_Pin 128u
#define SPI_MODE_GPIO_Port (&mock_gpio_i)
#define SPI_MODE_Pin 8u
#define DC_EN_GPIO_Port (&mock_gpio_a)
#define DC_EN_Pin 2048u
#define EXT_NSS_GPIO_Port (&mock_gpio_a)
#define EXT_NSS_Pin 16u
#define DIAG0_GPIO_Port (&mock_gpio_a)
#define DIAG0_Pin 128u
#define DIAG1_GPIO_Port (&mock_gpio_a)
#define DIAG1_Pin 256u
#define DC_IN_GPIO_Port (&mock_gpio_g)
#define DC_IN_Pin 4u
#endif
