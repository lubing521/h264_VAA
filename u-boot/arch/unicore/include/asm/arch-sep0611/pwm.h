/*********************************************************************
* Filename    :  pwm.h
* Description :  This file is used to define the PWM 
* Date        :  2010-03-22
* Author      :  SixRoom
*
**********************************************************************/
#ifndef PWM_H_
#define PWM_H_

#include 	<asm/arch/vic.h>


/***********************************************************
*	Function	��	pwm_mode
*	Parameter	��	pwmnum	��PWM�ţ�
* 					mode	��ģʽ��PWMģʽ������GPIO��һ�μ���ģʽ�����ڼ���ģʽ��
*	Return		��  
*	Description	��	
***********************************************************/
#define PWM_SetMode(pwmnum,mode)  						\
do{                              					\
      	*(RP)PWM_CTRL_CH(pwmnum) &= 0xfffffffc; 	\
		*(RP)PWM_CTRL_CH(pwmnum) |= (mode);  		\
  }while(0) 
/***********************************************************
*	Function	��	pwm_prescale
*	Parameter	��	pwmnum	��PWM�ţ�
* 					div		����Ƶֵ��
*	Return		��  
*	Description	��	
***********************************************************/
#define PWM_SetPreScale(pwmnum,div)    		\
do{                              			\
		*(RP)PWM_CTRL_CH(pwmnum) = (div); 	\
  }while(0) 

/***********************************************************
*	Function	��	pwm_prescale
*	Parameter	��	pwmnum	��PWM�ţ�
*	Return		��  
*	Description	��	
***********************************************************/
#define PWM_Enable(pwmnum, sta)  				\
do{                              				\
	*(RP)PWM_ENABLE &= ~(1<<(pwmnum-1));  		\
    *(RP)PWM_ENABLE |= ((sta)<<(pwmnum-1));  	\
  }while(0)

/***********************************************************
*	Function	��	pwm_clr_int
*	Parameter	��	pwmnum	��PWM�ţ�
*	Return		��  
*	Description	��	
***********************************************************/
#define PWM_ClrInt(pwmnum)  				\
do{                              			\
      *(RP)PWM_INT |= (1<<(pwmnum-1));  	\
  }while(0) 
/***********************************************************
*	Function	��	pwm_int_mask
*	Parameter	��	pwmnum	��PWM�ţ�
*	Return		��  
*	Description	��	
***********************************************************/
#define PWM_MaskInt(pwmnum, sta)  				\
do{                              				\
	 *(RP)PWM_INTMASK &= ~(1<<(pwmnum-1));  	\
     *(RP)PWM_INTMASK |= (sta<<(pwmnum-1));  	\
  }while(0) 

//***********************************************************
//		Function	��	pwm_gpio_dir
//		Parameter	��	pwmnum		��PWM�ţ�
//						direction	������
//		Description	��	PWM��Ϊ����GPIO��ģʽʱ�Ķ˿ڷ������ú���
//***********************************************************
#define PWM_SetHGDir(pwmnum,direction)    				\
do{                              						\
      	*(RP)PWM_CTRL_CH(pwmnum) &= 0xfffffff7;  		\
		*(RP)PWM_CTRL_CH(pwmnum) |= ((direction)<<3); 	\
  }while(0) 
//***********************************************************
//		Function	��	pwm_mode_gpio_read
//		Parameter	��	pwmnum		��PWM�ţ�
//		Description	��	PWM��Ϊ����GPIO��ģʽʱ���˿�ֵ
//***********************************************************
#define PWM_ReadHGPin(pwmnum)				\
		*(RP)PWM_DATA_CH(pwmnum)
//***********************************************************
//		Function	��	pwm_mode_gpio_write
//		Parameter	��	pwmnum		��PWM�ţ�
//						data		����д���ݣ�
//		Description	��	PWM��Ϊ����GPIO��ģʽʱ��ָ���˿�дֵ
//***********************************************************
#define PWM_WriteHGPin(pwmnum,data)  		\
do{                              				\
		*(RP)PWM_DATA_CH(pwmnum)=(data);		\
}while(0) 		

//***********************************************************
//		Function	��	pwm_timer_matchout
//		Parameter	��	pwmnum		��PWM�ţ�
//						mode		����д���ݣ�
//		Description	��	PWM����ƥ��ֵ
//***********************************************************  
#define PWM_SetMatchMode(pwmnum,mode)    				\
do{                              						\
      	*(RP)PWM_CTRL_CH(pwmnum) &= 0xffffffcf;  		\
		*(RP)PWM_CTRL_CH(pwmnum) |= ((mode)<<4); 		\
  }while(0) 

//***********************************************************
//		Function	��	pwm_period
//		Parameter	��	pwmnum		��PWM�ţ�
//						period		����ʱ���ڣ�
//		Description	��	PWM��������
//***********************************************************
#define PWM_SetPeriod(pwmnum,period)    		\
do{                              				\
		*(RP)PWM_PERIOD_CH(pwmnum) = (period); 	\
  }while(0)
//***********************************************************
//		Function	��	pwm_dutycycle
//		Parameter	��	pwmnum		��PWM�ţ�
//						pulse		��ռ�ձȣ�
//		Description	��	PWMռ�ձ�����
//***********************************************************
#define PWM_SetDutyCycle(pwmnum,pulse)    		\
do{                              				\
		*(RP)PWM_DATA_CH(pwmnum) = (pulse); 	\
  }while(0) 

#define PWM_SetHGData(pwmnum,data)    		\
do{                              			\
		*(RP)PWM_DATA_CH(pwmnum) = (data); 	\
//***********************************************************
//		Function	��	pwm_currentvalue
//		Parameter	��	pwmnum	��PWM�ţ�
//		Description	��	PWM��ȡ��ǰֵ
//***********************************************************
#define PWM_ReadCountValue(pwmnum)   				\
			*(RP)PWM_CNT_CH(pwmnum)	
//***********************************************************
//		Function	��	pwm_fifostatus
//		Parameter	��	pwmnum	��PWM�ţ�
//		Description	��	PWM��ȡ��ǰFIFO״̬
//***********************************************************
#define PWM_ReadFIFOStatus(pwmnum)   				\
			*(RP)PWM_STATUS_CH(pwmnum)


extern void PWM_Test(void);

#endif /*PWM_H_*/
