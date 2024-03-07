#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <string.h>
#include <asm/ioctls.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#define clcd "/dev/clcd"
#define led "/dev/led"
#define dot "/dev/dot"
#define fnd_dev	"/dev/fnd"

static char tactswDev[] = "/dev/tactsw";
static int  tactswFd = (-1);

//길찾기 맵 배열열
unsigned char tilemap[1][8] ={
	{ 0x80,0xFE,0x7E,0x70,0x7f,0x1B,0x03,0x01 } };

unsigned char hex[1][8];
unsigned char binary[8][8] = {   
        {1,0,0,0,0,0,0,0}, 
        {1,1,1,1,1,1,1,0},
        {0,1,1,1,1,1,1,0},
        {0,1,1,1,0,0,0,0},
        {0,1,1,1,1,1,1,1},
        {0,0,0,1,1,0,1,1},
        {0,0,0,0,0,0,1,1},
        {0,0,0,0,0,0,0,1}
    };

// 2진수 -> 16진수
void binaryToHex(unsigned char binary[8][8], unsigned char hex[1][8]) {
    int i = 0;
    while (i < 8) {
        unsigned char value = 0;
        int j = 0;
        while (j < 8) {
            value |= (binary[i][j] << (7 - j));
            j++;
        }
        hex[0][i] = value;
        i++;
    }
}

void DOT_control(int rps_col, int time_sleep){
	int dot_d;

	dot_d = open(dot , O_RDWR);
	if (dot_d < 0) { printf("dot Error\n"); } // 예외처리

	write(dot_d , &hex[rps_col], hex(binary)); // 출력
	sleep(time_sleep); // 몇초동안 점등할지

	close(dot_d);
}

int main() {
    binaryToHex(binary, hex);
    while(1) {
    	DOT_control(0, 10); // 컴퓨터는 무엇을 내었는지 3초동안 보여줌
    }
    return 0;
}