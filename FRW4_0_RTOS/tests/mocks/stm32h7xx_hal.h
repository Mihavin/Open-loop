#ifndef MOCK_HAL_H
#define MOCK_HAL_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
typedef struct { uint32_t BSRR; } GPIO_TypeDef;
typedef struct { uint32_t BDTR, DIER, CCER, CR1, CCMR1, CNT, CCR1, CCR2, CCR3, CCR4, PSC, ARR, SR; } TIM_TypeDef;
typedef struct { TIM_TypeDef *Instance; struct {uint32_t Prescaler, Period;} Init; uint32_t Channel; } TIM_HandleTypeDef;
typedef struct { struct { uint32_t DataSize, FirstBit, CLKPolarity, CLKPhase; } Init; } SPI_HandleTypeDef;
typedef struct { uint32_t raw; } ADC_HandleTypeDef;
typedef struct { uint32_t APB1CLKDivider, APB2CLKDivider; } RCC_ClkInitTypeDef;
typedef struct { uint32_t AHB4ENR, APB2ENR, APB1LENR; } MockRCC;
typedef struct { uint32_t KR, PR, RLR, SR; } MockIWDG;
typedef struct {uint32_t OCMode, OCPolarity, Pulse;} TIM_OC_InitTypeDef;
extern TIM_TypeDef mock_tim1, mock_tim2, mock_tim12, mock_tim8;
extern MockRCC mock_rcc;
extern MockIWDG mock_iwdg;
#define TIM1 (&mock_tim1)
#define TIM2 (&mock_tim2)
#define TIM12 (&mock_tim12)
#define TIM8 (&mock_tim8)
#define RCC (&mock_rcc)
#define IWDG1 (&mock_iwdg)
#define RCC_AHB4ENR_GPIOFEN 1u
#define RCC_AHB4ENR_GPIOGEN 2u
#define RCC_APB2ENR_TIM1EN 1u
#define RCC_APB1LENR_TIM12EN 1u
#define RCC_HCLK_DIV1 1u
#define RCC_FLAG_LSIRDY 1u
#define __HAL_RCC_LSI_ENABLE() ((void)0)
#define __HAL_RCC_GET_FLAG(f) ((void)(f),1u)
#define SPI_DATASIZE_8BIT 8u
#define SPI_DATASIZE_16BIT 16u
#define SPI_FIRSTBIT_MSB 0u
#define SPI_POLARITY_HIGH 1u
#define SPI_PHASE_2EDGE 1u
#define TIM_CHANNEL_1 0u
#define TIM_CHANNEL_2 4u
#define TIM_CHANNEL_3 8u
#define TIM_CHANNEL_4 12u
#define TIM_CHANNEL_ALL 0x3cu
#define HAL_TIM_ACTIVE_CHANNEL_2 2u
#define TIM_OCMODE_FORCED_INACTIVE 0x40u
#define TIM_OCMODE_TOGGLE 0x30u
#define TIM_OCPOLARITY_HIGH 0u
#define TIM_BDTR_MOE 1u
#define TIM_CCER_CC2E 0x10u
#define TIM_DIER_CC2IE 4u
#define TIM_CR1_CEN 1u
#define TIM_CCMR1_OC2M 0x7000u
#define TIM_CCMR1_OC2PE 0x800u
#define TIM_EVENTSOURCE_UPDATE 1u
#define TIM_FLAG_CC2 4u
#define TIM_FLAG_BREAK 0x80u
#define ADC_INJECTED_RANK_1 1u
#define ADC_CALIB_OFFSET_LINEARITY 0u
#define ADC_SINGLE_ENDED 0u
#define ADC3_IRQn 1u
#define TIM8_BRK_TIM12_IRQn 2u
#define __NOP() ((void)0)
#define __DMB() ((void)0)
#define __DSB() ((void)0)
extern uint32_t mock_primask;
#define __get_PRIMASK() mock_primask
#define __disable_irq() (mock_primask=1u)
#define __set_PRIMASK(x) (mock_primask=(x))
#define CLEAR_BIT(r,b) ((r)&=~(b))
#define MODIFY_REG(r,m,b) ((r)=((r)&~(m))|(b))
#define __HAL_TIM_DISABLE(t) ((t)->Instance->CR1 &= ~TIM_CR1_CEN)
#define __HAL_TIM_SET_PRESCALER(t,v) ((t)->Instance->PSC=(v))
#define __HAL_TIM_SET_AUTORELOAD(t,v) ((t)->Instance->ARR=(v))
#define __HAL_TIM_GET_AUTORELOAD(t) ((t)->Instance->ARR)
#define __HAL_TIM_SET_COUNTER(t,v) ((t)->Instance->CNT=(v))
#define __HAL_TIM_GET_COUNTER(t) ((t)->Instance->CNT)
#define __HAL_TIM_CLEAR_FLAG(t,v) ((t)->Instance->SR &= ~(v))
#define __HAL_TIM_GET_FLAG(t,v) ((t)->Instance->SR & (v))
#define __HAL_TIM_MOE_ENABLE(t) ((t)->Instance->BDTR |= TIM_BDTR_MOE)
static inline void mock_compare(TIM_HandleTypeDef *t,uint32_t ch,uint32_t v) {
 if(ch==TIM_CHANNEL_1)t->Instance->CCR1=v;
 if(ch==TIM_CHANNEL_2)t->Instance->CCR2=v;
 if(ch==TIM_CHANNEL_3)t->Instance->CCR3=v;
 if(ch==TIM_CHANNEL_4)t->Instance->CCR4=v;
}
#define __HAL_TIM_SET_COMPARE(t,ch,v) mock_compare(t,ch,v)
HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef*,uint8_t*,uint8_t*,uint16_t,uint32_t);
void HAL_GPIO_WritePin(GPIO_TypeDef*,uint16_t,GPIO_PinState);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef*,uint16_t);
void HAL_Delay(uint32_t);
uint32_t HAL_GetTick(void);
void HAL_RCC_GetClockConfig(RCC_ClkInitTypeDef*,uint32_t*);
uint32_t HAL_RCC_GetPCLK1Freq(void);
uint32_t HAL_RCC_GetPCLK2Freq(void);
void HAL_NVIC_SetPriority(uint32_t,uint32_t,uint32_t);
void HAL_NVIC_EnableIRQ(uint32_t);
HAL_StatusTypeDef HAL_TIM_GenerateEvent(TIM_HandleTypeDef*,uint32_t);
HAL_StatusTypeDef HAL_TIM_OC_ConfigChannel(TIM_HandleTypeDef*,TIM_OC_InitTypeDef*,uint32_t);
void HAL_TIM_IRQHandler(TIM_HandleTypeDef*);
HAL_StatusTypeDef HAL_TIM_Encoder_Start(TIM_HandleTypeDef*,uint32_t);
HAL_StatusTypeDef HAL_TIM_Base_Start(TIM_HandleTypeDef*);
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef*,uint32_t);
HAL_StatusTypeDef HAL_TIMEx_PWMN_Start(TIM_HandleTypeDef*,uint32_t);
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef*,uint32_t,uint32_t);
HAL_StatusTypeDef HAL_ADCEx_InjectedStart_IT(ADC_HandleTypeDef*);
uint32_t HAL_ADCEx_InjectedGetValue(ADC_HandleTypeDef*,uint32_t);
#endif
