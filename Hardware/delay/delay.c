#include "delay.h"
/*************************************************************************************************************
*�������ƣ�delay_nms()
*��   �� ��uint16_t n   ��ʱN������
*����ֵ  : void
*����������������ʱN�����룬�ⲿ����
**************************************************************************************************************/
void delay_nms(uint16_t n) 
{
	uint16_t  a,b;
	for (a=0;a<n;a++)
	{
		for (b=0;b<9000;b++);
	}
}


/*******************************************************************************************************
** ��������: DelayNms
** ��������: ��ʱN ms
** �䡡��: pdata        ���񸽼Ӳ���(ָ��)
** �䡡��: ��
********************************************************************************************************/
void DelayNms(uint8_t delx)
{
    uint16_t  i,j,k=0;
    while(k<delx)
    {
        k++;
        for(i = 0; i <= 2200*2; i ++)
        {
            j ++;
            j --;
        }
    }
}