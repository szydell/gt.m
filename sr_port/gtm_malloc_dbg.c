/****************************************************************
 *								*
 * Copyright (c) 2001-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* gtm_malloc_dbg -- the debugging version

   The bulk of the GTM storage manager code now sits in gtm_malloc_src.h and
   is included twice: once in gtm_malloc.c and again in gtm_malloc_dbg.c.
   The reason for this is that the production modules built for distribution
   in the field can have both forms of storage mgmt available in the event
   that it becomes necessary to chase a corruption issue. It can now be done
   without resorting to a "debug" version. Several different levels of
   debugging will also be made available to catch various problems.

   If the DEBUG flag is not defined (indicating a pro build), the gtm_malloc
   module will expand without all the asserts and special checking making for
   a compact and efficient storage manager. The gtm_malloc_dbg module will
   expand AS IF DEBUG had been specified supplying an alternate assert filled
   version of storage mgmnt with several different levels of storage validation
   available.

   If the DEBUG flag is defined (debug or beta build), the gtm_malloc module
   will expand with all debugging information intact and the gtm_malloc_dbg
   module will expand as call backs to the gtm_malloc module since it makes
   little sense to expand the identical module twice.
*/

#  define GTM_MALLOC_BUILD	/* so mdq.h won't include unnecessary and probelmatic trigger checking for gtm_malloc */
#ifndef DEBUG

/* We have a PRO build -- generate a full debug version with debug versions of
   our global names */

#  define gtmSmInit gtmSmInit_dbg
#  define gtm_malloc_main gtm_malloc_dbg
#  define gtm_free_main gtm_free_dbg
#  define findStorElem findStorElem_dbg
#  define processDeferredFrees processDeferredFrees_dbg
#  define release_unused_storage release_unused_storage_dbg
#  define raise_gtmmemory_error raise_gtmmemory_error_dbg
#  define gtm_bestfitsize gtm_bestfitsize_dbg
#  define DEBUG
#  define PRO_BUILD
#  define GTM_MALLOC_DEBUG
#  include "caller_id.h"
#  include "gtm_malloc_src.h"

#else

/* We have a DEBUG build -- Nobody should call gtm_malloc_dbg directly */
#  include "mdef.h"
#  include "gtm_malloc.h"
/* Include some defs for these rtns to keep the compiler quiet for this routine.
   Nobody should be calling these directly so we don't want them where they can
   get included anywhere else.

   Note the real versions of these routines are defined and only used/callable from
   the gtm_malloc_src.h include so when we define them here for completeness in a dbg
   build, we change the return signature to not return anything (saves us from having
   to put a "return" after the assertpro). These are just "catchalls" in case the
   expansion functioned incorrectly.
*/
void gtm_malloc_dbg(size_t size, int stack_level);
void gtm_free_dbg(void *addr, int stack_level);

void gtm_malloc_dbg(size_t size, int stack_level)
{
	assertpro(FALSE && "gtm_malloc_dbg called directly");
}

void gtm_free_dbg(void *addr, int stack_level)
{
	assertpro(FALSE && "gtm_free_dbg called directly");
}
#endif
#undef GTM_MALLOC_BUILD
