/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
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
#include "gtm_stdlib.h"
#include "gtm_unistd.h"

#include <errno.h>

#include <rtnhdr.h>
#include "startup.h"
#include "gtm_startup.h"
#include "stack_frame.h"
#include "job_addr.h"
#include "compiler.h"
#include "indir_enum.h"
#include "error.h"
#include "util.h"
#include "cli.h"
#include "job.h"
#include "gcall.h"
#include "jobchild_init.h"
#include "lv_val.h"	/* needed for "callg.h" */
#include "callg.h"
#include "invocation_mode.h"
#include "gtmci.h"
#include "send_msg.h"
#include "have_crit.h"

#define FILE_NAME_SIZE	255

#if defined(__x86_64__)
	extern void opp_ciret();
#endif

LITDEF char 		interactive_mode_buf[] = "INTERACTIVE";
LITDEF char 		other_mode_buf[] = "OTHER";

GBLREF stack_frame	*frame_pointer;
GBLREF uint4		process_id;

error_def(ERR_RUNPARAMERR);
error_def(ERR_TEXT);
error_def(ERR_SYSCALL);
error_def(ERR_JOBSTARTCMDFAIL);
error_def(ERR_JOBLABOFF);

CONDITION_HANDLER(job_init_ch)
{
	START_CH(TRUE);
	PRN_ERROR;
	NEXTCH;
}

/* Child process test and initialization. If this copy of GTM is a child process, then initialize the child. */
void jobchild_init(void)
{
	job_params_type	jparms;
	unsigned char	*transfer_addr;		/* Transfer data */
	rhdtyp		*base_addr;
	unsigned short	i, arg_len;
	char		run_file_name[FILE_NAME_SIZE + 2], *c;
	gcall_args	job_arglist;
	mval		job_args[MAX_ACTUALS];
	mstr		routine, label;
	int		offset;
	int		status;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	ESTABLISH(job_init_ch);
	/* Check if environment variable ppid - job parent pid exists. If it does not, we are a regular
	 * gtm process; else, we are a child process of a job command.
	 */
	if ((c = GETENV(CHILD_FLAG_ENV)) && strlen(c))
	{	/* We are a Jobbed process Get Job parameters and set up environment to run the Job command. */
		/* read parameters into parameter structure  - references CHILD_FLAG_ENV */
		ojchildparms(&jparms, &job_arglist, job_args);
		/* Clear the environment variable so that subsequent child mumps processes can start normal initialization. */
		PUTENV(status, CLEAR_CHILD_FLAG_ENV);
		if (status)
		{
			util_out_print("Unable to clear gtmj0 process !UL exiting.", TRUE, process_id);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) errno);
		}
		/* Execute the command to be run before executing the actual M routine */
		if (jparms.params.startup.len)
		{
			jparms.params.startup.buffer[jparms.params.startup.len] = '\0';
			status = SYSTEM(jparms.params.startup.buffer);
			if (-1 == status)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_JOBSTARTCMDFAIL, 0, errno);
			else if (0 != status)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(2) ERR_JOBSTARTCMDFAIL, 0);
		}
		MSTR_DEF(routine_mstr, jparms.params.routine.len, jparms.params.routine.buffer);
		MSTR_DEF(label_mstr, jparms.params.label.len, jparms.params.label.buffer);
		if (!job_addr(&routine_mstr, &label_mstr, jparms.params.offset, (char **)&base_addr, (char **)&transfer_addr))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBLABOFF);
		/* Set process priority */
		if (jparms.params.baspri)
		{	/* send message to system log if nice fails */
			if (-1 == nice((int)jparms.params.baspri))
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("nice"), CALLFROM, errno);
		}
		/* Set up $ZMODE to "OTHER" */
		(TREF(dollar_zmode)).mvtype = MV_STR;
		(TREF(dollar_zmode)).str.addr = (char *)other_mode_buf;
		(TREF(dollar_zmode)).str.len = SIZEOF(other_mode_buf) -1;
	} else
	{	/* If we are not a child, setup a dummy mumps routine */
		if (MUMPS_RUN == invocation_mode)
		{
			arg_len = FILE_NAME_SIZE;
			if (!cli_get_str("INFILE", run_file_name, &arg_len))
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_RUNPARAMERR);
			lref_parse((uchar_ptr_t)run_file_name, &routine, &label, &offset);
			if (!job_addr(&routine, &label, offset, (char **)&base_addr, (char **)&transfer_addr))
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_JOBLABOFF);
		} else if (MUMPS_CALLIN & invocation_mode) /* call-in mode */
		{
			base_addr = make_cimode();
			transfer_addr = PTEXT_ADR(base_addr);
		} else /* direct mode */
		{
			base_addr = make_dmode();
			transfer_addr = PTEXT_ADR(base_addr);
		}
		job_arglist.callargs = 0;
		/* Set up $ZMODE to "INTERACTIVE" */
		(TREF(dollar_zmode)).mvtype = MV_STR;
		(TREF(dollar_zmode)).str.addr = (char *)interactive_mode_buf;
		(TREF(dollar_zmode)).str.len = SIZEOF(interactive_mode_buf) -1;
	}
	gtm_init_env(base_addr, transfer_addr);
	if (MUMPS_CALLIN & invocation_mode)
	{
#if defined(__x86_64__)
		SET_CI_ENV(opp_ciret);
#else
		SET_CI_ENV(ci_ret_code_exit);
#endif
	}
	if (job_arglist.callargs)
		callg((INTPTR_T (*)(intszofptr_t cnt, ...))push_parm, (gparam_list *)&job_arglist);
	REVERT;
}
