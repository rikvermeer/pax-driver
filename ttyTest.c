#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <time.h>

static volatile sig_atomic_t doneflag = 0;
static int usbhost_fd=-1;

static void setdoneflag(int signo)
{
    doneflag = 1;
}

static int initSig(void)
{
	struct sigaction act;
	act.sa_handler = setdoneflag;
	act.sa_flags = 0;
	if((sigemptyset(&act.sa_mask) == -1)||
		(sigaction(SIGINT,&act,NULL) == -1))
	{
		perror("Failed to set SIGINT handler");
		return 1;
	}

	return 0;
}

int OpenSerial(unsigned char *SerialName)
{
    int fd;
	fd = open(SerialName,O_RDWR|O_NOCTTY|O_NDELAY);
	if(fd <0)
	{
        printf("%s line:%d errno:%d\n",__func__,__LINE__,errno);
		return fd;
	}
#if 1
	if(fcntl(fd,F_SETFL,0) < 0)
		printf("%s fcntl failed!\n",__func__);
	else
		printf("%s fcntl=%d\n",__func__,fcntl(fd,F_SETFL,0));	
#endif
	return fd;
}

int CloseSerial(int fd)
{
	if(close(fd) <0)
	{
        perror("close");
        return -1;
	}

    return 0;
}

int setSerial(int fd,int nSpeed,int nBits,char nEvent,int nStop)
{
	struct termios newtio,oldtio;

	if(tcgetattr(fd,&oldtio) != 0)
	{
		printf("%s line:%d errno:%d\n",__func__,__LINE__,errno);
		return 1;
	}

	bzero(&newtio,sizeof(newtio));
	
	//1:Setting char size
	newtio.c_cflag |= CLOCAL | CREAD;
	newtio.c_cflag &= ~CSIZE;

	//2:Setting bits
	switch(nBits)
	{
	case 7:
		newtio.c_cflag |= CS7;
		break;

	case 8:
		newtio.c_cflag |= CS8;
		break;
	}
	
	//3:Setting parity
	switch(nEvent)
	{
	case 'O':
		newtio.c_cflag |= PARENB;
		newtio.c_cflag |= PARODD;
		newtio.c_cflag |= (INPCK | ISTRIP);
		break;
	
	case 'E':
		newtio.c_cflag |= (INPCK | ISTRIP);
		newtio.c_cflag |= PARENB;
		newtio.c_cflag &= ~PARODD;
		break;

	case 'N':
		newtio.c_cflag &= ~PARENB;
		break;
	}

	//4:Setting speed
	switch(nSpeed)
	{
	case 2400:
		cfsetispeed(&newtio,B2400);
		cfsetospeed(&newtio,B2400);
		break;
	
	case 4800:
		cfsetispeed(&newtio,B4800);
		cfsetospeed(&newtio,B4800);
		break;

	case 9600:
		cfsetispeed(&newtio,B9600);
		cfsetospeed(&newtio,B9600);
		break;

	case 115200:
		cfsetispeed(&newtio,B115200);
		cfsetospeed(&newtio,B115200);
		break;

	case 460800:
		cfsetispeed(&newtio,B460800);
		cfsetospeed(&newtio,B460800);
		break;

	default:
		cfsetispeed(&newtio,B115200);
		cfsetospeed(&newtio,B115200);
		break;
	}

	//5:Setting stop bits
	switch(nStop)
	{
	case 1:
		newtio.c_cflag &= ~CSTOPB;
		break;

	case 2:
		newtio.c_cflag |= CSTOPB;	
		break;
	}

	//6:Setting recvice timeout and mini bytes  size of recvice
	newtio.c_cc[VTIME] = 0;
	newtio.c_cc[VMIN] = 0;

	//7:flush input and output buffer
	tcflush(fd,TCIOFLUSH);
	
	if(tcsetattr(fd,TCSANOW,&newtio) != 0)
	{
		printf("%s_%d-errno:%d\n",__func__,__LINE__,errno);
		return 2;
	}
	
	printf("serial setting done\n");
	
	return 0;
}

/*
return 
<0:occured a error
nozero: lengths of write data
*/
int WriteSerial(int fd,unsigned char *buf,unsigned int length)
{
    int ret;

    if(fd<0)
    {
        //printf("%s-Invaid fd\n",__func__);
        return -1;
    }
    
    ret = write(fd, buf, length);
	if(ret < 0)
	{
        //printf("%s line:%d errno:%d\n",__func__,__LINE__,errno);
        return -1;
	}    
    
    return ret;
}

/*
return 
<0:occured a error
nozero: lengths of read data
*/

int ReadSerial(int fd,unsigned char *buf,unsigned int length,unsigned int msTimeouts)
{
    struct timeval timeout;
    fd_set rset;	
    int ret;

    if((fd<0)||(fd>=FD_SETSIZE))
    {
        //printf("%s-Invalid fd\n",__func__);
        return -1;
    }

    if(msTimeouts)
    {   
        //printf("msTimeouts:%d\n",msTimeouts);
        timeout.tv_sec = (long)(msTimeouts/1000);
        timeout.tv_usec = (long)((msTimeouts%1000)*1000);
        //printf("sec:%d,usec:%d\n",timeout.tv_sec,timeout.tv_usec);

    	FD_ZERO(&rset);
    	FD_SET(fd,&rset);

        if(select(fd+1,&rset,NULL,NULL,&timeout)<0)
        {
            //printf("%s line:%d errno:%d\n",__func__,__LINE__,errno);
            return -2;
        }

        if(!FD_ISSET(fd,&rset))
        {
            //printf("timeout:%s line:%d-errno:%d\n",__func__,__LINE__,errno);
            return 0;//return -3;//timeout
        }
    }

    ret = read(fd,buf,length);
    if(ret < 0)
    {
        //printf("%s line:%d errno:%d\n",__func__,__LINE__,errno);
        return -4;
    }
    
    return ret;
}

unsigned char TxBuf[70000],RxBuf[70000];

void ServerTest(int fd)
{
	int rLen=0,wLen=0,ret=0,tn=0;
	unsigned int i=0;
	unsigned long long count=0,bcount;
	struct timeval tpend,tpstart;
	unsigned int speed=0;

    printf("%s\n",__func__);

	count = 0;
    bcount = 0;
	gettimeofday(&tpstart,NULL);

    while(doneflag ==0)
    {            
		rLen = ReadSerial(fd,RxBuf,sizeof(RxBuf),10000);
        if(rLen < 0)
        {
            printf("ReadSerial Error:%d,errno:%d\n",rLen,errno);
            goto byebye;
        }

        wLen = 0;
       	while(doneflag ==0)
	    {
        	ret = WriteSerial(fd, RxBuf+wLen, rLen-wLen);
        	if(ret < 0)
        	{
                printf("WriteSerial Error:%d,errno:%d\n",ret,errno);
                goto byebye;
        	}
            wLen += ret;

            if(wLen >= rLen)break;
 	    }       

        count++;
        bcount+=rLen*2;
		if(count%100==0) 
		{
			unsigned int time;
			gettimeofday(&tpend,NULL);
			time=tpend.tv_sec - tpstart.tv_sec;
			if(time ==0)
				time=1;
			speed = bcount/time;
			printf("\rSpeed:%d,bcount:%llu",speed,bcount);
			fflush(stdout); 
		}
    }

byebye:    
    doneflag=0;
}

void RecvFile(int fd)
{
	volatile int rLen=0,wLen=0,ret=0,tn=0;
	unsigned int i=0,time;
	unsigned int count=0;
	struct timeval tpend,tpstart;
	unsigned int speed=0,maxdata=0;
	unsigned char buf[100];

	printf("RecvFile Test\n");

	count = 0;

	while(doneflag==0)
	{
		ret = ReadSerial(fd,RxBuf,1024,5000);
		if(ret <= 0)continue;
		count=ret;
		break;
	}
	gettimeofday(&tpstart,NULL);
	printf("Recving...\n");
	
	while(doneflag==0)
	{
		ret = ReadSerial(fd,RxBuf,sizeof(RxBuf),300);
		if(ret <= 0)break;
		count +=ret;
	}

	gettimeofday(&tpend,NULL);
	time=tpend.tv_sec - tpstart.tv_sec;
	if(time==0)time=1;
	speed=count/time;
	printf("FileLen:%d,time:%d,Speed:%d\n",count,time,speed);
	fflush(stdout);

	doneflag=0;
}

void SendFile(int fd)
{
	int ret=0;
    unsigned int cnt;

    printf("%s\n",__func__);

    cnt=0;
    while(doneflag ==0)
    {
    	ret = WriteSerial(fd, TxBuf, 2048);
    	if(ret < 0)
    	{
            //printf("WriteSerial Error:%d\n",ret);
    	}
        else
        {
            cnt+=ret;
        }
    }

    printf("send len:%d\n",cnt);
byebye:    
    doneflag=0;
}

void usb_host_test(unsigned char *devName)
{
    unsigned char buf[100];
    int ret,len;
    
	printf("devName:%s\n",devName);
    while(1)
    {
        printf("1-OpenHost  2-CloseHost\n");
        printf("3-Tx        4-Rx\n");
        printf("5-TxCheck   6-FlushInOutBuffer\n");
        printf("7-TxRxs\n");
        //printf("7-TxRxs     8-RecvFile\n");
        //printf("9-SendFile  0-tcdrain\n");
        printf("Q-Quit\n");

		memset(buf,0,sizeof(buf));
		
        scanf("%s",buf);
        switch(buf[0])
        {
        case '1':
		if(usbhost_fd>=0)
				printf("REOPEN!\n");
			ret = OpenSerial(devName);
			if(ret<0)
			{
				printf("OpenSerial %s err:%d\n",devName,ret);
				if(usbhost_fd<0)break;
			}
    	    
			if(usbhost_fd<0)
			{
				usbhost_fd = ret;
				ret = setSerial(usbhost_fd,115200,8,'N',1);
				if(ret != 0)
				{
					printf("setSerial err:%d\n",ret);
					CloseSerial(usbhost_fd);
					usbhost_fd = -1;
					break;
				}
			}
			else
			{
				CloseSerial(ret);
			}

            break;

        case '2':
			if(usbhost_fd>=0)
				CloseSerial(usbhost_fd);
			usbhost_fd = -1;
            break;

        case '3':
            printf("Please input write data-less than 100 bytes\n");
            memset(buf,0,sizeof(buf));
            scanf("%s",buf);
            ret = WriteSerial(usbhost_fd, buf, strlen(buf));
            printf("WritSerial ret:%d,errno:%d\n",ret,errno);
			//ret = tcdrain(usbhost_fd);
			//printf("tcdrain ret:%d\n",ret);
            break;

        case '4':
            ret = ReadSerial(usbhost_fd, buf, sizeof(buf), 1000);
            printf("ReadSeial ret:%d,errno:%d\n",ret,errno);
            break;
		case '5':
			ret = ioctl(usbhost_fd, TIOCOUTQ, &len);
			if(ret<0)
			{
				printf("tx pool is failed errno:%d!\n",errno);
			}
			else
			{
				printf("tx pool :%d\n",len);
			}
			break;
        case '6':
            ret = tcflush(usbhost_fd,TCIOFLUSH);
            printf("Flush IN OUT buffer:%d\n",ret);
            break;

        case '7':
            ServerTest(usbhost_fd);
            break;

        case '8':   
            RecvFile(usbhost_fd);
            break;  
        case '9':
            SendFile(usbhost_fd);
            break;
        case 'q':
        case 'Q':
            return;
        }
    }
}

int main(int argc, char *argv[])
{
	int fd;
    int flags;
	int ret;
    unsigned char SerialName[30]={0};
    unsigned char buf[100];

	printf("ttyTest %s\n","2016.12.13_01\n");
    printf("exit:ctrl+c\n");
#if 1
	if(argc == 1)
	{
		strcpy(SerialName,"/dev/ttyPos0");
	}
	else if(argc == 2)
	{
		strcpy(SerialName,argv[1]);
	}
	else
	{
		printf("ARG ERROR\n");
		return 1;
	}

	printf("SerialName:%s\n",SerialName);
#endif
	ret = initSig();
	if(ret)
		return ret;	        

    usb_host_test(SerialName);

byebye:
	printf("doneflag:%d\n",doneflag);
    close(usbhost_fd);
	exit(0);
}

