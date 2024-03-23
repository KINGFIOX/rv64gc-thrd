/**
 * @file mtx.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2024-03-23
 *
 * @copyright Copyright (c) 2024
 *
 */

#define _GNU_SOURCE

#include <err.h>
#include <linux/futex.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

#if 1
#include "my_thrd.h"
#else
#endif

/**
 * @brief mtx=0 是可以获取；mtx=1 是已经上锁了
 *
 */

#define CAS_strong(p_obj, p_expected, desired) __atomic_compare_exchange_n(p_obj, p_expected, desired, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
#define ATOMIC_STORE_strong(p_obj, val) __atomic_store_n(p_obj, val, __ATOMIC_SEQ_CST)

inline static int futex(uint32_t* uaddr, int futex_op, uint32_t val, const struct timespec* timeout, uint32_t* uaddr2, uint32_t val3)
{
    return syscall(SYS_futex, uaddr, futex_op, val, timeout, uaddr2, val3);
}

bool _try_lock(uint32_t* futexp)
{
    uint32_t expected = 0;
    // 使用原子比较并交换操作尝试设置互斥锁的值为1
    return CAS_strong(futexp, &expected, 1);
}

/**
 * @brief 锁列表，假设最多只有 99 个锁，锁的编号，下标从 1 开始
 * mutexs[0] 表示有多少个锁（或者也可以表示：最后一个锁的位置）
 * 有一个假设：全局静态变量，总会初始化为0
 *
 */
static uint32_t* mutexs[100];

int mtx_create()
{
    int cnt = (int)(++mutexs[0]);
    uint32_t* futexp = malloc(sizeof(uint32_t));
    if (futexp == NULL) {
        return -1;
    }

    // *futex = 0; // 设置为有效
    ATOMIC_STORE_strong(futexp, 0);

    mutexs[cnt] = futexp;

    return cnt;
}

// 对指定编号的互斥锁在 共享区 上锁
void mtx_lock(int mtx)
{
    uint32_t* futexp = mutexs[mtx];
    // 如果直接尝试获取锁失败，则进入等待
    while (!_try_lock(futexp)) {
        // syscall(SYS_futex, futex_val, FUTEX_WAIT, 1, NULL, NULL, 0);
        futex(futexp, FUTEX_WAIT, 1, NULL, NULL, 0);
    }
}

// 对指定编号的互斥锁在 共享区 解锁
void mtx_unlock(int mtx)
{
    uint32_t* futexp = mutexs[mtx];
    ATOMIC_STORE_strong(futexp, 0);
    futex(futexp, FUTEX_WAKE, 1, NULL, NULL, 0);
}

void destroy_mtxs()
{
    int cnt = (int)mutexs[0];
    while (cnt > 0) {
        free(mutexs[cnt--]);
    }
}

#if 0

#include <pthread.h>
#include <stdio.h>

#include "mtx.h"

int sum = 0;

int mtx = 1; /* 初始化一个全局的锁 */

void* func(void*)
{
    for (int i = 0; i < 10000; i++) {
        mtx_lock(mtx);
        // printf("lock");
        sum++;
        mtx_unlock(mtx);
        // printf("unlock");
    }

    return NULL;
}

int main(void)
{

    mtx = mtx_create();

    printf("start threads\n");

    pthread_t arr[10];
    for (int i = 0; i < 10; i++) {
        pthread_create(&arr[i], NULL, func, NULL);
    }

    printf("start over\n");

    for (int i = 0; i < 10; i++) {
        pthread_join(arr[i], NULL);
    }

    printf("sum=%d\n", sum); // 期望是 1,000 * 10 = 10,000

    destroy_mtxs();
}

#endif
