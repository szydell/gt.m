/****************************************************************
 *								*
 * Copyright (c) 2013-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTMDBGFLAGS_H_INCLUDED
#define GTMDBGFLAGS_H_INCLUDED

#ifdef GTMDBGFLAGS_ENABLED
# define GTMDBGFLAGS_MASK_SET(MASK)	(TREF(gtmdbgflags) & MASK)
# define GTMDBGFLAGS_ONLY(MASK, ...)												\
{																\
	DCL_THREADGBL_ACCESS;													\
																\
	SETUP_THREADGBL_ACCESS;													\
	if (GTMDBGFLAGS_MASK_SET(MASK))												\
	{															\
		(TREF(gtmdbgflags_freq_cntr))++;										\
		if (TREF(gtmdbgflags_freq) == TREF(gtmdbgflags_freq_cntr))							\
		{														\
			__VA_ARGS__;												\
			TREF(gtmdbgflags_freq_cntr) = 0;									\
		}														\
	}															\
}
# define GTMDBGFLAGS_NOFREQ_ONLY(MASK, ...)											\
{																\
	DCL_THREADGBL_ACCESS;													\
																\
	SETUP_THREADGBL_ACCESS;													\
	if (GTMDBGFLAGS_MASK_SET(MASK))												\
	{															\
		__VA_ARGS__;													\
	}															\
}
# define GTMSOURCE_FORCE_READ_FILE_MODE		0x00000001
#else
# define GTMDBGFLAGS_MASK_SET(MASK)		FALSE
# define GTMDBGFLAGS_ONLY(MASK, FREQ, ...)
# define GTMDBGFLAGS_NOFREQ_ONLY(MASK, ...)
#endif

#endif
