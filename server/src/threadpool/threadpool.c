#include "threadpool.h"
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>

struct tpool_work {
    thread_func_t     func;
    void             *arg;
    struct tpool_work *next;
};

static tpool_work_t *tpool_work_create(thread_func_t func, void *arg);
static void          tpool_work_destroy(tpool_work_t *work);
static tpool_work_t *tpool_work_get(tpool_t *tm);
static void         *tpool_worker(void *arg);


tpool_t *tpool_create(size_t num) {
    tpool_t  *tm;
    pthread_t thread;
    size_t    i;

    if (num == 0) {
        num = 2;
    }

    tm = calloc(1, sizeof(*tm));
    if (!tm) {
        perror("Pool allocation failed");
        return NULL;
    }
    tm->thread_cnt = num;

    if (pthread_mutex_init(&(tm->work_mutex), NULL) != 0) {
        perror("Mutex init failed");
        free(tm);
        return NULL;
    }
    if (pthread_cond_init(&(tm->work_cond), NULL) != 0) {
        perror("Work cond init failed");
        pthread_mutex_destroy(&(tm->work_mutex));
        free(tm);
        return NULL;
    }
    if (pthread_cond_init(&(tm->working_cond), NULL) != 0) {
        perror("Working cond init failed");
        pthread_cond_destroy(&(tm->work_cond));
        pthread_mutex_destroy(&(tm->work_mutex));
        free(tm);
        return NULL;
    }

    for (i = 0; i < num; i++) {
        if (pthread_create(&thread, NULL, tpool_worker, tm) != 0) {
            perror("Thread create failed");
            tm->thread_cnt = i; /* fix count to threads actually started */
            tpool_destroy(tm);
            return NULL;
        }
        if (pthread_detach(thread) != 0) {
            perror("Thread detach failed");
        }
    }

    return tm;
}

void tpool_destroy(tpool_t *tm) {
    tpool_work_t *work;
    tpool_work_t *work2;

    if (tm == NULL) {
        return;
    }

    pthread_mutex_lock(&(tm->work_mutex));
    work = tm->work_first;
    while (work != NULL) {
        work2 = work->next;
        tpool_work_destroy(work);
        work = work2;
    }
    tm->work_first = NULL;
    tm->stop       = true;
    pthread_cond_broadcast(&(tm->work_cond));
    pthread_mutex_unlock(&(tm->work_mutex));

    tpool_wait(tm);

    pthread_mutex_destroy(&(tm->work_mutex));
    pthread_cond_destroy(&(tm->work_cond));
    pthread_cond_destroy(&(tm->working_cond));

    free(tm);
}

bool tpool_add_work(tpool_t *tm, thread_func_t func, void *arg) {
    tpool_work_t *work;

    if (tm == NULL) {
        return false;
    }

    work = tpool_work_create(func, arg);
    if (work == NULL) {
        return false;
    }

    pthread_mutex_lock(&(tm->work_mutex));
    if (tm->work_first == NULL) {
        tm->work_first = work;
        tm->work_last  = tm->work_first;
    } else {
        tm->work_last->next = work;
        tm->work_last       = work;
    }

    pthread_cond_broadcast(&(tm->work_cond));
    pthread_mutex_unlock(&(tm->work_mutex));

    return true;
}

void tpool_wait(tpool_t *tm) {
    if (tm == NULL) {
        return;
    }

    pthread_mutex_lock(&(tm->work_mutex));
    while (1) {
        if (tm->work_first != NULL ||
            (!tm->stop && tm->working_cnt != 0) ||
            (tm->stop  && tm->thread_cnt  != 0))
        {
            pthread_cond_wait(&(tm->working_cond), &(tm->work_mutex));
        } else {
            break;
        }
    }
    pthread_mutex_unlock(&(tm->work_mutex));
}


static void *tpool_worker(void *arg) {
    tpool_t      *tm = arg;
    tpool_work_t *work;

    while (true) {
        pthread_mutex_lock(&(tm->work_mutex));

        while (tm->work_first == NULL && !tm->stop) {
            pthread_cond_wait(&(tm->work_cond), &(tm->work_mutex));
        }

        if (tm->stop) {
            break;
        }

        work = tpool_work_get(tm);
        tm->working_cnt++;
        pthread_mutex_unlock(&(tm->work_mutex));

        if (work != NULL) {
            work->func(work->arg);
            tpool_work_destroy(work);
        }

        pthread_mutex_lock(&(tm->work_mutex));
        tm->working_cnt--;

        if (!tm->stop && tm->working_cnt == 0 && tm->work_first == NULL) {
            pthread_cond_signal(&(tm->working_cond));
        }
        pthread_mutex_unlock(&(tm->work_mutex));
    }

    tm->thread_cnt--;
    pthread_cond_signal(&(tm->working_cond));
    pthread_mutex_unlock(&(tm->work_mutex));
    return NULL;
}


static tpool_work_t *tpool_work_create(thread_func_t func, void *arg) {
    tpool_work_t *work;

    if (func == NULL) {
        return NULL;
    }

    work = malloc(sizeof(*work));
    if (!work) {
        perror("Work allocation failed");
    }

    work->func = func;
    work->arg  = arg;
    work->next = NULL;

    return work;
}

static void tpool_work_destroy(tpool_work_t *work) {
    if (work == NULL) {
        return;
    }
    free(work);
}

static tpool_work_t *tpool_work_get(tpool_t *tm) {
    tpool_work_t *work;

    work = tm->work_first;
    if (work == NULL) {
        return NULL;
    }

    if (work->next == NULL) {
        tm->work_first = NULL;
        tm->work_last  = NULL;
    } else {
        tm->work_first = work->next;
    }

    return work;
}
