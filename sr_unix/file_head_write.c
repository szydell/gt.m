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

#include "mdef.h"

#include <errno.h>
#include "gtm_unistd.h"
#include "gtm_fcntl.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "db_header_conversion.h"
#include "filestruct.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "gtmio.h"
#include "iosp.h"
#include "eintr_wrappers.h"
#include "file_head_write.h"
#include "gtmmsg.h"
#include "jnl.h"
#include "anticipatory_freeze.h"
#ifdef __MVS__
#include "gtm_zos_io.h"
#endif

error_def(ERR_DBFILOPERR);
error_def(ERR_DBNOTGDS);
error_def(ERR_TEXT);

/* This is a plain way to write file header to database.
 * User needs to take care of concurrency issue etc.
 * Parameters :
 *	fn : full name of a database file.
 *	header: Pointer to database file header structure (may not be in shared memory)
 *	len: length of header to write (should be either SGMNT_HDR_LEN or SIZEOF_FILE_HDR(header))
 */
boolean_t file_head_write(char *fn, sgmnt_data_ptr_t header, int4 len)
{
	int		save_errno, fd;
	ZOS_ONLY(int	realfiletag;)

	assert(SGMNT_HDR_LEN == len || SIZEOF_FILE_HDR(header) == len);
	OPENFILE(fn, O_RDWR, fd); /* udi not available so OPENFILE_DB not used */
	if (FD_INVALID == fd)
	{
		save_errno = errno;
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBFILOPERR, 2, LEN_AND_STR(fn), save_errno);
		return FALSE;
	}
#	ifdef __MVS__
	if (-1 == gtm_zos_tag_to_policy(fd, TAG_BINARY, &realfiletag))
		TAG_POLICY_GTM_PUTMSG(fn, errno, realfiletag, TAG_BINARY);
#	endif
	if (0 == memcmp(header->label, V6_GDS_LABEL, GDS_LABEL_SZ - 1))
		db_header_dwnconv(header);
	DB_LSEEKWRITE(NULL, ((unix_db_info *)NULL), NULL, fd, 0, header, len, save_errno);
	if (0 != save_errno)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBFILOPERR, 2, LEN_AND_STR(fn), save_errno);
		return FALSE;
	}
	CLOSEFILE_RESET(fd, save_errno);	/* resets "fd" to FD_INVALID */
	if (0 != save_errno)
	{
		gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_DBFILOPERR, 2, LEN_AND_STR(fn), save_errno);
		return FALSE;
	}
	return TRUE;
}
