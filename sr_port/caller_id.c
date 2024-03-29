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

/* Stub for caller_id routine called from CRIT_TRACE macro to
   return the return address of our caller allowing CRIT_TRACE
   (used in various semaphore routines) to determine who was
   calling those semaphore routines and for what purpose and
   when. This is a system dependent type of operation and is
   generally implemented in assembly language. This stub is
   so we continue to function on those platforms we have not
   yet implemented this function. */
#include "mdef.h"
#include "caller_id.h"

caddr_t caller_id(unsigned int extra_frames)
{
return (caddr_t)((INTPTR_T)-1);
}

