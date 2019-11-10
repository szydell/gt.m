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

#include <stddef.h>	/* for offsetof macro */
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "sys/mman.h"

#include <rtnhdr.h>
#include "fix_pages.h"
#include "zbreak.h"
#include "private_code_copy.h"
#include "cmd_qlf.h"
#include "urx.h"
#include "stringpool.h"
#include "gtm_text_alloc.h"
#include "zr_unlink_rtn.h"
#include "zroutines.h"
#include "incr_link.h"

/* Routine to unlink given old flavor of routine (as much of it as we are able).
 *
 * Parameters:
 *
 *   old_rhead - Old routine header to be processed
 *   free_all  - Currently only true by gtm_unlink_all. If TRUE, releases routine in its entirety - nothing stays behind.
 *               If FALSE, only the "releasable" sections as noted in comments of obj_code.c are released.
 *
 * Note when a "normal" routine is re-linked, not all of it is removed. Both the original routine header and the label
 * table are retained since addresses to those things exist in the linkage tables of other routines. But in the case of
 * triggers or "unlink-all" (ZGOTO 0:entryref), the entire routine is removed.
 */
void zr_unlink_rtn(rhdtyp *old_rhead, boolean_t free_all)
{
	unsigned char	*map_adr;	/* If code section is mmapped from shared .o file */
	textElem        *telem;
	rhdtyp		*rhdr, *next_rhdr;

	zr_remove_zbrks(old_rhead, NOBREAKMSG);	/* Remove breakpoints (now inactive) */
	/* If source has been read in for old routine, free space. On VMS, source is associated with a routine name
	 * table entry. On UNIX, source is associated with a routine header, and we may have different sources for
	 * different linked versions of the same routine name.
	 */
	free_src_tbl(old_rhead);
#	ifdef USHBIN_SUPPORTED
	urx_remove(old_rhead);			/* Remove all unresolved entries for this routine */
	/* We are about to release program areas containing literal text that could be pointed to by
	 * local variable mvals that are being kept so migrate program literals to the stringpool.
	 * Migrate text literals pointing into text area we are about to throw away into the stringpool.
	 * We also can release the read-only releasable segment as it is no longer needed.
	 * NOTE: go ahead and move these even if old routine is in a shared library and we aren't closing
	 * the shared library. Anything we move but isn't actually needed will be weeded out in the next
	 * stringpool garbage collection.
	 */
	if (0 < old_rhead->literal_text_len)
		stp_move((char *)old_rhead->literal_text_adr,
			 (char *)(old_rhead->literal_text_adr + old_rhead->literal_text_len));
	if (NULL == old_rhead->shlib_handle)
        {	/* Object is not resident in a shared library */
		if (!free_all)
			zlmov_lnames(old_rhead); 	/* Copy the label names from literal pool to malloc'd area */
		if (0 < old_rhead->shared_len)
		{	/* Object is being shared via mmap() */
			assert(NULL != old_rhead->shared_ptext_adr);
			assert(old_rhead->shared_ptext_adr == old_rhead->ptext_adr);
			map_adr = old_rhead->shared_ptext_adr  - SIZEOF(rhdtyp)- NATIVE_HDR_LEN;
			munmap(map_adr, old_rhead->shared_len);

		} else
		{	/* Process private linked object */
			GTM_TEXT_FREE(old_rhead->ptext_adr);
		}
		/* Reset the routine header pointers to the sections we just freed up.
		 * NOTE: literal_text_adr shouldn't be reset as it points to the label area malloc'd
		 * in zlmov_lnames().
		 */
		old_rhead->ptext_adr = old_rhead->ptext_end_adr = NULL;
		old_rhead->lnrtab_adr = NULL;
	}
	free(RW_REL_START_ADR(old_rhead));	/* Release the read-write releasable segments */
	old_rhead->literal_adr = NULL;
	old_rhead->vartab_adr = NULL;
	free(old_rhead->linkage_adr);		/* Release the old linkage section */
	old_rhead->linkage_adr = NULL;
#	else
	if (!old_rhead->old_rhead_ptr)
	{	/* On VMS, this makes the routine "malleable" and on UNIX tiz a stub */
	        fix_pages((unsigned char *)old_rhead, (unsigned char *)LNRTAB_ADR(old_rhead)
			  + (SIZEOF(lnr_tabent) * old_rhead->lnrtab_len));
	}
#	endif
	if (free_all)
	{	/* We are not keeping any parts of this routine (generally used for triggers and for gtm_unlink_all()) */
#		ifdef USHBIN_SUPPORTED
		free(old_rhead->labtab_adr);	/* Usually non-releasable but not in this case */
		if (old_rhead->lbltext_ptr)
			free(old_rhead->lbltext_ptr);	/* Get rid of any label text hangers-on */
		if (old_rhead->shared_len)		/* If this is a shared object (not library), drop rtn name/path text */
			free(old_rhead->src_full_name.addr);
		/* Run the chain of old (replaced) versions freeing them also if they exist*/
		for (rhdr = OLD_RHEAD_ADR(old_rhead); NULL != rhdr; rhdr = next_rhdr)
		{
			next_rhdr = rhdr->old_rhead_adr;
			if (rhdr->lbltext_ptr)
				free(rhdr->lbltext_ptr);	/* Get rid of any label text hangers-on */
			if (rhdr->shared_len)			/* If this is a shared object, drop rtn name/path text */
				free(rhdr->src_full_name.addr);
			free(rhdr->labtab_adr);			/* Free dangling label table */
			free(rhdr);
		}
		free(old_rhead);
#		elif !defined(VMS)
#		  if (!defined(__linux__) && !defined(__CYGWIN__)) || !defined(__i386) || !defined(COMP_GTA)
#		   error Unsupported NON-USHBIN platform
#		  endif
		/* For a non-shared binary platform we need to get an approximate addr range for stp_move. This is not
		 * done when a routine is replaced on these platforms but in this case we need to since the routines are
		 * going away which will cause problems with any local variables or environment varspointing to these
		 * literals.
		 *
		 * In this format, the only platform we support currently is Linux-x86 (i386) which uses GTM_TEXT_ALLOC()
		 * to allocate special storage for it to put executable code in. We can access the storage header for
		 * this storage and find out how big it is and use that information to give stp_move a good range since
		 * the literal segment occurs right at the end of allocated storage (for which there is no pointer
		 * in the fileheader). (Note we allow CYGWIN in here too but it has not been tested at this time)
		 */
		telem = (textElem *)((char *)old_rhead - offsetof(textElem, userStorage));
		assert(TextAllocated == telem->state);
		stp_move((char *)LNRTAB_ADR(old_rhead) + (old_rhead->lnrtab_len * SIZEOF(lnr_tabent)),
			 (char *)old_rhead + telem->realLen);
		/* Run the chain of old (replaced) versions freeing them first */
		for (rhdr = OLD_RHEAD_ADR(old_rhead); old_rhead != rhdr; rhdr = next_rhdr)
		{
			next_rhdr = (rhdtyp *)rhdr->old_rhead_ptr;
			GTM_TEXT_FREE(rhdr);
		}
		GTM_TEXT_FREE(old_rhead);
#		endif
	}
}
