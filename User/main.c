/******************************************************************************

                  ��Ȩ���� (C), 2001-2013, ���ֺ������ͨ�ż������޹�˾

 ******************************************************************************
  �� �� ��   : main.c
  �� �� ��   : ����
  ��    ��   :
  ��������   : 2017��8��7��
  ����޸�   :
  ��������   :
******************************************************************************/
#include "config.h"
#include "main.h"
#include "user_config.h"
#include "gpio_config.h"
#include "user_app_task.h"
#include "queue.h"
#include "Cmd_process.h"

#include "lpc177x_8x_eeprom.h"
#include "uart1.h"
#include "uart2.h"

#include "ADC.h"
//********************************************************************************
//************************************ȫ�ֱ�������*******************************
//********************************************************************************
const  char  SVersion[] = "4.10\0";//ģ�������汾��
const  char  type[5][5] = {"FSW\0", "OTDR\0", "OPM\0", "WDM\0", "OS\0"};
uint8  cfm[20];                    //ȷ��buf
uint8  run_flag;                   //LED ���е�
uint8  scan_flag;                  //ɨ���־
uint16 srch;                       //ɨ��ʵʱͨ��
uint8  stime;                      //ɨ��ʱ��
uint8  ADDR;                       //ҵ���̵�ַ
uint8  OPS_CH;                     //�⿪��ͨ����
uint8  u2RcvBuf[120];              //����TaskUart0Revice �õ�����
#ifdef  TYPE_OPM
    uint8 hardware_way[CHANNEL_NUM];  //��ʵ��Ӳ����λ
    struct EPROM_DATA EPROM;           //����EPROM���ò����Ľṹ��
    uint8 PD_INFO[120];
    uint8 MD_INFO[120];
#endif

OS_EVENT *Uart2RcvMbox;            //���� 2 ��������

OS_STK TaskWDTStk[TASK_STK_SIZE];
OS_STK TaskUart2CmdStk[TASK_STK_SIZE*2];
OS_STK TaskUart2RcvStk[TASK_STK_SIZE*2];
OS_STK TaskSvrStk[TASK_STK_SIZE*2];


#ifdef  TYPE_OPM
    OS_STK TaskCollectAStk[TASK_STK_SIZE*2];
    OS_STK TaskCollectBStk[TASK_STK_SIZE*2];
	OS_STK TaskSwitchStk[TASK_STK_SIZE - 1];
#endif
/********************************************************************************************************
** ��������: restore_set()
** ��������: ����Ĭ������
********************************************************************************************************/
void restore_set(void)
{
    EPROM.BPS = 9;            //���ڲ�����(115200)

    //�ٸ�EEPROM��ʼ��,��ֹEEPROM������,�޷�����
    LPC1778_EEPROM_Init();
}

/********************************************************************************************************
** ��������: systemInt
** ��������: ��ʼ��
********************************************************************************************************/
void READ_EPROM_Init(void)
{
    LPC1778_EEPROM_Init();     //I2C ��ʼ��

    EEPROM_Read_Str( 0x00, (uint8 *)&EEPROM_BASE_ADDRESS, sizeof(struct EPROM_DATA) );
    delay_nms(20);
    //�ٶ�һ�η�ֹ��������
    EEPROM_Read_Str( 0x00, (uint8 *)&EEPROM_BASE_ADDRESS, sizeof(struct EPROM_DATA) );
	  
	EPROM.BPS = 9;
	stime = 1;
}

/********************************************************************************************************
** ��������: HW_Init
** ��������: Ӳ����ʼ��
********************************************************************************************************/
void HW_Init(void)
{	
	ONLINE_PIN_INIT;
	OFFLINE;
	RUN_LED_INIT;
	
#ifdef TYPE_FSW
	OPS_PIN_INIT;
	OPS_RST;
	delay_nms(100);
	OPS_NRST;
    EPROM.TYPE = FSW;
	LPC_GPIO4->CLR |= (0x7F<<5);//�⿪�ظ�λ����һ· 
#endif
	
#ifdef TYPE_OTDR
    OTDR_INIT;
	OTDR_PWR_ON;
    EPROM.TYPE = OTDR;
#endif
	
#ifdef TYPE_WDM
    EPROM.TYPE = WDM;
#endif
		
#ifdef TYPE_OPM
    EPROM.TYPE = OPM;
	ADC_int(400);
#endif

#ifdef TYPE_OS
    OS_PIN_INIT;
    EPROM.TYPE = OS;
#endif
	
	
	READ_EPROM_Init();
	UART0Init();
	UART1_Init(115200);
	UART2_Init(115200);
	ONLINE;
	ADDR = ((LPC_GPIO4->PIN>>0) & 0x1f) + 1;
	EPROM.address = ADDR;

	/*******************�������Ź�********************/
	/**LPC1778�Ŀ��Ź�ʹ���ڲ�RCʱ��(500KHz),����4�η�Ƶ(500K/4=125K ,��ʮ������Ϊ0X1E848)**/
	LPC_WDT->TC  = 0X1E848;    //����WDT��ʱֵΪ1��.
	LPC_WDT->MOD = 0x03;       //����WDT����ģʽ,����WDT	



}

/********************************************************************************************************
** ��������: TaskInfo
** ��������: ����״̬
********************************************************************************************************/
void scan(void)
{
	  uint8  i, temp;
	
		if(scan_flag)
		{
			begin:
			  if(scan_flag == 1)
				{
						for(i=0; i<7; i++)
						{
								temp = ((EPROM.sbch - 1) >> i) & 0x01;
								if(temp)
										LPC_GPIO4->SET |= (1<<(5+i));
								else
										LPC_GPIO4->CLR |= (1<<(5+i));
						}
						srch = EPROM.sbch;
						OPS_CH = EPROM.sbch - 1;
						scan_flag = 2;
				}
				else
				{
					  srch++;
					  if(EPROM.sech > EPROM.sbch)
						{
								if(srch > EPROM.sech)
								{
										scan_flag = 1;
										goto begin;
								}
						}
						else if(EPROM.sech < EPROM.sbch)
						{
								if(srch > EPROM.maxch)
										srch = 1;
								if((srch > EPROM.sech) && (srch < EPROM.sbch))
								{
										scan_flag = 1;
										goto begin;
								}
						}
						else 
						{
								scan_flag = 1;
								goto begin;
						}
						OPS_CH = srch - 1;
						for(i=0; i<7; i++)
						{
								temp = ((srch - 1) >> i) & 0x01;
								if(temp)
										LPC_GPIO4->SET |= (1<<(5+i));
								else
										LPC_GPIO4->CLR |= (1<<(5+i));
						}
				}
		}
}

/********************************************************************************************************
** ��������: TaskWDT
** ��������: ���Ź���λ
** ˵  ��: ���Ź�ʹ���ڲ�RCʱ��(4MHz),����4�η�Ƶ��T=0x1000000*1us=1s (���Ź���λ��ʱʱ��)
********************************************************************************************************/
void TaskWDT(void *pdata)
{
	uint8 delay_temp = 0;
		while(1)
		{
				LPC_WDT->FEED = 0xAA;
				LPC_WDT->FEED = 0x55;
				
				if(run_flag)
				{
						RUN_LED_H;
						run_flag = 0;
				}
				else
				{
						RUN_LED_L;
						run_flag = 1;
						sprintf((char *)cfm, "<%02d_%01u_%s>\n", ADDR,EPROM.TYPE, EPROM.MN);
						UART1Write_Str((uint8 *)cfm);
				}
				delay_temp ++;
				if(delay_temp > 10)
				{
					delay_temp = 0;
					ADDR = ((LPC_GPIO4->PIN>>0) & 0x1f) + 1;
					EPROM.address = ADDR;
				}
				OSTimeDly(500);
		}
}

/********************************************************************************************************
** ��������: TaskInfo
** ��������: ����״̬
********************************************************************************************************/
void TaskSvr(void* pdata)
{
		while(1)
		{
			  scan();
				OSTimeDly(100 * stime);
		}
}

/********************************************************************************************************
** ��������: TaskUart0Cmd
** ��������: �������
********************************************************************************************************/
void TaskUart2Cmd(void* pdata)
{
		uint8  err;
		uint16 len;

		OSTimeDly(500);            //�ȴ���ʱ
		while(1)
		{
			OSMboxPend(Uart2RcvMbox, 0, &err);         // �ȴ�������������
	        if( (len = cmd_process((char*)&u2RcvBuf)) > 0 )
			{
			    UART2Put_str(u2RcvBuf, len);
			}
		#ifdef TYPE_OPM	
			eprom_set();
		#endif
		}
}

/********************************************************************************************************
** ��������: TaskUart2Rcv
** ��������: ��COS-II�����񡣴�UART2�������ݣ���������һ֡���ݺ�ͨ����
**           Ϣ���䴫�͵�TaskStart����
********************************************************************************************************/
void TaskUart2Rcv(void* pdata)
{
		uint8 *cp;
		uint8 i,temp;

		while(1)
		{
				cp = u2RcvBuf;
				while((*cp = UART2Getch()) != '<') ;  // ��������ͷ
				cp++;   								              //������һ���ֽ�
				for (i = 0; i < 50; i++)
				{
						temp = UART2Getch();
						*cp++ = temp;
						if (temp =='\n')
						{
								while(i < 48)
								{
										*cp++ = 0;                //����ĺ��油0
										i++;
								}
								break;
						}
				}
				OSMboxAccept(Uart2RcvMbox);          //��� ����Uart0ReviceMbox
				OSMboxPost(Uart2RcvMbox, (void *)u2RcvBuf);
		}
}
/*******************************************************************************************************
** ��������: main
** ��������: ������
********************************************************************************************************/
int main (void)
{
		SystemInit();
		HW_Init();
	
		IntDisAll();  //Note:����ʹ��UCOS, ��OS����֮ǰ����,ע���ʹ���κ��ж�.
		OSInit();
		OS_CPU_SysTickInit(SystemCoreClock/OS_TICKS_PER_SEC);

		OSTaskCreate(TaskWDT,      (void *)0, &TaskWDTStk[TASK_STK_SIZE - 1],        1);
	    OSTaskCreate(TaskSvr,      (void *)0, &TaskSvrStk[TASK_STK_SIZE*2 - 1],      2);
	
#ifdef TYPE_OPM
	    OSTaskCreate(TaskCollectA,  (void *)0, &TaskCollectAStk[TASK_STK_SIZE*2 - 1], 3);
	    OSTaskCreate(TaskCollectB,  (void *)0, &TaskCollectBStk[TASK_STK_SIZE*2 - 1], 4);
	    OSTaskCreate(TaskSwitch,(void *)0, &TaskSwitchStk[TASK_STK_SIZE - 1],            5);
#endif
		OSTaskCreate(TaskUart2Cmd, (void *)0, &TaskUart2CmdStk[TASK_STK_SIZE*2 - 1], 5);
		OSTaskCreate(TaskUart2Rcv, (void *)0, &TaskUart2RcvStk[TASK_STK_SIZE*2 - 1], 6);
    
		//��������0�Ľ�������
		Uart2RcvMbox = OSMboxCreate(NULL);
		if(Uart2RcvMbox == NULL)
			  return 1;
		
		OSStart();
		return 0;
}

/********************************************************************************************************
**                            End Of File
********************************************************************************************************/