#include <stdio.h>
#include "mem.h"
#include "misc.h"
#include "sim.h"

#ifdef SYNCHRONOUS

#define CHUNK_SIZE 2048		 /*Ocean: 8192*/ 
/*
#define CHUNK_SIZE 16384*/

#define CHUNK_THRESHOLD 16 /* gc threshold */

static int adj_merge_size = CHUNK_SIZE;

/*
#define ADJ_LIMIT 1024*1024*32
*/

#define ADJ_LIMIT CHUNK_SIZE	 /*Ocean*/ 
/*
#define ADJ_LIMIT 1024*32*/

#else 

#define CHUNK_SIZE 16384
#define CHUNK_THRESHOLD 16 /* gc threshold */
static int adj_merge_size = 16384;
#define ADJ_LIMIT 1024*32

#endif

#define DOUBLE_THRESHOLD (1024*1024*256)

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#if 0
inline int Mem::_binsearchw (LL addr)
{
  int i, j, m;
  
  if (chunks == 0) return 0;
  
  i = 0; j = chunks;

  while ((i+1) != j) {
    m = (i+j)/2;
    if (addr < mem_addr[m])
      j = m;
    else
      i = m;
  }
#if 0
  foobar = i;
#endif
  if (mem_addr[i] <= addr && addr <= mem_addr[i] + mem_len[i]) {
    if (i > 0 && mem_addr[i-1] + mem_len[i-1] == addr)
      return i-1;
    return i;
  }
  else {
    /* not found */
    if (addr < mem_addr[0])
      return 0;
    else if (addr > mem_addr[chunks-1] + mem_len[chunks-1])
      return chunks;
    else
      return i+1;
  }
}
#endif

#if 0
/*
 * store value at addr
 */
void 
Mem::Write (LL addr, LL val)
{
  int i, j, w;
  int ch, ret;
  LL len, sz;
  LL *data;

  addr >>= MEM_ALIGN;

  context_disable ();

  w = _binsearchw (addr);
  /*if ((addr==(0x42d450>>3)) && !Read(0x400fe0)) {
     printf("aaa\n");
     exit(0);
  }*/
  if (w < chunks &&
      (mem_addr[w] <= addr && addr < mem_addr[w]+mem_len[w])) {
    mem[w][addr-mem_addr[w]] = val;
    context_enable ();
    /*if ((addr==(0x42d450>>3)) && !Read(0x400fe0)) {
     printf("bbb\n");
     exit(0);
    }*/
    return;
  }
  i = w;

  if (i < chunks && chunks > 0 && (addr == mem_addr[i] + mem_len[i])) {
    /* check merging chunks! */
    if (i < chunks-1 && mem_addr[i+1] == addr) {
      /* merge two chunks! */
      merge_chunks (i,i+1);
      mem[i][addr-mem_addr[i]] = val;
      context_enable();
      /*if ((addr==(0x42d450>>3)) && !Read(0x400fe0)) {
     printf("ccc\n");
     exit(0);
      }*/
      return;
    }
    /* extend chunk! */
    get_more (i, addr-mem_addr[i]-mem_len[i]+1);
    mem[i][addr-mem_addr[i]] = val;
    context_enable();
    /*if ((addr==(0x42d450>>3)) && !Read(0x400fe0)) {
     printf("ddd\n");
     exit(0);
    }*/
    return;
  }
  /* we need to move other chunks to the end */
  ch = i;
//  if ((addr==(0x42d450>>3)) && !Read(0x400fe0)) {
//     printf("eee1\n");
     //exit(0);
//  }
  ret = create_new_chunk (addr, ch);
//  if ((addr==(0x42d450>>3)) && !Read(0x400fe0)) {
//     printf("eee\n");
     //exit(0);
//  }
  if (ret == 1) {
    /* save data and len */
    data = mem[chunks-1];
    len = mem_len[chunks-1];

    /* move other chunks up */
    for (j = chunks-1; j >= ch+1; j--) {
      mem_addr[j] = mem_addr[j-1];
      mem_len[j] = mem_len[j-1];
      mem[j] = mem[j-1];
      /*if ((addr==(0x42d450>>3)) && !Read(0x400fe0)) {
     printf("%d fff\n", j);
     exit(0);
     }*/
    }

    /* set chunk info */
    mem[ch] = data;
    mem_len[ch] = len;
    mem_addr[ch] = addr;
    /*if ((addr==(0x42d450>>3)) && !Read(0x400fe0)) {
     printf("ggg\n");
     exit(0);
    }*/
    get_more (ch, addr-mem_addr[ch]-mem_len[ch]+1);
    /*if ((addr==(0x42d450>>3)) && !Read(0x400fe0)) {
     printf("hhh\n");
     exit(0);
    }*/
    mem[ch][0] = val;
    /*if ((addr==(0x42d450>>3)) && !Read(0x400fe0)) {
     printf("iii\n");
     exit(0);
    }*/
  }
  else if (ret == 0) {
    mem[ch][addr - mem_addr[ch]] = val;
    /*if ((addr==(0x42d450>>3)) && !Read(0x400fe0)) {
     printf("jjj\n");
     exit(0);
    }*/
  }
  else {
    fatal_error ("Unknown ret value");
  }

  chunk_gc (0);

  context_enable();
  /*if ((addr==(0x42d450>>3)) && !Read(0x400fe0)) {
     printf("kkk\n");
     exit(0);
  }*/ 
  return;
}
#endif

#if 0
/*
 * read memory image from a file
 */
int 
Mem::ReadImage (FILE *fp)
{
  freemem ();
  return MergeImage (fp);
}

/*
 * read memory image from a file
 */
int 
Mem::ReadImage (char *s)
{
  FILE *fp;
  int i;

  if (!s) return 0;
  if (!(fp = fopen (s, "r")))
    return 0;
  freemem ();
  i = MergeImage (fp);
  fclose (fp);

  return i;
}

/*
 * read memory image from a file
 */
int 
Mem::MergeImage (char *s)
{
  FILE *fp;
  int i;

  if (!s) return 0;
  if (!(fp = fopen (s, "r")))
    return 0;
  i = MergeImage (fp);
  fclose (fp);

  return i;
}

/*
 * read memory image from a file
 */
int 
Mem::MergeImage (FILE *fp)
{
  char buf[1024];
  unsigned int a0,a1,v0,v1;
  unsigned int len;
  LL a, v, tmp;
  int endian;
  int done;

  done = 1;
  while (fgets (buf, 1024, fp)) {
    if (buf[0] == '!') {
      done = 0;
      break;
    }
    if (buf[0] == '\n') continue;
    if (buf[0] == '#') continue;
    if (buf[0] == '@') {
      /*
	Format: @address data length:

	 "length" data items starting at "address" all contain
	 "data"
      */
#if MEM_ALIGN == 3
      sscanf (buf+1, "0x%8x%8x 0x%8x%8x %lu", &a0, &a1, &v0, &v1, &len);
      a = ((LL)a0 << 32) | (LL)a1;
      v = ((LL)v0 << 32) | (LL)v1;
#elif MEM_ALIGN == 2
      sscanf (buf+1, "0x%8x 0x%8x %lu", &a, &v, &len);
#else
#error Unknown memory format
#endif
      while (len > 0) {
	Write (a, v);
	a += (1 << MEM_ALIGN);
	len--;
      }
      continue;
    }
    else if (buf[0] == '*') {
      /*
	Format: *address length
	        <list of data>

	 "length" locations starting at "address" contain the data
	 specified in the following "length" lines.
      */
#if MEM_ALIGN == 3
      sscanf (buf+1, "0x%8x%8x %lu", &a0, &a1, &len);
      a = ((LL)a0 << 32) | (LL)a1;
#elif MEM_ALIGN == 2
      sscanf (buf+1, "0x%8x %lu", &a, &len);
#else
#error Unknown memory format
#endif
      while (len > 0 && fgets (buf, 1024, fp)) {
//         if ((a>0x400fe0) && !Read(0x400fe0)) {
//           printf("aa%llx\n", a); fflush(stdout);
//           exit(0);
//         }
#if MEM_ALIGN == 3
	sscanf (buf, "0x%8x%8x", &v0, &v1);
//        if ((a>0x400fe0) && !Read(0x400fe0)) {
//           printf("bb%llx\n", a); fflush(stdout);
//           exit(0);
//        }
	v = ((LL)v0 << 32) | (LL)v1;
//        if ((a>0x400fe0) && !Read(0x400fe0)) {
//           printf("cc%llx\n", a); fflush(stdout);
//           exit(0);
//        }
#elif MEM_ALIGN == 2
	sscanf (buf, "0x%8x", &v);
#else
#error Unknown memory format
#endif
	Write (a, v);
//        if (!Read(0x400fe0)) {
//           printf("dd%llx\n", a); fflush(stdout);
//           exit(0);
//        }
	a += (1 << MEM_ALIGN);
	len--;
      }
      if (len != 0) {
	fatal_error ("Memory format error, *0xaddr len format ends prematurely");
      }
      continue;
    }
    else if (buf[0] == '+' || buf[0] == '-') {
      /* 
	 Handle non-aligned data

	 + or - followed by
	 Format: address length
	         <list of data>

         Starting at address, the following length *bytes* are
	 specified below.

	 Assumptions:
	      length < (1 << MEM_ALIGN)	 
	      +  means big-endian
	      -  means little-endian
      */
      if (buf[0] == '+')
	endian = 1; /* big-endian */
      else
	endian = 0; /* little-endian */
#if MEM_ALIGN == 3
      sscanf (buf+1, "0x%8x%8x %lu", &a0, &a1, &len);
      a = ((LL)a0 << 32) | (LL)a1;
#elif MEM_ALIGN == 2
      sscanf (buf+1, "0x%8x %lu", &a, &len);
#else
#error Unknown memory format
#endif
      while (len > 0 && fgets (buf, 1024, fp)) {
#if MEM_ALIGN == 3
	sscanf (buf, "0x%2x", &v0);
	v = v0;
#elif MEM_ALIGN == 2
	sscanf (buf, "0x%2x", &v);
#else
#error Unknown memory format
#endif
	tmp = Read (a);
	/* write the byte! */
	if (endian) {
	  tmp = (tmp & ~(0xffULL << ((~a & 3)<<3))) | (v << ((~a&3)<<3));
	}
	else {
	  tmp = (tmp & ~(0xffULL << ((a & 3)<<3))) | (v << ((a&3)<<3));
	}
	Write (a, tmp);
	a += 1;
	len--;
      }
      if (len != 0) {
	fatal_error ("Memory format error, *0xaddr len format ends prematurely");
      }
      continue;
    }
#if MEM_ALIGN == 3
    sscanf (buf, "0x%8x%8x 0x%8x%8x", &a0, &a1, &v0, &v1);
    a = ((LL)a0 << 32) | (LL)a1;
    v = ((LL)v0 << 32) | (LL)v1;
#elif MEM_ALIGN == 2
    sscanf (buf, "0x%8x 0x%8x", &a, &v);
#else
#error Unknown memory format
#endif
    Write (a, v);
  }
  if (done) {
    fclose (fp);
  }

  a = 0;
  for (a1=0; a1 < chunks; a1++)
    a += mem_len[a1];

#if 0
  {
    DUMPCHUNKS(0);
    
    printf ("************************************************************************\n");
    chunk_gc (1);
    DUMPCHUNKS(0);
    printf ("************************************************************************\n");
  }
#endif

  return a;
}
#endif

#if 0
/*
 * create new chunk, at position "chunks-1"
 *
 * Return values:
 *
 *   1  => created a new chunk at array position "chunks-1"
 *   0  => resized array at position ch
 */
int Mem::create_new_chunk (LL addr, int ch)
{
  int i, gap;
  static int last_create = -1;

#if 0
  printf ("Creating new chunk: 0x%llx @ %d, curcount = %d\n", addr,
	  ch, chunks);
#endif

  if (chunks == numchunks) {
    if (numchunks == 0) {
      numchunks = 4;
      MALLOC (mem, LL *, numchunks);
      MALLOC (mem_len, LL, numchunks);
      MALLOC (mem_addr, LL, numchunks);
      /*if ((addr==(0x42d450>>3)) && !Read(0x400fe0)) {
     printf("aaaa\n");
     exit(0);
     }*/
    }
    else {
      numchunks *= 2;
      REALLOC (mem, LL *, numchunks);
      REALLOC (mem_len, LL, numchunks);
      REALLOC (mem_addr, LL, numchunks);
      /*if ((addr==(0x42d450>>3)) && !Read(0x400fe0)) {
     printf("bbbb\n");
     exit(0);
     }*/
    }
  }
  if (0 <= ch && ch < chunks) {
    /* the new chunk is right next to "ch" */
    if (mem_addr[ch] - addr >= CHUNK_SIZE) {
      mem_len[chunks] = CHUNK_SIZE;
      /*if ((addr==(0x42d450>>3)) && !Read(0x400fe0)) {
     printf("cccc\n");
     exit(0);
     }*/
    }
    else {
      /* next chunk is close by, we need to just adjust the size of
	 the next chunk by CHUNK_SIZE *backward*
      */
      if (ch > 0) {
	if (last_create == ch) {
	  adj_merge_size *= 2;
	  if (adj_merge_size > ADJ_LIMIT)
	    adj_merge_size = ADJ_LIMIT;
	}
	last_create = ch;
	
	gap = mem_addr[ch] - mem_len[ch-1] - mem_addr[ch-1];
	gap = MIN (gap, adj_merge_size);
	REALLOC (mem[ch], LL, mem_len[ch] + gap);
        /*if ((addr==(0x42d450>>3)) && !Read(0x400fe0)) {
     printf("dddd\n");
     exit(0);
       }*/
	for (i=mem_len[ch]-1; i >= 0; i--) {
	  mem[ch][i+gap] = mem[ch][i];
        }
        /*if ((addr==(0x42d450>>3)) && !Read(0x400fe0)) {
     printf("eeee\n");
     exit(0);
	}*/
	for (i=0; i < gap; i++) {
	  mem[ch][i] = MEM_BAD;
	}
        /*if ((addr==(0x42d450>>3)) && !Read(0x400fe0)) {
     printf("ffff\n");
     exit(0);
        }*/
	mem_addr[ch] = mem_addr[ch] - gap;
	mem_len[ch] = mem_len[ch] + gap;
//        if ((addr==(0x42d450>>3)) && !Read(0x400fe0)) {
//     printf("gggg\n"); fflush(stdout);
//     exit(0);
//       }
	return 0;
      }
      mem_len[chunks] = mem_addr[ch] - addr;
      /*if ((addr==(0x42d450>>3)) && !Read(0x400fe0)) {
     printf("hhhh\n");
     exit(0);
     }*/
    }
    Assert (mem_len[chunks] > 0, "What?");
  }
  else {
    mem_len[chunks] = CHUNK_SIZE;
    /*if ((addr==(0x42d450>>3)) && !Read(0x400fe0)) {
     printf("iiii\n");
     exit(0);
    }*/
  }
  MALLOC (mem[chunks], LL, mem_len[chunks]);
  mem_addr[chunks] = 0;
  /*if ((addr==(0x42d450>>3)) && !Read(0x400fe0)) {
     printf("jjjj\n");
     exit(0);
  }*/
  for (i=0;i<=mem_len[chunks]-1;i++) {
     mem[chunks][i] = MEM_BAD;
  }
//   if (addr==(0x42d450>>3)) {
//     printf("kkkk%llx\n", Read(0x400fe0));fflush(stdout);
//  }
  chunks++;
  return 1;
}
#endif
