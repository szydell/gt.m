/****************************************************************
 *								*
 *	Copyright 2013, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/types.h>
#include "gtm_stat.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "gtm_limits.h"

#include "cmd_qlf.h"
#include "gtmio.h"
#include "parse_file.h"
#include <rtnhdr.h>
#include "obj_file.h"

GBLREF command_qualifier	cmd_qlf;
GBLREF char			object_file_name[];
GBLREF short			object_name_len;
GBLREF mident			module_name;

#define MKSTEMP_MASK		"XXXXXX"
#define MAX_MKSTEMP_RETRIES	100

error_def(ERR_FILEPARSE);
error_def(ERR_OBJFILERR);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);

/********************************************************************************************************************************
 *
 * While these routines might rightly belong in obj_file.c, since these routines are needed both in sr_unix and in sr_unix_nsb,
 * it was decided to create this module to share routines that are the same across both unix and linux-i386 so the routines only
 * need to exist in one place.
 *
 ********************************************************************************************************************************/

/* Routine to create a temporary object file. This file is created in the directory it is supposed to reside in but is created
 * with a temporary name. When complete, it is renamed to what it was meant to be replacing the previous version.
 *
 * Parameters:
 *
 *   object_fname     - Character array of the object path/name.
 *   object_fname_len - Length of that array in bytes.
 *
 * Return value:
 *   File descriptor for the open object file.
 */
int mk_tmp_object_file(const char *object_fname, int object_fname_len)
{
	int	fdesc, status, umask_creat, umask_orig, retry;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Make sure room in buffer for addition of unique-ifying MKSTEMP_MASK on end of file name */
	if ((object_fname_len + SIZEOF(MKSTEMP_MASK) - 1) > TLEN(tmp_object_file_name))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(9) ERR_OBJFILERR, 2, object_fname_len, object_fname, ERR_TEXT,
			      2, RTS_ERROR_TEXT("Object file name exceeds buffer size"));
	/* The mkstemp() routine is known to bogus-fail for no apparent reason at all especially on AIX 6.1. In the event
	 * this shortcoming plagues other platforms as well, we add a low-cost retry wrapper.
	 */
	retry = MAX_MKSTEMP_RETRIES;
	do
	{
		memcpy(TADR(tmp_object_file_name), object_fname, object_fname_len);
		/* Note memcpy() below purposely includes null terminator */
		memcpy(TADR(tmp_object_file_name) + object_fname_len, MKSTEMP_MASK, SIZEOF(MKSTEMP_MASK));
		fdesc = mkstemp(TADR(tmp_object_file_name));
	} while ((-1 == fdesc) && (EEXIST == errno) && (0 < --retry));
	if (FD_INVALID == fdesc)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_OBJFILERR, 2, object_fname_len, object_fname, errno);
	umask_orig = umask(000);	/* Determine umask (destructive) */
	(void)umask(umask_orig);	/* Reset umask */
	umask_creat = 0666 & ~umask_orig;
	/* Change protections to be those generated by previous versions. In some future version, the permissions may
	 * become tied to the permissions of the source but this works for now.
	 */
	status = FCHMOD(fdesc, umask_creat);
	if (-1 == status)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("fchmod()"), CALLFROM, errno);
        return fdesc;
}

/* Routine to rename the most recent temporary object file (name stored in threadgbl tmp_object_file_name) to the name
 * passed in via parameter.
 *
 * Parameters:
 *   object_fname         - non-temporary name of object file
 *
 * Global input:
 *   tmp_object_file_name - private/unique file created by mkstemp() in routine above.
 */
void rename_tmp_object_file(const char *object_fname)
{
	int	status;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	status = rename(TADR(tmp_object_file_name), object_fname);
	if (-1 == status)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("rename()"), CALLFROM, errno);
}

/* Routine to determine and initialize the fully qualified object name and path.
 *
 * Parameters:
 *   - none
 *
 * Global inputs:
 *   cmd_qlf.object_file - value from -object= option
 *   module_name         - routine name
 * Global outputs:
 *   object_file_name    - full path of object file
 *   object_name_len     - length of full path (not including null terminator)
 */
void init_object_file_name(void)
{
	int		status, rout_len;
	char		obj_name[SIZEOF(mident_fixed) + 5];
	mstr		fstr;
	parse_blk	pblk;

	memset(&pblk, 0, SIZEOF(pblk));
	pblk.buffer = object_file_name;
	pblk.buff_size = MAX_FBUFF;
	fstr.len = (MV_DEFINED(&cmd_qlf.object_file) ? cmd_qlf.object_file.str.len : 0);
	fstr.addr = cmd_qlf.object_file.str.addr;
	rout_len = (int)module_name.len;
	memcpy(&obj_name[0], module_name.addr, rout_len);
	memcpy(&obj_name[rout_len], DOTOBJ, SIZEOF(DOTOBJ));    /* Includes null terminator */
	pblk.def1_size = rout_len + SIZEOF(DOTOBJ) - 1;         /* Length does not include null terminator */
	pblk.def1_buf = obj_name;
	status = parse_file(&fstr, &pblk);
	if (0 == (status & 1))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_FILEPARSE, 2, fstr.len, fstr.addr, status);
	object_name_len = pblk.b_esl;
	object_file_name[object_name_len] = '\0';
}
