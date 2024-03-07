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
#include <pthread.h>

#define clcd "/dev/clcd"
#define led "/dev/led"
#define dot "/dev/dot"
#define fnd	"/dev/fnd"
#define MAX_STAGE 3
#define LIMIT_TIME 180

static char tactswDev[] = "/dev/tactsw";
static int tactswFd = (-1);
int fnds;

bool isEnd = false; // 게임이 끝났는지 알려주는 전역변수

// 랜덤맵 생성 변수
unsigned char tile[8][8];   // 랜덤맵 저장할 배열
unsigned char dirArr[4];    // 방향 배열 [U, L, R, D]
int x=0, y=0;     // 현재 위치 표시 좌표
// unsigned char* indexArr;    // 방향 배열의 인덱스를 저장할 변수
int loop = 1;               // 반복문 제어

// 스테이지 관련 변수
int s = 1;
int stage = 0;          // 스테이지
int tileCount[3];       // 스테이지별 타일 개수
int maxTile[3] = {10, 25, 50};      // 스테이지별 최대 타일 개수 제한. ex) 1 stage는 최대 10개

// 게임 진행 시 현재 위치
unsigned char nowX = 0;
unsigned char nowY = 0;

// 타이머 관련 변수
unsigned char timeTable[] = {~0x3f,~0x06,~0x5b,~0x4f,~0x66,~0x6d,~0x7d,~0x07,~0x7f,~0x67,~0x00};    // fnd 0 ~ 9, empty
int remaining_time = LIMIT_TIME;
pthread_mutex_t mutex;
int t1, t10, t100;
unsigned char fndNum[4];
bool isClear = false;

/*
unsigned char tilemap[8][8] = { // 타일맵 예시
        {1,0,0,0,0,0,0,0}, 
        {1,1,1,1,1,1,1,0},
        {0,1,1,1,1,1,1,0},
        {0,1,1,1,0,0,0,0},
        {0,1,1,1,1,1,1,1},
        {0,0,0,1,1,0,1,1},
        {0,0,0,0,0,0,1,1},
        {0,0,0,0,0,0,0,1}
};
*/

unsigned char tile1[8][8];      // 원본 맵을 기억할 배열
unsigned char hex[1][8];        // Dot Matrix에 출력할 16진수 배열
unsigned char position[8][8];   // 현재 위치만 찍는 배열
unsigned char posHex[1][8];     // position 배열을 Dot Matrix에 출력할 16진수 배열

// 아마도 택트 스위치 클릭값 얻기 위한것
unsigned char tactsw_get(int tmo) 
{   
    unsigned char b;
	if (tmo) { 
        	if (tmo < 0) tmo = ~tmo * 1000;
        	else tmo *= 1000000;
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
			case 2: selected_tact = 2; break; // 위
			case 4: selected_tact = 4; break; // 왼쪽
			case 6: selected_tact = 6; break; // 오른쪽
			case 8: selected_tact = 8; break; // 아래
            case 10: selected_tact = 10; break; // 다른맵으로 변경
			case 12: selected_tact = 12; break; // 재시작
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

// [[테스트 필요]] *********************************************************************
// FND 제어 함수
void FND_control(int index, int time_sleep){
    int fnd_fd = 0;

    fnd_fd = open(fnd, O_RDWR);
	if(fnd_fd <0){ printf("fnd error\n"); } // 예외처리

    write(fnd_fd, &fndNum, sizeof(fndNum)); // 출력
    sleep(time_sleep); // 점등시간 조절

    close(fnd_fd);
}

// 제한 시간 스레드
void *print_remaining_time(void *arg) {
    while (remaining_time > 0) {
        if(isClear) break;
        // mutex -- 상호배제
        pthread_mutex_lock(&mutex); 
        t1 = remaining_time % 10;                   // 1의 자리
        t10 = (remaining_time % 100 - t1) / 10;     // 10의 자리
        t100 = remaining_time / 100;                // 100의 자리
        fndNum[0] = timeTable[0];       // fnd 1000의 자리 출력
        fndNum[1] = timeTable[t100];    // fnd 100의 자리 출력
        fndNum[2] = timeTable[t10];     // fnd 10의 자리 출력
        fndNum[3] = timeTable[t1];      // fnd 1의 자리 출력
        FND_control(0, 1);      // 1초 동안 출력
        pthread_mutex_unlock(&mutex);
        remaining_time--;
        usleep(100000);
    }
    pthread_exit(NULL);
}
// [[테스트 필요]] *********************************************************************

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

// 타일맵 0으로 초기화. 출발 지점만 1
void tileInitialization()
{
    int i = 0, j = 0;
    while (i < 8) {
        while (j < 8) {
            tile[i][j] = 0;
            j++;
        }
        j = 0;
        i++;
    }
    tile[0][0] = 1;   // 시작 위치만 1로 초기화
}

// 2D matrix 출력 (테스트용)
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

// dirArr[4] 갱신 함수, 갈 수 있는 방향 찾기
void direction()
{
    // 위가 벽인지 확인
    if (x - 1 < 0) dirArr[0] = 0;   // 벽이므로 위로 이동 불가능
    else {  // 벽이 아닌 경우
        if (tile[x - 1][y] == 0) dirArr[0] = 1;    // 위가 0일 경우 이동 가능
        else dirArr[0] = 0;    // 0이 아닐 경우 이동 불가능
    }

    // 왼쪽이 벽인지 확인
    if (y - 1 < 0) dirArr[1] = 0;   // 벽이므로 왼쪽으로 이동 불가능
    else {
        if (tile[x][y - 1] == 0) dirArr[1] = 2;    // 왼쪽이 0일 경우 이동 가능
        else dirArr[1] = 0;    // 0이 아닐 경우 이동 불가능
    }

    // 오른쪽이 벽인지 확인
    if (y + 1 > 7) dirArr[2] = 0;    // 벽이므로 오른쪽으로 이동 불가능
    else {
        if (tile[x][y + 1] == 0) dirArr[2] = 3;    // 오른쪽이 0일 경우 이동 가능
        else dirArr[2] = 0;    // 0이 아닐 경우 이동 불가능
    }
    
    // 아래가 벽인지 확인
    if (x + 1 > 7) dirArr[3] = 0;    // 벽이므로 아래로 이동 불가능
    else {
        if (tile[x + 1][y] == 0) dirArr[3] = 4;    // 아래쪽이 0일 경우 이동 가능
        else dirArr[3] = 0;    // 0이 아닐 경우 이동 불가능
    }
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
        if (array[i] > 0) count++;
        i++;
    }
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

// 랜덤으로 길 생성
void makeLoad()
{   
    int i=0, dirCount=0;
    int selectNum;      // 방향 1:up, 2:left, 3:right, 4:down
    direction();        // 1. 갈 수 있는 방향 찾아서 dirArr 갱신
    
    while(i<4){         // 1-1. dirArr 확인
        if(dirArr[i] == 0) dirCount++;  // 갈 수 있는 방향이 있는지 없는지 확인
        i++;
    }
    if(dirCount == 4){  // 갈 수 있는 방향이 없으면 함수 종료
        loop = 0;
        return;
    }
    // indexArr = findIndex(dirArr);       // 2. dirArr 인덱스 찾기
    selectNum = selectRandom(dirArr); // 3. 인덱스 중 랜덤으로 하나 선택(이동할 방향 랜덤으로 선택)

    if (selectNum == 1) {       // 선택한 방향이 위쪽인 경우
        x -= 1;                 // 위로 이동
        tile[x][y] = 1;         // 길 만듦
    }
    else if (selectNum == 2) {  // 선택한 방향이 왼쪽인 경우
        y -= 1;                 // 왼쪽으로 이동
        tile[x][y] = 1;         // 길 만듦
    }
    else if (selectNum == 3) {  // 선택한 방향이 오른쪽인 경우
        y += 1;                 // 오른쪽으로 이동
        tile[x][y] = 1;         // 길 만듦
    }
    else if (selectNum == 4) {  // 선택한 방향이 아래쪽인 경우
        x += 1;                 // 아래쪽으로 이동
        tile[x][y] = 1;         // 길 만듦
    }
}

// 맵에서 남은 타일 개수 계산
int totalTile() {
    int i=0, j=0, count=0;
    while(i<8){
        while(j<8){
            if(tile[i][j] == 1) count++;
            j++;
        }
        j=0;
        i++;
    }
    return count;
}

// 최종 맵 생성
void makeMap(){
    // 맵 생성에 필요한 변수 초기화
    s = 1;
    loop = 1;
    x = 0;
    y = 0;
    tileInitialization();     // 타일맵 0으로 초기화
    while(s < maxTile[stage]){      // 맵 생성   
        makeLoad();
        s++;
        if (loop == 0) break;
    }
    tileCount[stage] = totalTile(); // 현재 스테이지의 타일 개수 저장
}

// 스테이지에 따라 타일 개수 조정
void makeMap2(){
    makeMap();
    if(stage > 0)      // 2스테이지부터 확인
        // 이전 스테이지보다 타일 개수가 적거나 같은 경우 맵 재생성
        while(tileCount[stage] <= tileCount[stage-1]) makeMap();
    copyArray(tile, tile1);     // 재시작을 위한 타일 복사
}

// 위치 이동 함수
void movePlayer(int dir) {
    if (dir == 2) {     // 위로 이동
        if (nowX - 1 < 0) clcd_input("wall");
        else {
            if (tile[nowX-1][nowY] == 0) clcd_input("fail");    // 이동할 위치의 값이 0인 경우
            else {
                tile[nowX][nowY] = 0;
                nowX -= 1;         
           }
        }
    }
    else if (dir == 4) {     // 왼쪽으로 이동
        if (nowY - 1 < 0) clcd_input("wall");
        else {
            if (tile[nowX][nowY-1] == 0) clcd_input("fail");   // 이동할 위치의 값이 0인 경우
            else {
                tile[nowX][nowY] = 0;
                nowY -= 1;
            }
        }
    }
    else if (dir == 6) {     // 오른쪽으로 이동
        if (nowY + 1 > 7) clcd_input("wall");
        else {
            if (tile[nowX][nowY+1] == 0) clcd_input("fail");   // 이동할 위치의 값이 0인 경우
            else {
                tile[nowX][nowY] = 0;
                nowY += 1;
            }
        }
    }
    else if (dir == 8) {     // 아래로 이동
        if (nowX + 1 > 7) clcd_input("wall");
        else {
            if (tile[nowX+1][nowY] == 0) clcd_input("fail");   // 이동할 위치의 값이 0인 경우
            else {
                tile[nowX][nowY] = 0;
                nowX += 1;
            }
        }
    }
}

void DOT_print(int time){       
    binaryToHex(tile);
    DOT_control(0,time);
}

int main(){
    // 게임 실행 **************************************************
    srand((unsigned int)time(NULL));    // seed값으로 현재시간 부여
    pthread_t timer_thread;     // 타이머 스레드
    int result;                 // 스레드 생성 결과
    pthread_mutex_init(&mutex, NULL);   // 뮤텍스 초기화(생성)

    /* 인트로 ********************** 할지말지?*/
    result = pthread_create(&timer_thread, NULL, print_remaining_time, NULL);
    if (result != 0) {
        fprintf(stderr, "thread fail\n");
        return 1;
    }
    
    int changeMapCount = 3;
    // 게임 시작
    while(!isEnd){
        clcd_input("2:U 4:L 6:R 8:D 10:Ch 12:Re");  // 조작키 출력
        while(stage < MAX_STAGE){
            // [[테스트 필요]] *********************************************************************
            if (remaining_time <= 0){   // 제한 시간이 지날 경우
                clcd_input("Game Over!");
                break;
            }
            // [[테스트 필요]] *********************************************************************
            makeMap2();
            // First_DOT_print();          // 맵을 5초동안 출력
            DOT_print(5000000);

            // 타일 지우기
            while(true){
                //DOT_print();                // 맵 출력
                DOT_print(300000);
                position[nowX][nowY] = 1;   // 현재 위치를 1로 바꿈
                binaryToPosHex(position);   // 현재 위치 배열을 16진수로 변환
                DOT_control_pos(0,150000);  // 현재 위치 Dot Matrix에 출력
                position[nowX][nowY] = 0;   // 출력 후 초기화

                // 현재 위치에서 갈 수 있는 방향이 없는 경우
                if (tile[nowX-1][nowY] == 0 && tile[nowX][nowY-1] == 0 && tile[nowX][nowY+1] == 0 && tile[nowX+1][nowY] == 0)
                    clcd_input("Restart(12) the game");
                 
                // tact switch 입력
                if(tact_switch_listener() == 2) movePlayer(2);      // 2번 입력(위로 이동)
                else if(tact_switch_listener() == 4) movePlayer(4); // 4번 입력(왼쪽 이동)
                else if(tact_switch_listener() == 6) movePlayer(6); // 6번 입력(오른쪽 이동)
                else if(tact_switch_listener() == 8) movePlayer(8); // 8번 입력(아래 이동)
                else if(tact_switch_listener() == 10){ // 10번 입력(다른맵으로 변경)    [[테스트 필요]]
                    if (changeMapCount > 0){            // 변경 가능 횟수가 존재할 경우
                        makeMap2();     // 맵 생성
                        // 현재 위치 초기화
                        nowX = 0;
                        nowY = 0;
                        clcd_input("Map is changed");
                        changeMapCount--;
                    } else clcd_input("You can't change");
                } else if(tact_switch_listener() == 12){ // 12번 입력(재시작)
                    copyArray(tile1, tile);     // 원래 맵으로 다시 복사
                    nowX = 0;
                    nowY = 0;
                    clcd_input("Reset");
                } else clcd_input("Use other key");
    
                // 스테이지 클리어
                int leftTile = totalTile();
                if(leftTile == 1){
                    stage++;    // 다음 스테이지로 넘어감
                    // 사용 변수 초기화
                    x = 0;
                    y = 0;
                    nowX = 0;
                    nowY = 0;
                    loop = 1;
                    clcd_input("Stage Clear!");
                    break;
                }
            }
        }
        pthread_join(timer_thread, NULL);   // timer_therad가 종료할 때까지 대기
        pthread_mutex_destroy(&mutex);      // 뮤텍스 제거

        if (remaining_time <= 0) clcd_input("Time out");    // 타임 아웃
        else {      // 모든 스테이지 클리어
            isClear = true;
            clcd_input("Clear!           Congratulations");
        }
        sleep(2); // 2초동안 clcd 출력
        clcd_input("Play Again?      4:Y 6:N");
        if(tact_switch_listener() == 4){
            stage = 0;
            isClear = false;
            remaining_time = LIMIT_TIME;
            result = pthread_create(&timer_thread, NULL, print_remaining_time, NULL);
            if (result != 0) {
                fprintf(stderr, "thread fail\n");
                return 1;
            }
        } else if(tact_switch_listener() == 6){   // 게임 종료
            //pthread_join(timer_thread, NULL);   // timer_therad가 종료할 때까지 대기
            //pthread_mutex_destroy(&mutex);      // 뮤텍스 제거
            clcd_input("See you again");
            sleep(1);
            isEnd = true;
        }
    }
    return 0;
}
