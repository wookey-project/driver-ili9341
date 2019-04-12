static char bus_owner;
static int wait_queue;
//id=0 => bus libre

void lock_bus(char id)
{
  while(bus_owner && (wait_queue>>(id-1)));
  bus_owner=id;
}

int try_lock_bus(char id)
{
  if(bus_owner)
  {
    wait_queue|=(1<<(id-1));
    return 0;
  }
  bus_owner=id;
  return 1;
}

void unlock_bus()
{
  bus_owner=0;
}

void clear_wait(char id)
{
	wait_queue&=~(1<<(id-1));
}
