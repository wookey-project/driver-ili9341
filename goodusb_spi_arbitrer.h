#ifndef ARBITRER_H
#define ARBITRER_H
void lock_bus(char id);
int try_lock_bus(char id);
void unlock_bus();
void clear_wait(char id);
#endif //ARBITRER_H
