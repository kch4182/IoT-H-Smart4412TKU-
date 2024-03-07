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

bool isEnd = false; // 게임이 끝났는지 알려주는 전역변수

// 랜덤맵 생성 변수
unsigned char tile[8][8];   // 랜덤맵 저장할 배열
unsigned char dirArr[4];    // 방향 배열 [U, L, R, D]
int x=0, y=0;     // 현재 위치 표시 좌표
unsigned char* indexArr;    // 방향 배열의 인덱스를 저장할 변수
int loop = 1;               // 반복문 제어

// 게임 진행 시 현재 위치
unsigned char nowX = 0;
unsigned char nowY = 0;

unsigned char tilemap[8][8] = { // 맵
        {1,0,0,0,0,0,0,0}, 
        {1,1,1,1,1,1,1,0},
        {0,1,1,1,1,1,1,0},
        {0,1,1,1,0,0,0,0},
        {0,1,1,1,1,1,1,1},
        {0,0,0,1,1,0,1,1},
        {0,0,0,0,0,0,1,1},
        {0,0,0,0,0,0,0,1}
};
unsigned char tile1[8][8];   // 원본 맵을 기억할 배열
unsigned char hex[1][8];        // Dot Matrix에 출력할 16진수 배열
unsigned char position[8][8];   // 현재 위치 찍는 배열
unsigned char posHex[1][8];     // position 배열을 출력할 16진수 배열

// tilemap - 2진수를 16진수로 변환
void binaryToHex(unsigned char binary[8][8]) {
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

// position 배열 - 2진수를 16진수로 변환
void binaryToPosHex(unsigned char binary[8][8]) {
    int i = 0;
    while (i < 8) {
        unsigned char value = 0;
        int j = 0;
        while (j < 8) {
            value |= (binary[i][j] << (7 - j));
            j++;
        }
        posHex[0][i] = value;
        i++;
    }
}

// 2D matix 0으로 초기화
void matrixInitialization(unsigned char matrix[8][8])
{
    int i = 0, j = 0;
    while (i < 8) {
        while (j < 8) {
            matrix[i][j] = 0;
            j++;
        }
        j = 0;
        i++;
    }
}

// 2D matrix 출력
void matrixPrint(unsigned char matrix[8][8])
{
    int i = 0, j = 0;
    while (i < 8) {
        while (j < 8) {
            printf("%2d ", matrix[i][j]);
            j++;
        }
        printf("\n");
        j = 0;
        i++;
    }
    printf("\n");
}

// dirArr 배열 인덱스 찾기, indexArr 갱신
unsigned char* findIndex(unsigned char array[4])
{
    int i = 0, j = 0;
    unsigned char* index = (unsigned char*)malloc(sizeof(unsigned char) * 1); // 동적할당
    while (i < 4) {
        if (array[i] == 1) {
            index[j] = i + 1;
            j++;
        }
        i++;
    }
    return index;
}

// 배열에서 0보다 큰 숫자 중 랜덤으로 선택
int selectRandom(unsigned char array[4])
{
    int size = 4;
    int count = 0;
    int i = 0, index;
    int selectNumber;

    // 0보다 큰 숫자 개수 세기
    while (i < size) {
        if (array[i] > 0) {
            count++;
        }
        i++;
    }
    i = 0;

    // 임의로 숫자 선택
    index = rand() % count;

    i = 0;
    while (i < size) {
        if (array[i] > 0) {
            if (index == 0) {
                selectNumber = array[i];
                break;
            }
            index--;
        }
        i++;
    }
    return selectNumber;
}

// dirArr[4] 갱신 함수, 갈 수 있는 방향 찾기
void direction()
{
    if (x - 1 < 0) {    // 위쪽 벽
        dirArr[0] = 0;     // 벽이기 때문에 이동 불가능
    }
    else {
        if (tile[x - 1][y] == 0) {    // 위가 0일 경우 이동 가능
            dirArr[0] = 1;
        }
        else {    // 0이 아닐 경우 이동 불가능
            dirArr[0] = 0;
        }
    }
    if (y - 1 < 0) {    // 왼쪽 벽
        dirArr[1] = 0;     // 벽이기 때문에 이동 불가능
    }
    else {
        if (tile[x][y - 1] == 0) {    // 왼쪽이 0일 경우 이동 가능
            dirArr[1] = 1;
        }
        else {    // 0이 아닐 경우 이동 불가능
            dirArr[1] = 0;
        }
    }
    if (y + 1 > 7) {    // 오른쪽 벽
        dirArr[2] = 0;     // 벽이기 때문에 이동 불가능
    }
    else {
        if (tile[x][y + 1] == 0) {    // 오른쪽이 0일 경우 이동 가능
            dirArr[2] = 1;
        }
        else {    // 0이 아닐 경우 이동 불가능
            dirArr[2] = 0;
        }
    }
    if (x + 1 > 7) {    // 아래쪽 벽
        dirArr[3] = 0;     // 벽이기 때문에 이동 불가능
    }
    else {
        if (tile[x + 1][y] == 0) {    // 아래쪽이 0일 경우 이동 가능
            dirArr[3] = 1;
        }
        else {    // 0이 아닐 경우 이동 불가능
            dirArr[3] = 0;
        }
    }
}

void makeMap()
{   
    int i=0, dirCount=0;
    int selectNum;  // 방향 1:up, 2:left, 3:right, 4:down
    direction();                        // 1. 갈 수 있는 방향 찾아서 dirArr 갱신
    
    while(i<4){                         // 1-1. dirArr 확인
        if(dirArr[i] == 0){             // 갈 수 있는 방향이 있는지 없는지 확인
            dirCount++;
        }
        i++;
    }
    if(dirCount == 4){                  // 갈수 있는 방향이 없으면 함수 종료
        loop = 0;
        return;
    }
    indexArr = findIndex(dirArr);       // 2. dirArr 인덱스 찾기
    selectNum = selectRandom(indexArr); // 3. 인덱스 중 랜덤으로 하나 선택

    if (selectNum == 1) {        // 방향이 위쪽인 경우
        x -= 1;                 // 위로 이동
        tile[x][y] = 1;         // 길 만듦
    }
    else if (selectNum == 2) {   // 방향이 왼쪽인 경우
        y -= 1;                 // 왼쪽으로 이동
        tile[x][y] = 1;         // 길 만듦
    }
    else if (selectNum == 3) {   // 방향이 오른쪽인 경우
        y += 1;                 // 오른쪽으로 이동
        tile[x][y] = 1;         // 길 만듦
    }
    else if (selectNum == 4) {   // 방향이 아래쪽인 경우
        x += 1;                 // 아래쪽으로 이동
        tile[x][y] = 1;         // 길 만듦
    }

    // matrixPrint(tile);     // 타일 출력
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

// CLCD 제어 함수
void clcd_input(char clcd_text[]){
	int clcd_d;

	clcd_d = open(clcd , O_RDWR);
	if (clcd_d < 0) { printf("clcd error\n"); }// 예외처리

	write(clcd_d , clcd_text , strlen(clcd_text)); // 두번째부터 각각 문자열, 문자열 크기
	close(clcd_d); 
}

// Dot Matrix 제어 함수 (tilemap)
void DOT_control(int col, int time_sleep){
	int dot_d;

	dot_d = open(dot , O_RDWR);
	if (dot_d < 0) { printf("dot Error\n"); } // 예외처리

	write(dot_d , &hex[col], sizeof(hex)); // 출력
	usleep(time_sleep); // 몇초동안 점등할지
	close(dot_d);
}

// Dot Matrix 제어 함수 (position)
void DOT_control_pos(int col, int time_sleep){
	int dot_d;

	dot_d = open(dot , O_RDWR);
	if (dot_d < 0) { printf("dot Error\n"); } // 예외처리

	write(dot_d , &posHex[col], sizeof(posHex)); // 출력
	usleep(time_sleep); // 몇초동안 점등할지
	close(dot_d);
}

// 위치 이동 함수
void movePlayer(int dir) {
    if (dir == 2) {     // 위로 이동
        if (nowX - 1 < 0){
            // 아무것도 실행 안함
            clcd_input("wall");
        }     
        else {
            if (tile[nowX-1][nowY] == 0){    // 이동할 위치의 값이 0인 경우
                clcd_input("fail");
            } else {
                tile[nowX][nowY] = 0;
                nowX = nowX - 1;         
            }
        }
    }
    else if (dir == 4) {     // 왼쪽으로 이동
        if (nowY - 1 < 0) {
            clcd_input("wall");     // 아무것도 실행 안함
        }
        else {
            if (tile[nowX][nowY-1] == 0){    // 이동할 위치의 값이 0인 경우
                clcd_input("fail");
            } else {
                tile[nowX][nowY] = 0;
                nowY = nowY - 1;
            }
        }
    }
    else if (dir == 6) {     // 오른쪽으로 이동
        if (nowY + 1 > 7) {
            clcd_input("wall");   // 아무것도 실행 안함
        }
        else {
            if (tile[nowX][nowY+1] == 0){    // 이동할 위치의 값이 0인 경우
                clcd_input("fail");
            } else {
                tile[nowX][nowY] = 0;
                nowY = nowY + 1;
            }
        }
    }
    else if (dir == 8) {     // 아래로 이동
        if (nowX + 1 > 7) {     // 벽
            clcd_input("wall");   // 아무것도 실행 안함
        } 
        else {
            if (tile[nowX+1][nowY] == 0){    // 이동할 위치의 값이 0인 경우
                clcd_input("fail");
            } else {
                tile[nowX][nowY] = 0;
                nowX = nowX + 1;
            }
        }
    }
    /*
    else if (dir == 12) {     // 재시작
        nowX = 0;
        nowY = 0;
    }*/
}

// 타일맵 16진수로 변환 후 출력
void DOT_print(){       
    binaryToHex(tile);
    DOT_control(0,300000);
}

// 게임 시작 시 맵 5초동안 보여주기
void First_DOT_print(){       
    binaryToHex(tile);
    DOT_control(0,5000000);
}

// 배열 복사
void copyArray(unsigned char src[8][8], unsigned char dest[8][8]) {
    int i = 0;
    while (i < 8) {
        int j = 0;
        while (j < 8) {
            dest[i][j] = src[i][j];
            j++;
        }
        i++;
    }
}

int main(){
    srand((unsigned int)time(NULL)); // seed값으로 현재시간 부여

    // 랜덤맵 생성
    matrixInitialization(tile);      // 타일맵 0으로 초기화
    tile[0][0] = 1;
    //matrixPrint(tile);
    
    int i=0, j=0;
    while(1){
        makeMap();
        if (loop==0) break;
        i++;
    }

    //matrixPrint(tile);

    copyArray(tile, tile1);
    while(!isEnd) {
        First_DOT_print();
        clcd_input("2:U 4:L 6:R     8:D 12:Re");
        while(true){
            DOT_print();
            position[nowX][nowY] = 1;  // 현위치를 1로 바꿈
            binaryToPosHex(position);  // 현위치 2진법 -> 16진법
            DOT_control_pos(0,150000); // 현위치 Dot Matrix에 출력
            position[nowX][nowY] = 0;  // 출력 후 초기화
            if(tact_switch_listener() == 2){    // 2번 입력(위)
                movePlayer(2);
                //printf("(%d, %d)\n", nowX, nowY);
            } else if(tact_switch_listener() == 4){ // 4번 입력(왼쪽)
                movePlayer(4);
                //printf("(%d, %d)\n", nowX, nowY);
            } else if(tact_switch_listener() == 6){ // 6번 입력(오른쪽)
                movePlayer(6);
                //printf("(%d, %d)\n", nowX, nowY);
            } else if(tact_switch_listener() == 8){ // 8번 입력(아래)
                movePlayer(8);;
                //printf("(%d, %d)\n", nowX, nowY);
            } else if(tact_switch_listener() == 12){ // 12번 입력(재시작)
                copyArray(tile1, tile);
                nowX=0;
                nowY=0;
                //printf("(%d, %d)\n", nowX, nowY);
                clcd_input("Reset");
                
            } else { 
                clcd_input("Use key:2, 4, 6, 8, 12");
            }

            // 타임아웃
            // if (...) { break; }
            
            // 도착
	    /*
            if((nowX==7) && (nowY==7)){
                
                break;
            }*/
        }
        isEnd = true;
    }
    return 0;
}
