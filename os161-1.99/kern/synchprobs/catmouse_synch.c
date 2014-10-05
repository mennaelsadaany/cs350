#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>

/* 
 * This simple default synchronization mechanism allows only creature at a time to
 * eat.   The globalCatMouseSem is used as a a lock.   We use a semaphore
 * rather than a lock so that this code will work even before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

struct lock ** bowllock
int nummice;
int numcats;
struct cv* mice = cv_create("mice");
struct cv* cats = cv_create("cats");


/* 
 * The CatMouse simulation will call this function once before any cat or
 * mouse tries to each.
 *
 * You can use it to initialize synchronization and other variables.
 * 
 * parameters: the number of bowls
 */
void
catmouse_sync_init(int bowls)
{
  nummice=0;
  numcats=0;
  for (int i=0; i<= bowls; i++){
    bowllock[i]= lock_create(bowls);
  }
  return;
}

/* 
 * The CatMouse simulation will call this function once after all cat
 * and mouse simulations are finished.
 *
 * You can use it to clean up any synchronization and other variables.
 *
 * parameters: the number of bowls
 */
void
catmouse_sync_cleanup(int bowls)
{
  for (int i=0; i<= bowls; i++){
    KASSERT(mybowl != NULL);
    lock_destroy(bowllock[i]);
  }
  KASSERT(mice != NULL);
  KASSERT(cats != NULL);
  cv_destroy(mice);
  cv_destroy(cats);
}


/*
 * The CatMouse simulation will call this function each time a cat wants
 * to eat, before it eats.
 * This function should cause the calling thread (a cat simulation thread)
 * to block until it is OK for a cat to eat at the specified bowl.
 *
 * parameter: the number of the bowl at which the cat is trying to eat
 *             legal bowl numbers are 1..NumBowls
 *
 * return value: none
 */

void
cat_before_eating(unsigned int bowl) 
{
  mybowl= bowllock[bowl]; 
  lock_acquire(mybowl); //get lock for specific bowl 

  while(nummice > 0){ //if there are mice eating 
    cv_wait(cats,mybowl); //wait until cats are allowed to eat, then get the lock 
  }
  numcats++; //another cat is now eating 
}

/*
 * The CatMouse simulation will call this function each time a cat finishes
 * eating.
 *
 * You can use this function to wake up other creatures that may have been
 * waiting to eat until this cat finished.
 *
 * parameter: the number of the bowl at which the cat is finishing eating.
 *             legal bowl numbers are 1..NumBowls
 *
 * return value: none
 */

void
cat_after_eating(unsigned int bowl) 
{
  numcats--; //a cat finished eating

  if (numcats == 0){ //no more cats are eating 
    for (int i=0; i<= bowl; i++){ //go through all bowls and signal that a mouse can eat
      cv_signal(mice, bowllock[i]);
    }
  }
  lock_do_i_hold(bowllock[bowl]);
  lock_release(bowllock[bowl]); 
}

/*
 * The CatMouse simulation will call this function each time a mouse wants
 * to eat, before it eats.
 * This function should cause the calling thread (a mouse simulation thread)
 * to block until it is OK for a mouse to eat at the specified bowl.
 *
 * parameter: the number of the bowl at which the mouse is trying to eat
 *             legal bowl numbers are 1..NumBowls
 *
 * return value: none
 */

void
mouse_before_eating(unsigned int bowl) 
{
  mybowl= bowllock[bowl]; 
  lock_acquire(mybowl); //get lock for specific bowl 

  while(numcats > 0){ //if there are cats eating 
    cv_wait(mice,mybowl); //wait until mice are allowed to eat, then get the lock 
  }
  nummice++; //another cat is now eating
}

/*
 * The CatMouse simulation will call this function each time a mouse finishes
 * eating.
 *
 * You can use this function to wake up other creatures that may have been
 * waiting to eat until this mouse finished.
 *
 * parameter: the number of the bowl at which the mouse is finishing eating.
 *             legal bowl numbers are 1..NumBowls
 *
 * return value: none
 */

void
mouse_after_eating(unsigned int bowl) 
{
  nummice--; //a cat finished eating

  if (nummice == 0){ //no more cats are eating 
    for (int i=0; i<= bowl; i++){ //go through all bowls and signal that a cat can eat
      cv_signal(cats, bowllock[i]); 
    }
  }
  lock_do_i_hold(bowllock[bowl]);
  lock_release(bowllock[bowl]); 
}
