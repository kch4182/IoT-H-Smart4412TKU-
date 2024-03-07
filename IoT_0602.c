//#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
//#include <unistd.h>
#include <fcntl.h>
//#include <sys/signal.h>
#include <sys/types.h>
#include <string.h>
//#include <asm/ioctls.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
//#include <unistd.h>
//#include <sys/ioctl.h>
#include <sys/stat.h>

#define clcd "/dev/clcd"
#define led "/dev/led"
#define dot "/dev/dot"
#define fnd_dev	"/dev/fnd"

static char tactswDev[] = "/dev/tactsw";
static int  tactswFd = (-1);

unsigned char nowX = 0;
unsigned char nowY = 0;
int led_count = 0; // 베팅액을 확인하기 위한 전역변수
// int user_money[4] = { 3, 0, 0, 0 }; // 초기 지급 금액, 3000원
bool isEnd = false; // 경기가 끝났는지 알려주는 전역변수
unsigned char tilemap[8][8] = {   
        {1,0,0,0,0,0,0,0}, 
        {1,1,1,1,1,1,1,0},
        {0,1,1,1,1,1,1,0},
        {0,1,1,1,0,0,0,0},
        {0,1,1,1,1,1,1,1},
        {0,0,0,1,1,0,1,1},
        {0,0,0,0,0,0,1,1},
        {0,0,0,0,0,0,0,1}
};

unsigned char hex[1][8];

// 2진수를 16진수로 변환
void binaryToHex(unsigned char binary[8][8],unsigned char hex[1][8]) {
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

// 아마도 택트 스위치 클릭값 얻기 위한것
unsigned char tactsw_get(int tmo) 
{   
    unsigned char b;
	if (tmo) { 
        	if (tmo < 0)
            		tmo = ~tmo * 1000;
        	else
            		tmo *= 1000000;

        	while (tmo > 0) {
            		usleep(10000);
            		read(tactswFd, &b, sizeof(b));
	       		if (b) return(b);
            			tmo -= 10000;
        	}
	        return(-1); 
    	}
    	else {
      		read(tactswFd, &b, sizeof(b));
        	return(b);
    	}
}

// 호출하면 택트 스위치값 반환해주는 함수
int tact_switch_listener(){
	unsigned char c;
	int selected_tact = 0; // false 값 넣기

	if((tactswFd = open( tactswDev, O_RDONLY )) < 0){     	// 예외처리    
		perror("tact error");
		exit(-1);
	}

	while(1){
		c = tactsw_get(10);
		switch (c) {
				case 2:  selected_tact = 2 ; break; // 위
				case 4:  selected_tact = 4 ; break; // 왼쪽
				case 6:  selected_tact = 6 ; break; // 오른쪽
				case 8:  selected_tact = 8 ; break; // 아래
				case 12:  selected_tact = 12 ; break; // 재시작
				//default: printf("press other key\n", c); break; // LCD 제어
		}
		return selected_tact; // 어떤 스위치가 눌렸는지 int 형으로 반환함
		}
}

void clcd_input(char clcd_text[]){
	int clcd_d;

	clcd_d = open(clcd , O_RDWR);
	if (clcd_d < 0) { printf("clcd error\n"); }// 예외처리

	write(clcd_d , clcd_text , strlen(clcd_text)); // 두번째부터 각각 문자열, 문자열 크기
	close(clcd_d); 
}

void DOT_control(int col, int time_sleep){
	int dot_d;

	dot_d = open(dot , O_RDWR);
	if (dot_d < 0) { printf("dot Error\n"); } // 예외처리

	write(dot_d , &hex[col], sizeof(hex)); // 출력
	sleep(time_sleep); // 몇초동안 점등할지

	close(dot_d);
}
/*
void movePlayer(unsigned char posX, unsigned char posY, int dir) {
    //int map1[8][8];
    if (dir == 2) {     // 위로 이동
        if (posX - 1 < 0){
            // 아무것도 실행 안함
            clcd_input("wall");
        }     
        else {
            if (tilemap[posX-1][posY] == 0){    // 이동할 위치의 값이 0인 경우
                clcd_input("fail");
            } else {
                tilemap[posX][posY] = 0;
                posX = posX - 1;
            }
        }
    }
    else if (dir == 4) {     // 왼쪽으로 이동
        if (posY - 1 < 0) {
            clcd_input("wall");     // 아무것도 실행 안함
        }
        else {
            if (tilemap[posX][posY-1] == 0){    // 이동할 위치의 값이 0인 경우
                clcd_input("fail");
            } else {
                tilemap[posX][posY-1] = 0;
                posY = posY - 1;
            }
        }
    }
    else if (dir == 6) {     // 오른쪽으로 이동
        if (posY + 1 > 7) {
            clcd_input("wall");   // 아무것도 실행 안함
        }
        else {
            if (tilemap[posX][posY+1] == 0){    // 이동할 위치의 값이 0인 경우
                clcd_input("fail");
            } else {
                tilemap[posX][posY] = 0;
                posY = posY + 1;
            }
        }
    }
    else if (dir == 8) {     // 아래로 이동
        if (posX + 1 > 7) {     // 벽
            clcd_input("wall");   // 아무것도 실행 안함
        } 
        else {
            if (tilemap[posX+1][posY] == 0){    // 이동할 위치의 값이 0인 경우
                clcd_input("fail");
            } else {
                tilemap[posX][posY] = 0;
                posX = posX + 1;
            }
        }
    }
    else if (dir == 12) {     // 재시작
        posX = 0;
        posY = 0;
    }
    //return posX, posY;
}*/

void DOT_print(){       // 타일맵 16진수로 변환 후 출력
    binaryToHex(tilemap, hex);
    DOT_control(0,2);
}

int main(){
    //static unsigned char nowX = 0;
    //static unsigned char nowY = 0;
    //binaryToHex(tilemap);
    while(!isEnd) {
        //DOT_control(0,10);
        clcd_input("2:Up, 4:Left, 6:Right, 8:Under, 12:Restart");
        while(true){
            DOT_print();
            if(tact_switch_listener() == 2){    // 2번 입력(위)
                // movePlayer(nowX, nowY, 2);
                // nowX -= 1;

			



                DOT_print();
                printf("(%d, %d)\n", nowX, nowY);
            } else if(tact_switch_listener() == 4){ // 4번 입력(왼쪽)
                // movePlayer(nowX, nowY, 4);
                //nowY -= 1;
                DOT_print();
                printf("(%d, %d)\n", nowX, nowY);
            } else if(tact_switch_listener() == 6){ // 6번 입력(오른쪽)
                // movePlayer(nowX, nowY, 6);
                //nowY += 1;
                DOT_print();
                printf("(%d, %d)\n", nowX, nowY);
            } else if(tact_switch_listener() == 8){ // 8번 입력(아래)
                // movePlayer(nowX, nowY, 8);
                //nowX += 1;
// 8888888888888888888888888888888
    else if (dir == 8) {     // 아래로 이동
        if (nowX + 1 > 7) {     // 벽
            clcd_input("wall");   // 아무것도 실행 안함
        } 
        else {
            if (tilemap[nowX+1][nowY] == 0){    // 이동할 위치의 값이 0인 경우
                clcd_input("fail");
            } else {
                tilemap[nowX][nowY] = 0;
                nowX = nowX + 1;
            }
        }
    }
// 888888888888888888888888888888
                DOT_print();
                printf("(%d, %d)\n", nowX, nowY);
            } else if(tact_switch_listener() == 12){ // 12번 입력(재시작)
                // movePlayer(nowX, nowY, 12);
                DOT_print();
                printf("(%d, %d)\n", nowX, nowY);
            } else {
                clcd_input("Use right key:2, 4, 6, 8, 12");
            }

            
            // 타임아웃
            // if (...) { break; }
            
            // 도착
            if((nowX==7) && (nowY==7)){
                break;
            }
        }
    }
    return 0;
}
