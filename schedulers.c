#include "schedulers.h"
#include "lwp.h"
#include "smartalloc.h"
#include <stdio.h>
#include <sys/types.h>
#include <stdint.h>

struct scheduler rr_publish = {NULL, NULL, rr_admit, rr_remove, rr_next};
scheduler RoundRobin = &rr_publish;

static struct schedule_entry *pool_head = NULL;

void rr_admit(thread new) {
    if (!pool_head) {
        pool_head = (struct schedule_entry *) smartalloc(sizeof(struct schedule_entry), NULL, 0, 0);
        pool_head->ctx = new;
        pool_head->next = NULL;
        pool_head->prev = NULL;
        pool_head->state = ST_READY;
    }
    else {
        struct schedule_entry *new_entry = (struct schedule_entry *) smartalloc(sizeof(struct schedule_entry), NULL, 0, 0);
        new_entry->ctx = new;
        new_entry->next = pool_head;
        new_entry->prev = pool_head->prev;
        pool_head->prev->next = new_entry;
        new_entry->state = ST_READY;
    }
    return;
}

void rr_remove(thread victim) {
    struct schedule_entry *curr = pool_head;
    while (curr->ctx != victim) //may not work, pointer stuff
    {
        curr = curr->next;
    }
    curr->next->prev = curr->prev;
    curr->prev->next = curr->next;
    smartfree(curr, NULL, 0);
    return;
}

thread rr_next(void) {
    pool_head = pool_head->next;
    pool_head->state = ST_RUNNING;
    return pool_head->ctx;
}