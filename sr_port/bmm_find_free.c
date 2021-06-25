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
#include "gdsroot.h"
#include "gdsblk.h"
#include "gdsbml.h"
#include "min_max.h"
#include "gtm_ffs.h"
#include "bmm_find_free.h"

#define MAX_FFS_SIZE 32

/* Returns the location of the first set bit in the field.	*/
/* The search starts at the hint and wraps if necessary.	*/

block_id bmm_find_free(block_id hint, uchar_ptr_t base_addr, block_id total_bits)
{
	block_id	start, top, width, answer;

	if (hint >= total_bits)
		hint = 0;
	for (start = hint, top = total_bits;  top;  start = 0, top = hint, hint = 0)
	{	/* one or two passes through outer loop; second is a wrap to the beginning */
		for (width = MIN(top, ROUND_DOWN2(start, MAX_FFS_SIZE) + MAX_FFS_SIZE) - start;  width > 0;
			start += width, width = MIN(top - start, MAX_FFS_SIZE))
		{
			answer = gtm_ffs(start, base_addr, width);
			if (NO_FREE_SPACE != answer)
				return answer;
		}
	}
	assert(NO_FREE_SPACE == answer);
	return NO_FREE_SPACE;
}
