#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "memsys.h"


//---- Cache Latencies  ------

#define DCACHE_HIT_LATENCY   1
#define ICACHE_HIT_LATENCY   1
#define L2CACHE_HIT_LATENCY  10

extern MODE   SIM_MODE;
extern uns64  CACHE_LINESIZE;
extern uns64  REPL_POLICY;

extern uns64  DCACHE_SIZE; 
extern uns64  DCACHE_ASSOC; 
extern uns64  ICACHE_SIZE; 
extern uns64  ICACHE_ASSOC; 
extern uns64  L2CACHE_SIZE; 
extern uns64  L2CACHE_ASSOC; 

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////


Memsys *memsys_new(void) 
{
  Memsys *sys = (Memsys *) calloc (1, sizeof (Memsys));

  sys->dcache = cache_new(DCACHE_SIZE, DCACHE_ASSOC, CACHE_LINESIZE, REPL_POLICY);

  if(SIM_MODE!=SIM_MODE_A){
    sys->icache = cache_new(ICACHE_SIZE, ICACHE_ASSOC, CACHE_LINESIZE, REPL_POLICY);
    sys->l2cache = cache_new(L2CACHE_SIZE, L2CACHE_ASSOC, CACHE_LINESIZE, REPL_POLICY);
    sys->dram    = dram_new();
  }

  return sys;

}


////////////////////////////////////////////////////////////////////
// This function takes an ifetch/ldst access and returns the delay
////////////////////////////////////////////////////////////////////

uns64 memsys_access(Memsys *sys, Addr addr, Access_Type type)
{
  uns delay=0;


  // all cache transactions happen at line granularity, so get lineaddr
  Addr lineaddr=addr/CACHE_LINESIZE;
  

  if(SIM_MODE==SIM_MODE_A){
    delay = memsys_access_modeA(sys,lineaddr,type);
  }else{
    delay = memsys_access_modeBC(sys,lineaddr,type);
  }


  //update the stats
  if(type==ACCESS_TYPE_IFETCH){
    sys->stat_ifetch_access++;
    sys->stat_ifetch_delay+=delay;
  }

  if(type==ACCESS_TYPE_LOAD){
    sys->stat_load_access++;
    sys->stat_load_delay+=delay;
  }

  if(type==ACCESS_TYPE_STORE){
    sys->stat_store_access++;
    sys->stat_store_delay+=delay;
  }


  return delay;
}



////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

void memsys_print_stats(Memsys *sys)
{
  char header[256];
  sprintf(header, "MEMSYS");

  double ifetch_delay_avg=0;
  double load_delay_avg=0;
  double store_delay_avg=0;

  if(sys->stat_ifetch_access){
    ifetch_delay_avg = (double)(sys->stat_ifetch_delay)/(double)(sys->stat_ifetch_access);
  }

  if(sys->stat_load_access){
    load_delay_avg = (double)(sys->stat_load_delay)/(double)(sys->stat_load_access);
  }

  if(sys->stat_store_access){
    store_delay_avg = (double)(sys->stat_store_delay)/(double)(sys->stat_store_access);
  }


  printf("\n");
  printf("\n%s_IFETCH_ACCESS  \t\t : %10llu",  header, sys->stat_ifetch_access);
  printf("\n%s_LOAD_ACCESS    \t\t : %10llu",  header, sys->stat_load_access);
  printf("\n%s_STORE_ACCESS   \t\t : %10llu",  header, sys->stat_store_access);
  printf("\n%s_IFETCH_AVGDELAY\t\t : %10.3f",  header, ifetch_delay_avg);
  printf("\n%s_LOAD_AVGDELAY  \t\t : %10.3f",  header, load_delay_avg);
  printf("\n%s_STORE_AVGDELAY \t\t : %10.3f",  header, store_delay_avg);
  printf("\n");

  cache_print_stats(sys->dcache, "DCACHE");

  if(SIM_MODE!=SIM_MODE_A){
    cache_print_stats(sys->icache, "ICACHE");
    cache_print_stats(sys->l2cache, "L2CACHE");
    dram_print_stats(sys->dram);
  }

}


////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

//           Xiaofei's Implementation starts From here            //

uns64 memsys_access_modeA(Memsys *sys, Addr lineaddr, Access_Type type){
  Flag needs_dcache_access=FALSE;
  Flag mark_dirty=FALSE;
  
  if(type == ACCESS_TYPE_IFETCH){
    // no icache in this mode
  }
    
  if(type == ACCESS_TYPE_LOAD){
    needs_dcache_access=TRUE;
    mark_dirty=FALSE;
  }
  
  if(type == ACCESS_TYPE_STORE){
    needs_dcache_access=TRUE;
    mark_dirty=TRUE;
  }

  if(needs_dcache_access){
    Flag outcome=cache_access(sys->dcache, lineaddr, mark_dirty);
    if(outcome==MISS){
      cache_install(sys->dcache, lineaddr, mark_dirty);
    }
  }

  // timing is not simulated in Part A
  return 0;
}

////////////////////////////////////////////////////////////////////
// --------------- DO NOT CHANGE THE CODE ABOVE THIS LINE ----------
////////////////////////////////////////////////////////////////////

uns64 memsys_access_modeBC(Memsys *sys, Addr lineaddr, Access_Type type){
  uns64 delay=0;
  Flag needs_dcache_access=FALSE;
  Flag mark_dirty=FALSE;

 
  if(type == ACCESS_TYPE_IFETCH)
  {
    Flag outcome = cache_access(sys->icache, lineaddr, 0);

    delay = ICACHE_HIT_LATENCY;

    if(outcome==MISS)
    {
      delay = delay + memsys_L2_access(sys, lineaddr, 0);
      //If miss, Allocate space and mark the dirty bit
      cache_install(sys->icache, lineaddr, 0); 

      // then do read from L2.
      
    }
  }
    
  if(type == ACCESS_TYPE_LOAD)
  {
    needs_dcache_access=TRUE;
    mark_dirty=FALSE;
  }
  

  if(type == ACCESS_TYPE_STORE)
  {
    needs_dcache_access=TRUE;
    mark_dirty=TRUE;
  }

  if(needs_dcache_access)
  {
    Flag outcome = cache_access(sys->dcache, lineaddr, mark_dirty);

    delay = DCACHE_HIT_LATENCY;

    if(outcome==MISS)
    {
      cache_install(sys->dcache, lineaddr, mark_dirty);
 
      // Check last_evicted_line. if it is valid and dirty, write to Upper level MEM
      if(sys->dcache->last_evicted_line.valid)
      {
        if(sys->dcache->last_evicted_line.dirty)
        {
          sys->dcache->last_evicted_line.dirty = FALSE;
          sys->dcache->last_evicted_line.valid = FALSE;
          Addr L1_EVCT_LINE_ADDR = sys->dcache->last_evicted_line.tag ;//* sys->dcache->num_sets;
          memsys_L2_access(sys, L1_EVCT_LINE_ADDR, 1);
        }
      }

      // read from L2
      delay = delay + memsys_L2_access(sys, lineaddr, 0);
      
    } 
  }

  return delay;
}

/////////////////////////////////////////////////////////////////////
// This function is called on ICACHE miss, DCACHE miss, DCACHE writeback
// ----- YOU NEED TO WRITE THIS FUNCTION AND UPDATE DELAY ----------
/////////////////////////////////////////////////////////////////////

uns64   memsys_L2_access(Memsys *sys, Addr lineaddr, Flag is_writeback){
  uns64 delay = L2CACHE_HIT_LATENCY;

  Flag outcome;

  if(is_writeback==0)
  {
    // If it is a read request from L1, check outcome of L2. read miss/hit.
    outcome = cache_access(sys->l2cache, lineaddr, 0);

    if(outcome==MISS)
    {
      cache_install(sys->l2cache, lineaddr, 0);
      
      if(sys->l2cache->last_evicted_line.valid)
      {
        if(sys->l2cache->last_evicted_line.dirty)
        {
          sys->l2cache->last_evicted_line.dirty = FALSE;
          sys->l2cache->last_evicted_line.valid = FALSE;
          Addr L2_EVCT_LINE_ADDR = sys->l2cache->last_evicted_line.tag;// * sys->l2cache->num_sets;
          dram_access(sys->dram, L2_EVCT_LINE_ADDR, 1);
        }
      }

      // then  Read from Dram.
      delay = delay + dram_access(sys->dram, lineaddr, 0);
      
    }
  }

  if(is_writeback==1)
  {
    // If it is write back operation, check write miss/hit
    outcome = cache_access(sys->l2cache, lineaddr, 1);

    if(outcome==MISS)
    {

      cache_install(sys->l2cache, lineaddr, 1);

      if(sys->l2cache->last_evicted_line.valid)
      {
        if(sys->l2cache->last_evicted_line.dirty)
        {
          sys->l2cache->last_evicted_line.dirty = FALSE;
          sys->l2cache->last_evicted_line.valid = FALSE;
          Addr L2_EVCT_LINE_ADDR = sys->l2cache->last_evicted_line.tag;// * sys->l2cache->num_sets;
          dram_access(sys->dram, L2_EVCT_LINE_ADDR, 1);
        }
      }

      delay = delay + dram_access(sys->dram, lineaddr, 0);
      
    }
  }

  return delay;
}

