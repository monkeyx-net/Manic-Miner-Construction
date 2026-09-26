#ifndef COLLISION_H
#define COLLISION_H

#include "common.h"

void collision_clear(void);
void collision_add_npc(int pos, const u16 *line, int mirror);
int  collision_hits_npc(int pos, const u16 *line, int mirror);

#endif
