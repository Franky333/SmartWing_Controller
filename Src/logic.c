#include "logic.h"
#include "swdriver.h"
#include "tmc6200/TMC6200_highLevel.h"
#include "tmc4671/TMC4671_highLevel.h"
#include "as5147.h"
#include "usart.h"
#include "tim.h"
#include <stdbool.h>


volatile uint16_t systick_counter = 0;
volatile uint16_t systick_counter_2 = 0;

volatile uint16_t pwm_in[4] = {1500, 1500, 1500, 1500};
volatile bool pwm_updated = false;


void logic_init(void)
{
	HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, GPIO_PIN_SET);

	HAL_Delay(500);

	uint8_t i;

	for(i=0; i<4; i++) tmc6200_highLevel_init(i);
	for(i=0; i<4; i++) swdriver_setEnable(i, true);
	for(i=0; i<4; i++) TMC4671_highLevel_init(i);

	HAL_Delay(10);

	//TMC4671_highLevel_openLoopTest(3);
	//TMC4671_highLevel_printOffsetAngle(0);
	//TMC4671_highLevel_torqueTest(0);
	//TMC4671_highLevel_positionTest(3);

	for(i=0; i<4; i++) TMC4671_highLevel_positionMode_fluxTorqueRamp(i);

	// pwm inputs //FIXME: timer interrupt priority lower than spi?
	HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1);
	HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_2);
	HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_3);
	HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_4);

	//TMC4671_highLevel_pwmOff(3);
}


void logic_loop(void)
{
	static int32_t positionTarget[4] = {0, 0, 0, 0};

	if(pwm_updated)
	{
		pwm_updated = false;

		uint8_t i;

		for(i=0; i<4; i++)
		{
			positionTarget[i] = (int32_t)pwm_in[i] - 1500;
			//positionTarget[i] = (int32_t)((float)positionTarget[i] * 16383 / 500.0 + 0.5); //2731.0 --> +-15°
			positionTarget[i] = clacAngle(i, positionTarget[i]);
		}

		for(i=0; i<4; i++) TMC4671_highLevel_setPosition_nonBlocking(i, positionTarget[i]);
	}

	if(systick_counter >= 20) //50Hz
	{
		systick_counter = 0;
	}

	if(systick_counter_2 >= 200) //5Hz
	{
		systick_counter_2 = 0;

		static char string[128];
//		uint16_t angle = as5147_getAngle(0);
//		uint16_t len = snprintf(string, 128, "driver %d encoder angle: %d (11bit) %d (16bit)\n", 0, angle, (int16_t)(angle << 5));
		uint16_t len = snprintf(string, 128, "pwm_in: %d %d %d %d\n", pwm_in[0], pwm_in[1], pwm_in[2], pwm_in[3]);
//		uint16_t len = snprintf(string, 128, "%d\n", pwm_in[3]);
//		uint16_t len = snprintf(string, 128, "%d %d %d\n", DRV0_OFFSET_ENC_PHIM, DRV0_OFFSET_ENC_PHIE, DRV0_OFFSET_PHIM_PHIE);
		HAL_UART_Transmit_IT(&huart3, (uint8_t*)string, len);
	}
}


void HAL_SYSTICK_Callback(void)
{
	systick_counter++;
	systick_counter_2++;

	// error led
	if(swdriver_getStatus(0) || swdriver_getStatus(1) || swdriver_getStatus(2) || swdriver_getStatus(3) ||
	   swdriver_getFault(0) || swdriver_getFault(1) || swdriver_getFault(2) || swdriver_getFault(3))
	{
		HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_SET);
	}
	else
	{
		HAL_GPIO_WritePin(LED_RED_GPIO_Port, LED_RED_Pin, GPIO_PIN_RESET);
	}
}


void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
	if(htim == &htim2) // pwm in
	{
		static uint32_t timestamp_risingEdge[4] = {0, 0, 0, 0};

		switch(htim->Channel)
		{
			case HAL_TIM_ACTIVE_CHANNEL_1:
				if(HAL_GPIO_ReadPin(PWM_IN_0_GPIO_Port, PWM_IN_0_Pin))
				{
					timestamp_risingEdge[0] = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1); //rising edge
					pwm_updated = true;
				}
				else
				{
					uint16_t pwm = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1) - timestamp_risingEdge[0]; //falling edge
					if(pwm < 1000) pwm = 1000;
					else if(pwm > 2000) pwm = 2000;
					pwm_in[0] = pwm;
				}
				break;
			case HAL_TIM_ACTIVE_CHANNEL_2:
				if(HAL_GPIO_ReadPin(PWM_IN_1_GPIO_Port, PWM_IN_1_Pin)) timestamp_risingEdge[1] = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2); //rising edge
				else
				{
					uint16_t pwm = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2) - timestamp_risingEdge[1]; //falling edge
					if(pwm < 1000) pwm = 1000;
					else if(pwm > 2000) pwm = 2000;
					pwm_in[1] = pwm;
				}
				break;
			case HAL_TIM_ACTIVE_CHANNEL_3:
				if(HAL_GPIO_ReadPin(PWM_IN_2_GPIO_Port, PWM_IN_2_Pin)) timestamp_risingEdge[2] = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_3); //rising edge
				else
				{
					uint16_t pwm = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_3) - timestamp_risingEdge[2]; //falling edge
					if(pwm < 1000) pwm = 1000;
					else if(pwm > 2000) pwm = 2000;
					pwm_in[2] = pwm;
				}
				break;
			case HAL_TIM_ACTIVE_CHANNEL_4:
				if(HAL_GPIO_ReadPin(PWM_IN_3_GPIO_Port, PWM_IN_3_Pin)) timestamp_risingEdge[3] = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_4); //rising edge
				else
				{
					uint16_t pwm = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_4) - timestamp_risingEdge[3]; //falling edge
					if(pwm < 1000) pwm = 1000;
					else if(pwm > 2000) pwm = 2000;
					pwm_in[3] = pwm;
				}
				break;
			default:
				break;
		}
	}
}

int32_t clacAngle(uint8_t drv, int32_t positionTarget)
{
	float alpha = 0;

	alpha =  ( (float)positionTarget/ 500.0 * ANGLE_MAX_ALPHA_DEGREE ); // in degree

	if(drv == 1 || drv == 3)
		alpha = 2.490378*alpha + 0.001711*alpha*alpha + 0.000138*alpha*alpha*alpha;

	return (int32_t)( alpha * 2.0  / 360.0 * 65536.0);
}
