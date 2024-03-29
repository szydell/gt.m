/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "mv_stent.h"
#include "error.h"
#include "error_trap.h"
#include "fgncal.h"
#include "gtmci.h"
#include "util.h"

GBLREF  boolean_t		created_core, dont_want_core;
GBLREF  dollar_ecode_type 	dollar_ecode;
GBLREF  int                     mumps_status;
GBLREF	int4			exi_condition;
GBLREF  unsigned char		*fgncal_stack, *msp;

error_def(ERR_ASSERT);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_MEMORY);
error_def(ERR_OUTOFSPACE);
error_def(ERR_STACKOFLOW);

CONDITION_HANDLER(gtmci_ch)
{
	char	src_buf[MAX_ENTRYREF_LEN];
	mstr	src_line;

	START_CH(TRUE);
	if (DUMPABLE)
	{
		gtm_dump();
		TERMINATE;
	}
	src_line.len = 0;
	src_line.addr = &src_buf[0];
	set_zstatus(&src_line, MAX_ENTRYREF_LEN, SIGNAL, NULL, FALSE);
	if (msp < FGNCAL_STACK) /* restore stack to the last marked position */
		fgncal_unwind();
	else TREF(temp_fgncal_stack) = NULL;	/* If fgncal_unwind() didn't run to clear this, we have to */
	if (TREF(comm_filter_init))
		TREF(comm_filter_init) = FALSE;  /* Exiting from filters */
	mumps_status = SIGNAL;
	UNWIND(NULL, NULL);
}
