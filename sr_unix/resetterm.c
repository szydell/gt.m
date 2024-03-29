/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>

#include "gtm_termios.h"
#include "gtm_signal.h"	/* for SIGPROCMASK used inside Tcsetattr */

#include "io.h"
#include "iottdef.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "setterm.h"
#include "gtm_isanlp.h"

error_def(ERR_TCSETATTR);

void  resetterm(io_desc *iod)
{
	int		status;
	int		save_errno;
	struct termios 	t;
	d_tt_struct	*ttptr;

	ttptr =(d_tt_struct *) iod->dev_sp;
	if (ttptr->ttio_struct)
	{
	        t = *ttptr->ttio_struct;
		Tcsetattr(ttptr->fildes, TCSANOW, &t, status, save_errno);
		if (status != 0)
		{
			if (gtm_isanlp(ttptr->fildes) == 0)
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_TCSETATTR, 1, ttptr->fildes, save_errno);
		}
	}
	return;
}
