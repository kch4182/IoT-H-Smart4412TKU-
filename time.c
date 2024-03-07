#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define TIME_INTERVAL 1
#define LIMIT_TIME 180

int remaining_time = LIMIT_TIME;
int input_value = -1;
int count = 0;

pthread_mutex_t mutex;

// 1초에 한 번씩 남은 시간을 출력하는 함수
void *print_remaining_time(void *arg) {
    while (remaining_time > 0) {
        pthread_mutex_lock(&mutex);
        printf("남은 시간: %d초\n", remaining_time);
        pthread_mutex_unlock(&mutex);
        remaining_time--;

        sleep(TIME_INTERVAL);
    }
    printf("남은 시간이 0초입니다. 프로그램을 종료합니다.\n");
    pthread_exit(NULL);
}

int main() {
    pthread_t timer_thread;
    int result;

    pthread_mutex_init(&mutex, NULL);

    // 타이머 스레드 생성
    result = pthread_create(&timer_thread, NULL, print_remaining_time, NULL);
    if (result != 0) {
        fprintf(stderr, "타이머 스레드 생성 실패\n");
        return 1;
    }

    // 사용자 입력 받기
    while (remaining_time > 0) {
        printf("값을 입력하세요: ");
        scanf("%d", &input_value);
        count++;
        printf("%d\n", count);
        if (input_value == 5) {
            pthread_mutex_lock(&mutex);
            remaining_time -= 3;
            pthread_mutex_unlock(&mutex);
        }
    }

    pthread_join(timer_thread, NULL);
    pthread_mutex_destroy(&mutex);

    return 0;
}