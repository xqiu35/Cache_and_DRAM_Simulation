#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "cache.h"


extern uns64 cycle_count; // You can use this as timestamp for LRU

////////////////////////////////////////////////////////////////////
// ------------- DO NOT MODIFY THE INIT FUNCTION -----------
////////////////////////////////////////////////////////////////////

Cache  *cache_new(uns64 size, uns64 assoc, uns64 linesize, uns64 repl_policy){

   Cache *c = (Cache *) calloc (1, sizeof (Cache));
   c->num_ways = assoc;
   c->repl_policy = repl_policy;

   if(c->num_ways > MAX_WAYS){
     printf("Change MAX_WAYS in cache.h to support %llu ways\n", c->num_ways);
     exit(-1);
   }

   // determine num sets, and init the cache
   c->num_sets = size/(linesize*assoc);
   c->sets  = (Cache_Set *) calloc (c->num_sets, sizeof(Cache_Set));

   return c;
}

////////////////////////////////////////////////////////////////////
// ------------- DO NOT MODIFY THE PRINT STATS FUNCTION -----------
////////////////////////////////////////////////////////////////////

void    cache_print_stats    (Cache *c, char *header){
  double read_mr =0;
  double write_mr =0;

  if(c->stat_read_access){
    read_mr=(double)(c->stat_read_miss)/(double)(c->stat_read_access);
  }

  if(c->stat_write_access){
    write_mr=(double)(c->stat_write_miss)/(double)(c->stat_write_access);
  }

  printf("\n%s_READ_ACCESS    \t\t : %10llu", header, c->stat_read_access);
  printf("\n%s_WRITE_ACCESS   \t\t : %10llu", header, c->stat_write_access);
  printf("\n%s_READ_MISS      \t\t : %10llu", header, c->stat_read_miss);
  printf("\n%s_WRITE_MISS     \t\t : %10llu", header, c->stat_write_miss);
  printf("\n%s_READ_MISSPERC  \t\t : %10.3f", header, 100*read_mr);
  printf("\n%s_WRITE_MISSPERC \t\t : %10.3f", header, 100*write_mr);
  printf("\n%s_DIRTY_EVICTS   \t\t : %10llu", header, c->stat_dirty_evicts);

  printf("\n");
}


////////////////////////////////////////////////////////////
/*          Xiaofei's Implementation starts from here     */

////////////////////////////////////////////////////////////////////
// Note: the system provides the cache with the line address
// Return HIT if access hits in the cache, MISS otherwise 
// Also if mark_dirty is TRUE, then mark the resident line as dirty
// Update appropriate stats
////////////////////////////////////////////////////////////////////

Flag    cache_access(Cache *c, Addr lineaddr, uns mark_dirty)
{
  Flag outcome=MISS;

  Addr Target = lineaddr;// / (c->num_sets);

  uns64 Set_Index = lineaddr % (c->num_sets);

  uns64 NumOfWays = c->num_ways;

  uns64 i=0;

  if(mark_dirty)
  {
    c->stat_write_access++;
  }
  else
  {
    c->stat_read_access++;
  }

  for(i=0;i<NumOfWays;i++)
  {
    if(c->sets[Set_Index].line[i].valid==TRUE)
    {
      if(Target == c->sets[Set_Index].line[i].tag)
      {
        outcome = HIT;
        c->sets[Set_Index].line[i].last_access_time = cycle_count;

        if(mark_dirty)
        {
          c->sets[Set_Index].line[i].dirty = TRUE;
        }

      }
    }
  }

  if(outcome==MISS)
  {
    if(mark_dirty)
    {
      c->stat_write_miss++;
    }
    else
    {
      c->stat_read_miss++;
    }
  }

  return outcome;
}

////////////////////////////////////////////////////////////////////
// Note: the system provides the cache with the line address
// Install the line: determine victim using repl policy (LRU/RAND)
// copy victim into last_evicted_line for tracking writebacks
////////////////////////////////////////////////////////////////////

void    cache_install(Cache *c, Addr lineaddr, uns mark_dirty)
{
  Addr Target = lineaddr;// / (c->num_sets);

  uns64 Set_Index = lineaddr % (c->num_sets);

  uns64 NumOfWays = c->num_ways;

  uns64 Replace_LineAddr=NumOfWays;

  for(uns64 i=0;i<NumOfWays;i++)
  {
    if(c->sets[Set_Index].line[i].valid == FALSE)
    {
      Replace_LineAddr = i;
      break;
    }
  }
  
  if(Replace_LineAddr == NumOfWays)
  {
    if(c->repl_policy) // repl_policy = 1 => Rand
    {
      Replace_LineAddr = rand() % NumOfWays; 
    }
    else //LRU
    {
      uns64 i=0;
      uns SmallestCycleCount = cycle_count;

      for(i=0;i<NumOfWays;i++)
      {
        if(c->sets[Set_Index].line[i].last_access_time < SmallestCycleCount)
        {
          SmallestCycleCount = c->sets[Set_Index].line[i].last_access_time;
          Replace_LineAddr = i;
        }
      }
    }

    if(c->sets[Set_Index].line[Replace_LineAddr].dirty)
    {
      c->stat_dirty_evicts++;
    }

    c->last_evicted_line = c->sets[Set_Index].line[Replace_LineAddr];
  }

  else
  {
    c->last_evicted_line.valid = FALSE; // set valid bit of last_evicted_line to 0 if there is a free space
  }  


  c->sets[Set_Index].line[Replace_LineAddr].valid = TRUE;
  c->sets[Set_Index].line[Replace_LineAddr].tag = Target;
  c->sets[Set_Index].line[Replace_LineAddr].dirty = mark_dirty;
  c->sets[Set_Index].line[Replace_LineAddr].last_access_time = cycle_count;



  // Your Code Goes Here
  // Note: You can use cycle_count as timestamp for LRU
}


////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////


