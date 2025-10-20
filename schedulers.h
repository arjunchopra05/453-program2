#ifndef SCHEDULERSH
#define SCHEDULERSH

#include <lwp.h>
extern scheduler AlwaysZero;
extern scheduler ChangeOnSIGTSTP;
extern scheduler ChooseHighestColor;
extern scheduler ChooseLowestColor;

scheduler RoundRobin;

struct schedule_entry {
    struct schedule_entry *prev;
    thread ctx;
    struct schedule_entry *next;
    uint8_t state;
};
#define ST_READY 0
#define ST_RUNNING 1
#define ST_BLOCKED 2

//idk if this is a good approach or if i should just keep it in lwp.c
#endif
