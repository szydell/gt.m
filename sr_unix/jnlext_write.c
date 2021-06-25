/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information	*
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
#include "gtm_stdio.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "buddy_list.h"
#include "jnl.h"
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "muprec.h"
#include "io.h"
#include "io_params.h"
#include "op.h"
#include "gtm_multi_proc.h"

GBLREF	io_pair		io_curr_device;
GBLREF	mstr		sys_output;
GBLREF	mur_gbls_t	murgbl;

static readonly unsigned char open_params_list[] =
{
	(unsigned char)iop_stream,
	(unsigned char)iop_nowrap,
	(unsigned char)iop_eol
};

/* Due to historic reasons, *buffer must be terminated with '\\' character (or something else). (length-1) of the buffer will be
 * written to the file or STDOUT which means '\\' will be ignored here.
 */
void jnlext_write(jnl_ctl_list *jctl, jnl_record *rec, enum broken_type recstat, char *buffer, int length)
{
	mval			op_val, op_pars;
	io_pair			dev_in_use;
	fi_type			*file_info;
	reg_ctl_list		*rctl;

	rctl = jctl->reg_ctl;
	assert(buffer[length - 1] == '\\');
	dev_in_use = io_curr_device;
	op_val.mvtype = MV_STR;
	file_info = rctl->file_info[recstat];
	if (NULL != file_info)
	{	/* Output to a file */
		op_val.str.addr = (char *)file_info->fn;
		op_val.str.len = file_info->fn_len;
	} else
	{	/* Output to stdout */
		assert(1 == murgbl.reg_total);
		assert(sys_output.len > 0);
		assert(sys_output.addr);
		op_val.str = sys_output;
	}
	op_pars.str.len = SIZEOF(open_params_list);
	op_pars.str.addr = (char *)open_params_list;
	op_pars.mvtype = MV_STR;
	op_use(&op_val, &op_pars);
	op_val.mvtype = MV_STR;
	op_val.str.addr = (char *)buffer;
	op_val.str.len = length - 1;
	op_write(&op_val);
	op_wteol(1);
	io_curr_device = dev_in_use;
	if (1 < murgbl.reg_total)
	{	/* We need to do merge-sort of extract files created for multiple regions later in "mur_cre_merge_extfile".
		 * Prepare for it below.
		 */
		jnlext_merge_sort_prepare(jctl, rec, recstat, length);
	}
}
