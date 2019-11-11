/****************************************************************
 *								*
 * Copyright (c) 2003-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_fcntl.h"
#include "gtm_time.h"
#include <errno.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cli.h"
#include "iosp.h"
#include "jnl.h"
#include "gtmimagename.h"
#include "gtm_rename.h"
#include "send_msg.h"
#include "gtmmsg.h"
#include "gtm_file_stat.h"

error_def(ERR_FILERENAME);
error_def(ERR_RENAMEFAIL);

/* --------------------------------------------------------------------------------
	This function  renames a file, if exists. Otherwise do nothing.
  --------------------------------------------------------------------------------- */
int rename_file_if_exists(char *org_fn, int org_fn_len, char *rename_fn, int *rename_fn_len, uint4 *ustatus)
{
	mstr 		orgfile;
	int		status;
	jnl_tm_t	now;

	orgfile.addr = org_fn;
	orgfile.len = org_fn_len;
	if (FILE_NOT_FOUND == (status = gtm_file_stat(&orgfile, NULL, NULL, FALSE, ustatus)))
		return RENAME_NOT_REQD;
	else if (FILE_STAT_ERROR == status)
	{
		assert(SS_NORMAL != *ustatus);
		return RENAME_FAILED;
	}
	/* File is present in the system */
	assert(0 <= (MAX_FN_LEN - org_fn_len));
	JNL_SHORT_TIME(now);
	if (SS_NORMAL != (status = prepare_unique_name(org_fn, org_fn_len, "", "", rename_fn, rename_fn_len, now, ustatus)))
	{	/* "prepare_unique_name" would not have set "ustatus" to the error code. So set it here and return */
		assert(SS_NORMAL == *ustatus);
		*ustatus = status;
		assert(SS_NORMAL != *ustatus);
		return RENAME_FAILED;
	}
	assert(0 == rename_fn[*rename_fn_len]);
	if (SS_NORMAL != (status = gtm_rename(org_fn, org_fn_len, rename_fn, *rename_fn_len, ustatus)))
	{
		*ustatus = status;
		assert(SS_NORMAL != *ustatus);
		if (IS_GTM_IMAGE)
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_RENAMEFAIL, 4, org_fn_len, org_fn, *rename_fn_len, rename_fn,
				status, 0, *ustatus);
		else
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT1(8) ERR_RENAMEFAIL, 4, org_fn_len, org_fn, *rename_fn_len, rename_fn,
				status, PUT_SYS_ERRNO(*ustatus));
		return RENAME_FAILED;
	}
	if (IS_GTM_IMAGE)
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT (6) ERR_FILERENAME, 4, org_fn_len, org_fn, *rename_fn_len, rename_fn);
	else
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT (6) ERR_FILERENAME, 4, org_fn_len, org_fn, *rename_fn_len, rename_fn);
	return RENAME_SUCCESS;
}
