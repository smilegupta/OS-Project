
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>

#define N_DISHES        2   
#define N_CATS          6  
#define N_MICE          2 

#define CAT_WAIT        15      
#define CAT_EAT         1       
#define CAT_N_EAT       4       
#define MOUSE_WAIT      1       
#define MOUSE_EAT       1      
#define MOUSE_N_EAT     4       

typedef struct dish {
    int free_dishes;            
    int cats_eating;           
    int mice_eating;           
    int cats_waiting;         
    enum {
        none_eating,
        cat_eating,
        mouse_eating
    } status[N_DISHES];         
    pthread_mutex_t mutex;      
    pthread_cond_t free_cv;     
    pthread_cond_t cat_cv;      
} dish_t;

static const char *progname = "pets";


static void
dump_dish(const char *name, pthread_t pet, const char *what,
          dish_t *dish, int my_dish)
{
    int i;
    struct tm t;
    time_t tt;
    
    tt = time(NULL);
    assert(tt != (time_t) -1);
    localtime_r(&tt, &t);

    printf("%02d:%02d:%02d [", t.tm_hour, t.tm_min, t.tm_sec);
    for (i = 0; i < N_DISHES; i++) {
        if (i) printf(":");
        switch (dish->status[i]) {
        case none_eating:
            printf("-");
            break;
        case cat_eating:
            printf("c");
            break;
        case mouse_eating:
            printf("m");
            break;
        }
    }
    printf("] %s (id %x) %s eating from dish %d\n", name, pet, what, my_dish);
}


static void* 
cat(void *arg)
{
    dish_t *dish = (dish_t *) arg;
    int n = CAT_N_EAT;
    int my_dish = -1;
    int i;

    for (n = CAT_N_EAT; n > 0; n--) {

        pthread_mutex_lock(&dish->mutex);
        pthread_cond_broadcast(&dish->cat_cv);
        dish->cats_waiting++;
        while (dish->free_dishes <= 0 || dish->mice_eating > 0) {
            pthread_cond_wait(&dish->free_cv, &dish->mutex);
        }
        dish->cats_waiting--;
        assert(dish->free_dishes > 0);
        dish->free_dishes--;
        assert(dish->cats_eating < N_CATS);
        dish->cats_eating++;
        
        for (i = 0; i < N_DISHES && dish->status[i] != none_eating; i++) ;
        my_dish = i;
        assert(dish->status[my_dish] == none_eating);
        dish->status[my_dish] = cat_eating;
        dump_dish("cat", pthread_self(), "started", dish, my_dish);
        pthread_mutex_unlock(&dish->mutex);

        sleep(CAT_EAT);
        
        pthread_mutex_lock(&dish->mutex);
        assert(dish->free_dishes < N_DISHES);
        dish->free_dishes++;
        assert(dish->cats_eating > 0);
        dish->cats_eating--;
        dish->status[my_dish] = none_eating;

        pthread_cond_broadcast(&dish->free_cv);
        dump_dish("cat", pthread_self(), "finished", dish, my_dish);
        pthread_mutex_unlock(&dish->mutex);

        sleep(rand() % CAT_WAIT);
    }

    return NULL;
}


static void* 
mouse(void *arg)
{
    dish_t *dish = (dish_t *) arg;
    int n = MOUSE_N_EAT;
    struct timespec ts;
    struct timeval tp;
    int my_dish;
    int i;

    for (n = MOUSE_N_EAT; n > 0; n--) {

        pthread_mutex_lock(&dish->mutex);
        while (dish->free_dishes <= 0 || dish->cats_eating > 0
               || dish->cats_waiting > 0) {
            pthread_cond_wait(&dish->free_cv, &dish->mutex);
        }

        assert(dish->free_dishes > 0);
        dish->free_dishes--;
        assert(dish->cats_eating == 0);
        assert(dish->mice_eating < N_MICE);
        dish->mice_eating++;

        for (i = 0; i < N_DISHES && dish->status[i] != none_eating; i++) ;
        my_dish = i;
        assert(dish->status[my_dish] == none_eating);
        dish->status[my_dish] = mouse_eating;
        dump_dish("mouse", pthread_self(), "started", dish, my_dish);
        pthread_mutex_unlock(&dish->mutex);
        
        gettimeofday(&tp,NULL);
        ts.tv_sec  = tp.tv_sec;
        ts.tv_nsec = tp.tv_usec * 1000;
        ts.tv_sec += MOUSE_EAT;
        pthread_mutex_lock(&dish->mutex);
        pthread_cond_timedwait(&dish->cat_cv, &dish->mutex, &ts);
        pthread_mutex_unlock(&dish->mutex);
        
        pthread_mutex_lock(&dish->mutex);
        assert(dish->free_dishes < N_DISHES);
        dish->free_dishes++;
        assert(dish->cats_eating == 0);
        assert(dish->mice_eating > 0);
        dish->mice_eating--;
        dish->status[my_dish]=none_eating;

        pthread_cond_broadcast(&dish->free_cv);
        dump_dish("mouse", pthread_self(), "finished", dish, my_dish);
        pthread_mutex_unlock(&dish->mutex);
        
        sleep(rand() % MOUSE_WAIT);
    }

    return NULL;
}



int
main(int argc, char *argv[])
{
    int i, err;
    dish_t _dish, *dish;
    pthread_t cats[N_CATS];
    pthread_t mice[N_MICE];

    srand(time(NULL));  

    dish = &_dish;
    memset(dish, 0, sizeof(dish_t));
    dish->free_dishes = N_DISHES;
    pthread_mutex_init(&dish->mutex, NULL);
    pthread_cond_init(&dish->free_cv, NULL);
    pthread_cond_init(&dish->cat_cv, NULL);
    
    for (i = 0; i < N_CATS; i++) {
        err = pthread_create(&cats[i], NULL, cat, dish);
        if (err != 0) {
            fprintf(stderr, "%s: %s: unable to create cat thread %d: %d\n",
                    progname, __func__, i, err);
        }
    }

    for (i = 0; i < N_MICE; i++) {
        err = pthread_create(&mice[i], NULL, mouse, dish);
        if (err != 0) {
            fprintf(stderr, "%s: %s: unable to create mouse thread %d: %d\n",
                    progname, __func__, i, err);
        }
    }

    for (i = 0; i < N_CATS; i++) {
        (void) pthread_join(cats[i], NULL);
    }
    for (i = 0; i < N_MICE; i++) {
        (void) pthread_join(mice[i], NULL);
    }
    
    pthread_mutex_destroy(&dish->mutex);
    pthread_cond_destroy(&dish->free_cv);
    pthread_cond_destroy(&dish->cat_cv);
    
    return EXIT_SUCCESS;
}
