/****************************************************************
 *								*
 * Copyright (c) 2011-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"
#include "gtm_ipc.h"
#include <sys/sem.h>
#include <errno.h>
#include "gtm_c_stack_trace.h"
#include "gtm_c_stack_trace_semop.h"
#include "semwt2long_handler.h"
#include "wbox_test_init.h"
#include "gt_timer.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"

GBLREF uint4            process_id;
#ifdef DEBUG
GBLREF	gd_region	*gv_cur_region;
GBLREF  sgmnt_addrs     *cs_addrs;
#endif
int try_semop_get_c_stack(int semid, struct sembuf sops[], int nsops)
{
	int                     stuckcnt, loopcount;
	int                     semop_pid, save_errno;
	int                     last_sem_trace, rc;
#	ifdef DEBUG
	node_local_ptr_t        cnl = NULL;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	ifdef DEBUG
	if ((NULL != gv_cur_region) && gv_cur_region->open && (NULL != cs_addrs))
		cnl = cs_addrs->nl;
#	endif
	stuckcnt = 0;
	loopcount = -1;
	save_errno = 0;
	last_sem_trace = -1;
	TREF(semwait2long) = TRUE;
	do
	{
		if (TREF(semwait2long))
		{	/* 1st time or semwait2long timer pop mean we need to (re)start timer */
			TREF(semwait2long) = FALSE;
			start_timer((TID)semwt2long_handler,(int4)MAX_SEM_WAIT_TIME, semwt2long_handler, 0, NULL);
		}
		rc = semop(semid, sops, nsops);
		if (-1 == rc)
		{	/* didn't work */
			save_errno = errno;
			if ((EINTR == save_errno) && TREF(semwait2long))
			{	/* semwait2long timer popped, get C-stack trace */
				stuckcnt++;
				last_sem_trace = -1;
				for (loopcount = 0; loopcount < nsops; loopcount++)
				{	/* for does 1 pass of each semop in set */
					if ((last_sem_trace != sops[loopcount].sem_num) && (0 == sops[loopcount].sem_op))
					{	/* Do not take trace of the same process again, in a point of time */
						last_sem_trace = sops[loopcount].sem_num;
						semop_pid = semctl(semid, sops[loopcount].sem_num, GETPID);
						if (-1 == semop_pid)
						{	/* no owner to trace leave the for loop with the errno from semctl */
							save_errno = errno;
							rc = -1;
							break;
						}
						if (semop_pid != process_id)
						{	/* not good to try tracing own process */
							GET_C_STACK_FROM_SCRIPT("SEMOP_INFO", process_id, semop_pid, stuckcnt);
							/* Got stack trace signal the first process to continue */
#							ifdef DEBUG
							if (cnl)
								GTM_WHITE_BOX_TEST(WBTEST_SEMTOOLONG_STACK_TRACE,
									cnl->wbox_test_seq_num, 3);
#							endif
						}
					}
				}	/* for */
			}	/* our timer pop */
		}	/* error return from semop */
	} while ((-1 == rc) && (EINTR == save_errno));	/* keep trying as long as error is an EINTR */
	cancel_timer((TID)semwt2long_handler);
	return (-1 == rc) ? save_errno : 0;
}
