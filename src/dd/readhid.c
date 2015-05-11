#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <fcntl.h>


int main(int argc, char *argv[])
{
    unsigned char buf;
    unsigned char data[24];
    int n;
    int i;
    int maxfd;
    fd_set readfs;
    struct timeval timeout;
    int res;
    int pos=0;

  //  system("stty -F /dev/ttyUSB0 4800 cs8 -cstopb ");

    int myhid = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY | O_NDELAY);

    if(myhid == -1)
    {
	perror("open_port:Unable to open /dev/ttyUSB0");
    }
    else
    {

	struct termios port_settings;
	cfsetospeed(&port_settings, B9600);
	port_settings.c_cflag &= ~PARENB;
	port_settings.c_cflag &= ~CSTOPB;
	port_settings.c_cflag &= ~CSIZE;
	port_settings.c_cflag |= CS8;

	cfmakeraw(&port_settings);
	tcsetattr(myhid, TCSANOW, &port_settings);
	printf("端口配置完毕！\n");

    while(1)
    {
	maxfd = myhid+1;
	FD_SET(myhid, &readfs);
	timeout.tv_usec = 0;
	timeout.tv_sec = 1;
	res=select(maxfd, &readfs, NULL, NULL, &timeout);
	if(res == 0) continue;
        n = read(myhid, (void *)&buf, 1);

	if(n>0)
	{
	    if( 0xff == (unsigned int)buf)
	    {
		data[0] = buf;
		pos=1;
	    }
	    else
	    {

		data[pos]=buf;
		pos++;
		if(pos == 24 && data[pos-1]==0xfe)
		{
		    pos=0;
		    printf("\n****************************************************************************\n");
		    printf("接收到24byte数据：");
		    for(i=0; i<24; i++)
		    {
			    printf("%02x ", data[i]);
		    }
		    printf("\n");
		    printf("测量结果：\n");

		    printf("高压:%dmmHg\t 低压:%dmmHg\t 脉搏:%d/min\n", data[10], data[11], data[12]);
		    unsigned char ResultDataDA, ResultDataHO;
		    unsigned char year, month, day, hour, minute;

		    year = data[13];
		    ResultDataDA = data[14];
		    ResultDataHO = data[15];
		    minute = data[16] &0x3f;

		    month = (ResultDataDA & 0xc0)/64 + (ResultDataHO & 0xc0)/16;
		    day = ResultDataDA &0x3f;
		    hour = ResultDataHO &0x3f;
	
		    printf("测量日期:20%02d-%02d-%02d\t 测量时间:%02d:%02d \n", year, month, day, hour, minute );
		    printf("****************************************************************************\n\n\n");

		}
		else if(24==pos)
		{
		    printf("无效数据：");
		    for(i=0; i<pos; i++)
		    {
			    printf("%02x ", data[i]);
		    }
		    printf("\r");
		    pos=0;
		}

	    }
	}
	else if (n==0)
	{
	    printf("read  returns 0\n");
	}
	else
	{
	    printf("read  returns -1, errno:%u \n", n, errno);
	}


    }



    close(myhid);
    }

    return 0;
}
