#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <stdio.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include "main.h"

#define NUM_MPS         5
#define PERIOD          7000
#define MP_OFFSET       1001
#define MILLI_TO_SEC    1000


int32_t main(int32_t UNUSED(argc), char UNUSED(*argv[]))
{
    int32_t k, i = 0;

    publishMe = malloc(sizeof(topicToPublish));
    publishMe[0].topic_id = 1000;
    publishMe[0].period = PERIOD;
    publishMe[0].numMPs = NUM_MPS;
    publishMe[0].topicSubscription = malloc(sizeof(MPinfo));
    publishMe[0].topicSubscription = realloc(publishMe[0].topicSubscription, sizeof(MPinfo)*publishMe[0].numMPs);
        
    // setting values
    for (i = 0 ; i < publishMe[0].numMPs ; i++)
    {
        publishMe[0].topicSubscription[i].mp = MP_OFFSET + i;
        publishMe[0].topicSubscription[i].period = PERIOD;
        printf("publishMe[0].topicSubscription[i].mp: %d\n", publishMe[0].topicSubscription[i].mp );
        printf("publishMe[0].topicSubscription[i].period: %d\n", publishMe[0].topicSubscription[i].period );

        publishMe[0].topicSubscription[i].mp_val_long = malloc(sizeof(int32_t));
        publishMe[0].topicSubscription[i].mp_val_long = realloc( publishMe[0].topicSubscription[i].mp_val_long, ( publishMe[0].topicSubscription[i].period / MILLI_TO_SEC )*sizeof(int32_t) );
        for (k = 0 ; k < ( publishMe[0].topicSubscription[i].period / MILLI_TO_SEC ) ; k++ )
        {
            publishMe[0].topicSubscription[i].mp_val_long[k] = k;
            printf("publishMe[0].topicSubscription[i].mp_val_long[k]: %d\n", publishMe[0].topicSubscription[i].mp_val_long[k]);
        }
        // to please valgrind
        free(publishMe[0].topicSubscription[i].mp_val_long);
    }

    // to please valgrind
    free(publishMe[0].topicSubscription);
    free(publishMe);
    return 0;
}
