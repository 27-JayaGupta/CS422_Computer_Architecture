/*************************************************************************
 *
 *  Copyright (c) 1999 Cornell University
 *  Computer Systems Laboratory
 *  Ithaca, NY 14853
 *  All Rights Reserved
 *
 *  Permission to use, copy, modify, and distribute this software
 *  and its documentation for any purpose and without fee is hereby
 *  granted, provided that the above copyright notice appear in all
 *  copies. Cornell University makes no representations
 *  about the suitability of this software for any purpose. It is
 *  provided "as is" without express or implied warranty. Export of this
 *  software outside of the United States of America may require an
 *  export license.
 *
 *  $Id: queue.cc,v 1.1.1.1 2006/05/23 13:53:59 mainakc Exp $
 *
 *************************************************************************/
#include "queue.h"


Queue::Queue (char *name, int size)
{
  char *s;

  Assert (size > 0, "Queue::Queue cannot handle zero length queues");

  sz = size;
  p = 0;
  g = 0;
  put = 0;
  get = 0;
  putCount = 0;
  getCount = 0;

  NEW (pc, eventcount);
  n0 = s = new char[strlen(name)+5];
  sprintf (s, "%s.put", name);
  initialize_event_count (pc, size, s);

  NEW (gc, eventcount);
  n1 = s = new char[strlen(name)+5];
  sprintf (s, "%s.get", name);
  initialize_event_count (gc, 0, s);
}

Queue::~Queue (void)
{
  delete n0;
  delete n1;

  delete_event_count (pc);
  delete_event_count (gc);
}


int Queue::Put (void)
{
  int ret;
  await (pc, ++p);
  putCount++;
  ret = put;
  put = (put + 1) % sz;
  advance (gc);
  return ret;
}

int Queue::Get (void)
{
  int ret;

  await (gc, ++g);
  getCount++;
  ret = get;
  get = (get + 1) % sz;
  advance (pc);
  return ret;
}

int Queue::isEmpty (void)
{
  return (putCount == getCount) ? 1 : 0;
}

int Queue::isFull (void)
{
  return (putCount - getCount == sz) ? 1 : 0;
} 

int
Queue::GetHeadIndex(void)
{
   return get;
}

int 
Queue::PutHeadIndex(void)
{
   return put;
}
count_t
Queue::Getg(void)
{
   return g;
}

count_t
Queue::Getp(void)
{
   return p;
}

count_t
Queue::GetgetCount(void)
{
   return getCount;
}

count_t
Queue::GetputCount(void)
{
   return putCount;
}

#include "checkpoint.h"

void Queue::Save (FILE *fp)
{
  fprintf (fp, "%d %d\n", put, get);
  count_write (fp, p);
  count_write (fp, g);
  SaveQueueState (fp, pc);
  SaveQueueState (fp, gc);
}


void Queue::Restore (FILE *fp, UnCheckPoint *uc)
{
  fscanf (fp, "%d %d", &put, &get);
  count_read (fp, &p);
  count_read (fp, &g);
  RestoreQueueState (fp, pc, uc);
  RestoreQueueState (fp, gc, uc);
}
