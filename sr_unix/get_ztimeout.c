/****************************************************************
 *								*
 * Copyright (c) 2018-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "stringpool.h"
#include "time.h"
#include "gt_timer.h"
#include "ztimeout_routines.h"
#include "deferred_events.h"
#include "min_max.h"
#include "compiler.h"

GBLREF int		process_exiting;
GBLREF spdesc           stringpool;

/*				"%ld.%ld"	       ":" \0 */
#define ZTIMEOUTSTRLEN 	((2 * MAX_DIGITS_IN_INT8) + 1 + 1 + 1)

int get_ztimeout(mval *result)
{
	unsigned char	*cp;
	char		*ztimeout_vector_ptr;
	int		ztimeout_vector_len;
	char		full_ztimeout[ZTIMEOUTSTRLEN];
	int		req_len, time_len;
	long int 	ms = 0;
	ABS_TIME 	cur_time;
	DCL_THREADGBL_ACCESS;

 	SETUP_THREADGBL_ACCESS;
	ztimeout_vector_ptr = (TREF(dollar_ztimeout)).ztimeout_vector.str.addr;
	ztimeout_vector_len = (TREF(dollar_ztimeout)).ztimeout_vector.str.len;
	if (ztimeout_vector_len) ztimeout_vector_len--;
	if ((((TREF(dollar_ztimeout)).ztimeout_seconds.m[1] / MV_BIAS) == -1)
			&& (NULL == ztimeout_vector_ptr))
		time_len = SNPRINTF(full_ztimeout, ZTIMEOUTSTRLEN, "%d", ((TREF(dollar_ztimeout)).ztimeout_seconds.m[1] / MV_BIAS));
	else
	{
		sys_get_curr_time(&cur_time);
		cur_time = sub_abs_time(&(TREF(dollar_ztimeout)).end_time, &cur_time);
		if (0 <= cur_time.at_sec)
		{
			DBGDFRDEVNT((stderr,"cur_time.at_usec is: %d\n", cur_time.at_usec));
			ms = DIVIDE_ROUND_DOWN(cur_time.at_usec, MICROSECS_IN_MSEC);
			time_len = SNPRINTF(full_ztimeout, ZTIMEOUTSTRLEN, ((NULL == ztimeout_vector_ptr)? "%ld.%ld" : "%ld.%ld:"),
					cur_time.at_sec, ms);
		} else
			time_len = SNPRINTF(full_ztimeout, ZTIMEOUTSTRLEN, ((NULL == ztimeout_vector_ptr)? "%ld" : "%ld:"), ms);
	}
	assert((0 < time_len) && (time_len <= ZTIMEOUTSTRLEN));
	assert(((0 == ztimeout_vector_len) && (NULL == ztimeout_vector_ptr))
			|| ((0 < ztimeout_vector_len) && (NULL != ztimeout_vector_ptr)));
	req_len = time_len + ztimeout_vector_len;
	if ((process_exiting) && !(IS_STP_SPACE_AVAILABLE_PRO(req_len)))
		return -1;	/* Process is exiting, avoid adding to the memory pressure */
	ENSURE_STP_FREE_SPACE(req_len);
	cp = stringpool.free;
	stringpool.free += req_len;
	result->str.addr = (char *)cp;
	result->str.len = req_len;
	result->mvtype = MV_STR;
	memcpy(cp, full_ztimeout, time_len);
	if ((0 < ztimeout_vector_len) && (NULL != ztimeout_vector_ptr))
		memcpy(cp + time_len, ztimeout_vector_ptr, ztimeout_vector_len);
	assert(IS_AT_END_OF_STRINGPOOL(cp, req_len));
        return 0;
}
