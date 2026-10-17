/* Execute the production implementation against deterministic peripherals.
   Including C files also permits fixture reset between independent scenarios. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "main.h"
#include "cmsis_os2.h"
#include "operator_input.h"
#include "../Core/Src/drv_spi.c"
#include "../Core/Src/fault_manager.c"
#include "../Core/Src/motor_foc.c"
#include "../Core/Src/tmc2130.c"
#include "../Core/Src/tmc2130_motion.c"
#include "../Core/Src/ui_api.c"

TIM_TypeDef mock_tim1, mock_tim2, mock_tim12, mock_tim8;
MockRCC mock_rcc;
MockIWDG mock_iwdg;
GPIO_TypeDef mock_gpio_f, mock_gpio_g, mock_gpio_b, mock_gpio_h, mock_gpio_i, mock_gpio_a;
uint32_t mock_primask;
TIM_HandleTypeDef htim1={.Instance=TIM1}, htim2={.Instance=TIM2}, htim12={.Instance=TIM12};
ADC_HandleTypeDef hadc1,hadc2,hadc3;
SPI_HandleTypeDef spi_drv, spi_step;
drv8323_spi_t g_drv8323;
TMC2130Motion_Handle_t g_stepper_motion;
static uint32_t ticks, serial_calls, bad_pwm_start;
static uint16_t drv_regs[16];
static uint32_t step_regs[128], step_response;
static bool spi_stuck, spi_error, n_fault, driver_enabled, limit_clear;
typedef struct { uint8_t data[16][32]; unsigned size,head,tail,count; } MockQueue;
static MockQueue queue;

HAL_StatusTypeDef HAL_SPI_TransmitReceive(SPI_HandleTypeDef *spi,uint8_t *tx,uint8_t *rx,uint16_t size,uint32_t timeout)
{
    (void)timeout; serial_calls++;
    if(spi_error) return HAL_TIMEOUT;
    if(spi==&spi_drv) {
        assert(size==1); /* A 16-bit frame is exactly two bytes, never Size=2. */
        uint16_t word; memcpy(&word,tx,2);
        unsigned address=(word>>11)&15;
        uint16_t value=drv_regs[address];
        if(!(word&0x8000)) {
            if(address==3 || ((drv_regs[3]>>8)&7)!=DRV_LOCK_LOCK_CODE)
                drv_regs[address]=word&0x7ff;
            if(address==2) drv_regs[address]&=~1u;
        }
        if(spi_stuck)value=0;
        memcpy(rx,&value,2);
    } else {
        assert(spi==&spi_step && size==5);
        rx[0]=0;
        rx[1]=(uint8_t)(step_response>>24);rx[2]=(uint8_t)(step_response>>16);
        rx[3]=(uint8_t)(step_response>>8);rx[4]=(uint8_t)step_response;
        unsigned address=tx[0]&127;
        if(tx[0]&128) {
            uint32_t data=((uint32_t)tx[1]<<24)|((uint32_t)tx[2]<<16)|((uint32_t)tx[3]<<8)|tx[4];
            if(address==TMC2130_REG_GSTAT) step_regs[address]&=~data;
            else step_regs[address]=data;
        }
        step_response=step_regs[address];
        if(spi_stuck) memset(rx,0,5);
    }
    return HAL_OK;
}
void HAL_GPIO_WritePin(GPIO_TypeDef *gpio,uint16_t pin,GPIO_PinState value) {
    gpio->BSRR=value==GPIO_PIN_SET?pin:(uint32_t)pin<<16;
    if(gpio==DRV_EN_GPIO_Port && pin==DRV_EN_Pin) {
        driver_enabled=value==GPIO_PIN_SET;
        if(!driver_enabled)memset(drv_regs,0,sizeof(drv_regs));
    }
}
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *gpio,uint16_t pin) {
    if(gpio==LIMIT_BUTTON_GPIO_Port && pin==LIMIT_BUTTON_Pin) return limit_clear?GPIO_PIN_SET:GPIO_PIN_RESET;
    return gpio==DRV_NFAULT_GPIO_Port && pin==DRV_NFAULT_Pin ? (n_fault?GPIO_PIN_SET:GPIO_PIN_RESET):GPIO_PIN_RESET;
}
void HAL_Delay(uint32_t ms){ticks+=ms;}
uint32_t HAL_GetTick(void){return ticks;}
void HAL_RCC_GetClockConfig(RCC_ClkInitTypeDef *cfg,uint32_t *latency){cfg->APB1CLKDivider=cfg->APB2CLKDivider=2;*latency=3;}
uint32_t HAL_RCC_GetPCLK1Freq(void){return 112500000;}
uint32_t HAL_RCC_GetPCLK2Freq(void){return 112500000;}
void HAL_NVIC_SetPriority(uint32_t irq,uint32_t preempt,uint32_t sub){(void)irq;(void)preempt;(void)sub;}
void HAL_NVIC_EnableIRQ(uint32_t irq){(void)irq;}
HAL_StatusTypeDef HAL_TIM_GenerateEvent(TIM_HandleTypeDef *t,uint32_t e){(void)e;t->Instance->CNT=0;return HAL_OK;}
HAL_StatusTypeDef HAL_TIM_OC_ConfigChannel(TIM_HandleTypeDef *t,TIM_OC_InitTypeDef *oc,uint32_t ch){(void)ch;t->Instance->CCMR1=oc->OCMode<<8;return HAL_OK;}
void HAL_TIM_IRQHandler(TIM_HandleTypeDef *t){HAL_TIM_OC_DelayElapsedCallback(t);}
HAL_StatusTypeDef HAL_TIM_Encoder_Start(TIM_HandleTypeDef *t,uint32_t ch){(void)t;(void)ch;return HAL_OK;}
HAL_StatusTypeDef HAL_TIM_Base_Start(TIM_HandleTypeDef *t){t->Instance->CR1|=TIM_CR1_CEN;return HAL_OK;}
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *t,uint32_t ch){(void)ch;if(driver_enabled)bad_pwm_start++;t->Instance->BDTR|=TIM_BDTR_MOE;return HAL_OK;}
HAL_StatusTypeDef HAL_TIMEx_PWMN_Start(TIM_HandleTypeDef *t,uint32_t ch){return HAL_TIM_PWM_Start(t,ch);}
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *a,uint32_t mode,uint32_t single){(void)a;(void)mode;(void)single;return HAL_OK;}
HAL_StatusTypeDef HAL_ADCEx_InjectedStart_IT(ADC_HandleTypeDef *a){(void)a;return HAL_OK;}
uint32_t HAL_ADCEx_InjectedGetValue(ADC_HandleTypeDef *a,uint32_t rank){(void)rank;return a->raw;}
osMessageQueueId_t osMessageQueueNew(uint32_t count,uint32_t size,const void *attr){(void)attr;assert(count==16 && size<=32);memset(&queue,0,sizeof(queue));queue.size=size;return &queue;}
osStatus_t osMessageQueuePut(osMessageQueueId_t id,const void *item,uint8_t priority,uint32_t timeout){(void)priority;(void)timeout;MockQueue*q=id;if(q->count==16)return osError;memcpy(q->data[q->head],item,q->size);q->head=(q->head+1)%16;q->count++;return osOK;}
osStatus_t osMessageQueueGet(osMessageQueueId_t id,void *item,uint8_t *priority,uint32_t timeout){(void)priority;(void)timeout;MockQueue*q=id;if(!q->count)return osError;memcpy(item,q->data[q->tail],q->size);q->tail=(q->tail+1)%16;q->count--;return osOK;}

static void reset(void) {
    memset(&mock_tim1,0,sizeof(mock_tim1));memset(&mock_tim2,0,sizeof(mock_tim2));memset(&mock_tim12,0,sizeof(mock_tim12));
    memset(&mock_gpio_f,0,sizeof(mock_gpio_f));memset(&mock_gpio_g,0,sizeof(mock_gpio_g));
    memset(drv_regs,0,sizeof(drv_regs));memset(step_regs,0,sizeof(step_regs));
    mock_rcc=(MockRCC){3,1,1};mock_iwdg=(MockIWDG){0};
    spi_drv=(SPI_HandleTypeDef){.Init={16,SPI_FIRSTBIT_MSB,0,1}};
    spi_step=(SPI_HandleTypeDef){.Init={8,SPI_FIRSTBIT_MSB,1,1}};
    g_drv8323=(drv8323_spi_t){.hspi=&spi_drv,.ncs_port=&mock_gpio_g,.ncs_pin=1024,.en_port=DRV_EN_GPIO_Port,.en_pin=DRV_EN_Pin,.spi_timeout_ms=5};
    htim1.Init.Prescaler=1;htim1.Init.Period=2812;mock_tim1.ARR=2812;
    htim12.Channel=HAL_TIM_ACTIVE_CHANNEL_2;
    hadc1.raw=hadc2.raw=hadc3.raw=32768;
    ticks=serial_calls=bad_pwm_start=mock_primask=0;spi_stuck=spi_error=driver_enabled=false;n_fault=limit_clear=true;
    g_fault_bits=0;g_started=g_calibrated=g_watchdog_started=false;g_source=UI_CONTROL_KNOB;
    g_step_dev=NULL;g_step_queue=NULL;step_response=0;step_regs[TMC2130_REG_IOIN]=0x11000000;
}
static void sample(void){HAL_ADCEx_InjectedConvCpltCallback(&hadc2);HAL_ADCEx_InjectedConvCpltCallback(&hadc1);HAL_ADCEx_InjectedConvCpltCallback(&hadc3);}
static void calibrated(void) {
    MotorFOC_Init();MotorFOC_Start();
    for(unsigned i=0;i<520;i++){sample();if(i%20==0){ticks++;MotorFOC();}}
    assert(!Fault_IsCriticalActive());assert(MotorFOC_GetState()==MOTOR_FOC_STATE_IDLE);
    assert(!(mock_tim1.BDTR&TIM_BDTR_MOE));assert(bad_pwm_start==0);
}
static void owner_tick(void){ticks++;sample();MotorFOC();}
static void driver_frames(void) {
    reset();drv8323_cfg_t cfg;drv8323_cfg_set_defaults(&cfg);
    assert(drv8323_init(&g_drv8323,&cfg)==HAL_OK);assert(serial_calls>10);
    uint16_t value;assert(drv8323_read_reg(&g_drv8323,DRV_REG_CSA_CTRL,&value)==HAL_OK);
    assert(value==cfg.reg6_csa);assert(drv8323_verify_config(&g_drv8323,&cfg)==HAL_OK);
    drv_regs[5]^=8;assert(drv8323_verify_config(&g_drv8323,&cfg)==HAL_ERROR);
    spi_stuck=true;assert(drv8323_init(&g_drv8323,&cfg)==HAL_ERROR);
    spi_stuck=false;spi_error=true;assert(drv8323_init(&g_drv8323,&cfg)==HAL_TIMEOUT);
    puts("PASS: 16-bit frames, readback mismatch, stuck MISO and SPI timeout");
}
static void startup_stop_restart(void) {
    reset();calibrated();assert(g_calibrated);assert(fabsf(g_foc.dt_i-1.0f/20003.556f)<1e-8f);
    MotorFOC_RequestArm();owner_tick();assert(MotorFOC_GetState()==MOTOR_FOC_STATE_ALIGN);
    assert(mock_tim1.BDTR&TIM_BDTR_MOE);
    MotorFOC_RequestStop();MotorFOC_RequestArm();owner_tick();
    assert(MotorFOC_GetState()==MOTOR_FOC_STATE_IDLE);assert(!(mock_tim1.BDTR&TIM_BDTR_MOE));assert(g_started);
    MotorFOC_RequestArm();owner_tick();assert(MotorFOC_GetState()==MOTOR_FOC_STATE_ALIGN);
    MotorFOC_RequestDisarm();MotorFOC_RequestArm();owner_tick();assert(MotorFOC_GetState()==MOTOR_FOC_STATE_IDLE);
    spi_stuck=true;MotorFOC_RequestArm();owner_tick();assert(Fault_IsCriticalActive());assert(!(mock_tim1.BDTR&TIM_BDTR_MOE));
    puts("PASS: no startup torque, stop/disarm priority, restart and failed re-arm readback");
}
static void faults_and_adc(void) {
    reset();calibrated();MotorFOC_RequestArm();owner_tick();
    mock_tim12.CR1=TIM_CR1_CEN;mock_tim12.CCER=TIM_CCER_CC2E;mock_tim12.DIER=TIM_DIER_CC2IE;
    Fault_Report(FAULT_LIMIT);assert(!(mock_tim1.BDTR&TIM_BDTR_MOE));assert(!mock_tim12.CR1 && !mock_tim12.DIER);
    assert(mock_gpio_f.BSRR==((uint32_t)DRV_EN_Pin<<16));assert(mock_gpio_g.BSRR==DRV_ENN_Pin);
    Fault_Clear(0xffffffff);assert(Fault_IsCriticalActive());MotorFOC_RequestArm();owner_tick();assert(MotorFOC_GetState()==MOTOR_FOC_STATE_FAULT);
    reset();calibrated();MotorFOC_RequestArm();owner_tick();hadc2.raw=50000;sample();assert(Fault_GetActive()&FAULT_CURRENT);
    reset();calibrated();HAL_ADCEx_InjectedConvCpltCallback(&hadc1);HAL_ADCEx_InjectedConvCpltCallback(&hadc1);assert(Fault_GetActive()&FAULT_ADC);
    reset();MotorFOC_Init();MotorFOC_Start();hadc1.raw=hadc2.raw=hadc3.raw=0;
    for(unsigned i=0;i<512;i++) { sample(); }
    assert(Fault_GetActive()&FAULT_ADC);assert(!g_calibrated);
    reset();calibrated();ticks+=11;MotorFOC();assert(Fault_GetActive()&FAULT_ADC);
    reset();calibrated();limit_clear=false;MotorFOC_RequestArm();owner_tick();assert(Fault_GetActive()&FAULT_LIMIT);
    assert(!(mock_tim1.BDTR&TIM_BDTR_MOE));
    puts("PASS: immediate two-axis fault latch, overcurrent, missing/stale ADC and invalid offsets");
}
static void bounded_input(void) {
    assert(Operator_EncoderDelta(9999,0,10000)==-1);assert(Operator_EncoderDelta(0,9999,10000)==1);
    for(unsigned i=0;i<10000;i++){
        assert(Operator_EncoderDelta((i+1)%10000,i,10000)==1);
        assert(Operator_EncoderDelta((i+9999)%10000,i,10000)==-1);
    }
    assert(Operator_AddBounded(0,-1,2000)==-1);assert(Operator_AddBounded(1999,100,2000)==2000);
    assert(Operator_AddBounded(-1999,-100,2000)==-2000);assert(Operator_AddBounded(NAN,1,2000)==0);
    reset();calibrated();MotorFOC_SetSpeedRPM(9999);assert(g_cmd.speed_ref_rpm==2000);
    MotorFOC_SetSpeedRPM(NAN);assert(g_cmd.speed_ref_rpm==0);
    mock_tim2.CNT=400000;owner_tick();MotorFOC_SetMode(MOTOR_FOC_MODE_POSITION);
    assert(MotorFOC_GetRequestedMode()==MOTOR_FOC_MODE_SPEED); /* moving */
    owner_tick();MotorFOC_SetMode(MOTOR_FOC_MODE_POSITION);
    MotorFOC_SetPositionRevolutions(1e30f);assert(g_cmd.position_ref_counts==440000);
    MotorFOC_SetPositionRevolutions(-1e30f);assert(g_cmd.position_ref_counts==360000);
    MotorFOC_RequestArm();owner_tick();MotorFOC_SetMode(MOTOR_FOC_MODE_TORQUE);assert(MotorFOC_GetRequestedMode()==MOTOR_FOC_MODE_POSITION);
    MotorFOC_RequestZeroEncoder();assert(!(g_cmd.requests&MOTOR_FOC_REQ_ZERO_ENCODER));
    puts("PASS: exhaustive encoder wrap, signed bounded commands, NaN and moving mode/zero rejection");
}
static TMC2130_Handle_t stepper;
static void init_stepper(void){TMC2130_Config_t cfg;TMC2130_ConfigDefaults(&cfg);assert(TMC2130_Init(&stepper,&spi_step,&htim12,TIM_CHANNEL_2,&cfg)==HAL_OK);}
static void edge(void) {
    if(!stepper.step_clock_running)return;
    mock_tim12.CNT=mock_tim12.CCR2;
    HAL_TIM_OC_DelayElapsedCallback(&htim12);
}
static void counted_steps(void) {
    uint32_t rates[]={10,100,1000,2000};
    for(unsigned rate=0;rate<4;rate++){
        reset();init_stepper();TMC2130_SetStepTarget(&stepper,17);
        assert(TMC2130_StartStepClock(&stepper,rates[rate])==HAL_OK);
        for(unsigned i=0;i<100;i++)edge();
        assert(TMC2130_GetEmittedSteps(&stepper)==17 && !stepper.step_clock_running);
        TMC2130_SetDirection(TMC2130_DIR_REVERSE);TMC2130_SetStepTarget(&stepper,-9);
        mock_tim12.CNT=65533;assert(TMC2130_StartStepClock(&stepper,rates[rate])==HAL_OK);
        for(unsigned i=0;i<100;i++)edge();
        assert(TMC2130_GetEmittedSteps(&stepper)==-9 && !stepper.step_clock_running);
    }
    reset();init_stepper();TMC2130_SetStepTarget(&stepper,100);assert(TMC2130_StartStepClock(&stepper,10)==HAL_OK);
    edge();uint32_t deadline=mock_tim12.CCR2;mock_tim12.CNT+=100;
    for(unsigned i=0;i<50;i++)assert(TMC2130_SetStepFrequency(&stepper,100)==HAL_OK);
    assert(mock_tim12.CCR2==deadline && mock_tim12.CNT==(uint16_t)(deadline-50000+100));
    TMC2130_StopStepClock(&stepper);assert(stepper.step_clock_running);edge();assert(!stepper.step_clock_running);
    puts("PASS: exact STEP counts at 10/100/1000/2000 Hz, reversal, timer wrap and phase-preserving updates");
}
static void stepper_owner(void) {
    reset();init_stepper();TMC2130Motion_Init(&g_stepper_motion,&stepper,NULL);UI_InitCommandQueues();
    UI_StepperEnable(true);assert(!g_stepper_motion.enabled);UI_ProcessStepperCommands();assert(g_stepper_motion.enabled);
    UI_StepperMoveTo(17);assert(g_stepper_motion.target_steps==0);UI_ProcessStepperCommands();assert(g_stepper_motion.target_steps==17);
    for(unsigned us=0;us<2000000;us++){
        mock_tim12.CNT=(uint16_t)(mock_tim12.CNT+1);
        if(stepper.step_clock_running && mock_tim12.CNT==mock_tim12.CCR2)HAL_TIM_OC_DelayElapsedCallback(&htim12);
        if(us%1000==0){ticks++;TMC2130Motion_Task(&g_stepper_motion,.001f);}
    }
    assert(!Fault_IsCriticalActive());assert(g_stepper_motion.position_steps==17);assert(stepper.emitted_steps==17);
    UI_StepperMoveTo(-18);UI_ProcessStepperCommands();
    for(unsigned us=0;us<2000000;us++){
        mock_tim12.CNT=(uint16_t)(mock_tim12.CNT+1);
        if(stepper.step_clock_running && mock_tim12.CNT==mock_tim12.CCR2)HAL_TIM_OC_DelayElapsedCallback(&htim12);
        if(us%1000==0){ticks++;TMC2130Motion_Task(&g_stepper_motion,.001f);}
    }
    assert(!Fault_IsCriticalActive());assert(stepper.emitted_steps==-18);
    UI_StepperMoveTo(100000);UI_ProcessStepperCommands();
    for(unsigned us=0;us<1000000;us++){
        mock_tim12.CNT=(uint16_t)(mock_tim12.CNT+1);
        if(stepper.step_clock_running && mock_tim12.CNT==mock_tim12.CCR2)HAL_TIM_OC_DelayElapsedCallback(&htim12);
        if(us==200000){UI_StepperStop();UI_ProcessStepperCommands();}
        if(us%1000==0){ticks++;TMC2130Motion_Task(&g_stepper_motion,.001f);}
    }
    assert(!Fault_IsCriticalActive());assert(!stepper.step_clock_running);
    assert(g_stepper_motion.state==TMC2130_MOTION_IDLE);
    UI_StepperMoveTo(1000);UI_ProcessStepperCommands();TMC2130Motion_Task(&g_stepper_motion,.001f);
    assert(TMC2130Motion_EmergencyStop(&g_stepper_motion)==HAL_OK);
    assert(Fault_GetActive()&FAULT_USER_STOP);assert(!mock_tim12.DIER);
    assert(!g_stepper_motion.enabled && g_stepper_motion.state==TMC2130_MOTION_FAULT);
    g_fault_bits=0; /* New independent overflow case in this fixture. */
    UI_SetBldcSpeed(20);assert(UI_GetControlSource()==UI_CONTROL_API);
    for(unsigned i=0;i<17;i++) { UI_StepperMoveTo(1); }
    assert(Fault_IsCriticalActive());
    puts("PASS: queued ownership, finite forward/reverse motion, decelerating stop and queue overflow inhibit");
}
static void watchdog_health(void) {
    reset();Fault_WatchdogStart();mock_iwdg.KR=0;
    ticks=200;Fault_Heartbeat(FAULT_HEALTH_MOTOR);Fault_Heartbeat(FAULT_HEALTH_HID);Fault_Heartbeat(FAULT_HEALTH_STEPPER);
    Fault_WatchdogPoll();assert(mock_iwdg.KR==0xaaaa);
    ticks=501;mock_iwdg.KR=0;Fault_WatchdogPoll();assert(Fault_GetActive()&FAULT_CPU);assert(mock_iwdg.KR==0);
    puts("PASS: watchdog feeds only with fresh motor, HID and stepper health");
}
static void rotor_wrap(void) {
    uint32_t boundaries[]={0x7ffffff0u,0xfffffff0u};
    for(unsigned i=0;i<2;i++) {
        reset();mock_tim2.CNT=boundaries[i];calibrated();
        int32_t before=g_angle_mod;
        mock_tim2.CNT+=64u;sample();
        assert((g_angle_mod-before+MOTOR_FOC_ENCODER_CPR)%MOTOR_FOC_ENCODER_CPR==64);
    }
    puts("PASS: electrical angle remains continuous at signed and hardware encoder wrap");
}
static void current_sampling(void) {
    reset();calibrated();
    assert((mock_tim1.CCMR2&TIM_CCMR2_OC4M)==(TIM_OCMODE_PWM2<<8));
    assert(mock_tim1.CCR4==2812u-MOTOR_FOC_ADC_TRIGGER_LEAD_COUNTS);
    assert(fabsf(g_foc.v_limit-15.0f*(MOTOR_FOC_DUTY_MAX-0.5f)*1.1547005f)<1e-4f);
    pwm_set_duty_abc(1.0f,1.0f,1.0f);assert(mock_tim1.CCR1==(uint32_t)(MOTOR_FOC_DUTY_MAX*2812.0f));
    MotorFOC_RequestArm();owner_tick();assert(MotorFOC_GetState()==MOTOR_FOC_STATE_ALIGN);
    /* Phase A at high duty: its garbage sample is replaced by -(ib+ic). */
    pwm_set_duty_abc(0.93f,0.5f,0.5f);hadc1.raw=65535;hadc2.raw=32768+1000;hadc3.raw=32768-1000;sample();
    assert(!Fault_IsCriticalActive());assert(fabsf(g_foc.ia)<1e-4f);assert(g_foc.ib>0.2f && g_foc.ic<-0.2f);
    /* Two untrusted phases: hold the previous currents. */
    float ia=g_foc.ia,ib=g_foc.ib,ic=g_foc.ic;
    pwm_set_duty_abc(0.93f,0.93f,0.1f);hadc1.raw=hadc2.raw=65535;hadc3.raw=32768;sample();
    assert(!Fault_IsCriticalActive());assert(g_foc.ia==ia && g_foc.ib==ib && g_foc.ic==ic);
    for(unsigned i=1;i<MOTOR_FOC_MAX_HELD_SAMPLES;i++){pwm_set_duty_abc(0.93f,0.93f,0.1f);sample();}
    assert(!Fault_IsCriticalActive());pwm_set_duty_abc(0.93f,0.93f,0.1f);sample();
    assert(Fault_GetActive()&FAULT_CURRENT);
    reset();calibrated();MotorFOC_RequestArm();owner_tick();
    /* A trusted phase out of range still trips. */
    pwm_set_duty_abc(0.5f,0.5f,0.5f);hadc1.raw=65535;hadc2.raw=hadc3.raw=32768;sample();
    assert(Fault_GetActive()&FAULT_CURRENT);
    puts("PASS: ADC trigger centred on the low-side window, duty cap, high-duty phase reconstruction and bounded hold");
}
int main(void) {
    driver_frames();startup_stop_restart();faults_and_adc();bounded_input();counted_steps();stepper_owner();watchdog_health();rotor_wrap();current_sampling();
    puts("All firmware regression scenarios passed.");return 0;
}
