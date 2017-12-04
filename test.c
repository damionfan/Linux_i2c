/*
	寄存器 打地鼠 游戏
	基于at24c02
	在at24c02的寄存器0x00~0xff中，
	存在10个地鼠，你有10次机会去打地鼠
	如果在10次敲击中打中8次，即可证明打中地鼠，如果没有此游戏失败。

	其中，按键中断key1证明敲击下锤子
*/


#include <stdio.h>
#include <linux/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <linux/i2c.h>
#include <signal.h>


#define SLAVE_ADDR "/dev/at24"
int num_hamster = 10;
int ecific_numbe = 10;	//剩余次数
int num_bited = 0;	//打中的次数

int fd,oflags;
unsigned char address;
unsigned char ret[10] = {0x01,0x05,0x06,0x07,0x09,0x0a,0x0b,0x0e,0x03,0x0f};

static void signalio_handler(int signum)
{
	int i;
	printf("The Answer is :");
	for(i = 0; i < 10; i++)
	printf("0x%x  ",ret[i]);
}

int game_chose()
{	if (num_bited == 8) {
		ecific_numbe = 0;
		printf("Congratulations! YOU WIN!\n");
		return 0;
	}	
	printf("\t hamster num: %d,\t\t Hit the number of times: %d,\t\t ecific_numbe:%d\t\t\n",num_hamster,num_bited,ecific_numbe);
	printf("Please input what you chose register!\n\t0x");
	scanf("%x",&address);
	if (address < 0) {
		printf("Sorry,you input is error,please input again!\n");
	} else if (address > 0x10) {
		printf("Sorry,you input is error,please input again!\n");
	} else {
		ecific_numbe--;
		if (ecific_numbe == 0)
			printf("you only this chances!\n");
		read(fd, &address, 1);
		printf("%x\n",address);
		if (address == 0x40) {
			num_hamster--;
			num_bited++;
		printf("Congratulations on your hit on the ground mouse!\n");
		} else {
		printf("Sorry,you don't bit this mouse!\n");
		}
		}
}

static void show_the_first(void)
{
	int tmp;
	printf("*****************************\n");
	printf("When the hamster exists in the \nregister of 0x00~0x10 in AT24C02, you have 10 chances to choose\n the location of the hamster. If you can play 8 times in the 10 time, \nyou will win. Otherwise, you lose.\n");
	printf("If you understand, please enter 1! \n");
	scanf("%d",&tmp);
	if (tmp == 1) {
		system("clear");
		while(ecific_numbe) 
			game_chose();
		if (num_bited < 8)printf("So,sorry,you lose!\n");
	} else {
		system("clear");
		printf("Please read the game instructions carefully!\n ");
	}
}

int main(void)
{
	int first = 0;
	int last = 0x10;
	int i;
	unsigned char buff[2];
	
	fd = open(SLAVE_ADDR, O_RDWR);
	if (!fd) {
		printf("Error open the device!\n Please try again!\n");
		return 0;
	}
	for (i = first; i <= last; i++) {
		buff[0] = i;
		buff[1] = 0x0;
		write(fd, buff, sizeof(buff));
	}

	for (i = 0; i < 10; i++) {
		buff[0] = ret[i];
		buff[1] = 0x40;
		write(fd, buff, sizeof(buff));
	}

	signal(SIGIO, signalio_handler);
			fcntl(fd, F_SETOWN, getpid());
			oflags = fcntl(fd, F_GETFL);
			fcntl(fd, F_SETFL, oflags | FASYNC);
	
	show_the_first();
	return 0;
		
}
