/*************************************************************************
 *
 *  Copyright (c) 1999 Cornell University
 *  Computer Systems Laboratory
 *  Cornell University, Ithaca, NY 14853
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
 *  $Id: array.h,v 1.1.1.1 2006/05/23 13:53:59 mainakc Exp $
 *
 **************************************************************************
 */
#ifndef __ARRAY_H__
#define __ARRAY_H__

#include "misc.h"

/*
 *
 *  Declaration:
 *
 *  struct foo {
 *      A_DECL (type, name);
 *      ...
 *  } f;
 *
 *  (use L_A_DECL if you want the vars to be static)
 *
 *  Initialize:
 *
 *    A_INIT(f.name);
 *
 *  Write a value:
 *     A_NEW(f.name, type);
 *     A_NEXT(f.name) = value;
 *     A_INC(f.name);
 *
 *  Use A_NEWM instead of A_NEW if you want the allocation to start
 *  out with 1 element.
 *
 *  Loop over values:
 *     for (i=0; i < A_LEN(f.name); i++)
 *	    function (f.name[i]);
 *  
 *  Free values:
 *     for (i=0; i < A_LEN(f.name); i++)
 *          free f.name[i];
 *     A_FREE (f.name);
 *
 */


#define A_DECL(type,name)  int name##_num; int name##_max; type *name;
#define L_A_DECL(type,name)  static int name##_num = 0; static int name##_max = 0; static type *name = NULL;

#define A_LEN(name)  name##_num
#define A_MAX(name)  name##_max

#define A_ARG_DECL(type,name)  int name##_num, int name##_max, type *name
#define A_ARG(name)  name##_num,name##_max,name

#define A_INIT(name) do { A_LEN(name) = 0; A_MAX(name) = 0; } while (0)

#define A_ASSIGN(nn,on)				\
   do {						\
      A_FREE (nn);				\
      A_LEN(nn) = A_LEN(on);			\
      A_MAX(nn) = A_MAX(on);			\
      A_LEN(on) = A_MAX(on) = 0;		\
      nn = on;					\
   } while (0)

#define A_NEW(name,type)			\
    do {					\
      if (A_LEN(name) == A_MAX(name)) {		\
	if (A_MAX(name) == 0) {			\
	  A_MAX(name) = 32;			\
	  MALLOC (name, type, A_MAX(name));	\
	}					\
	else {					\
	  A_MAX(name) = 2*A_MAX(name);		\
	  REALLOC (name, type, A_MAX(name));	\
	}					\
      }						\
    } while (0)

/* like a_new, but uses smallest allocation to start off */
#define A_NEWM(name,type)			\
    do {					\
      if (A_LEN(name) == A_MAX(name)) {		\
	if (A_MAX(name) == 0) {			\
	  A_MAX(name) = 1;			\
	  MALLOC (name, type, A_MAX(name));	\
	}					\
	else {					\
	  A_MAX(name) = 2*A_MAX(name);		\
	  REALLOC (name, type, A_MAX(name));	\
	}					\
      }						\
    } while (0)

#define A_NEXT(name)  name[A_LEN(name)]
#define A_INC(name)  A_LEN(name)++

#define A_FREE(name)  do { if (A_MAX(name) > 0) FREE (name); A_INIT (name); } while (0)

#endif /* __ARRAY_H__ */
