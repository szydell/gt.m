/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef UPDPROC_INCLUDED
#define UPDPROC_INCLUDED

#define SKIP_REC 5

enum upd_bad_trans_type
{
	upd_good_record = 0,
	upd_bad_forwptr,
	upd_bad_backptr,
	upd_rec_not_replicated,
	upd_bad_jnl_seqno,
	upd_bad_mname_size,
	upd_bad_key_size,
	upd_bad_val_size,
	upd_fence_bad_t_num,
	upd_nofence_bad_tupd_num,
	upd_bad_histinfo_len,
	upd_bad_histinfo_start_seqno1,
	upd_bad_histinfo_start_seqno2,
	upd_fence_bad_ztworm_t_num,
	upd_fence_bad_lgtrig_t_num,
	upd_bad_key
};

#define	UPD_GV_BIND_NAME(GD_HEADER, GVNAME, GVNH_REG)	GV_BIND_NAME_AND_ROOT_SEARCH(GD_HEADER, &GVNAME, GVNH_REG)

#ifdef GTM_TRIGGER
#define UPD_GV_BIND_NAME_APPROPRIATE(GD_HEADER, GVNAME, KEY, KEYLEN, GVNH_REG)				\
{													\
	char			*tr_ptr;								\
	mname_entry		gvname1;								\
	int			tr_len;									\
	sgmnt_addrs		*csa;									\
													\
	GBLREF boolean_t	dollar_ztrigger_invoked;						\
													\
	if (IS_MNAME_HASHT_GBLNAME(GVNAME.var_name))							\
	{	/* gbl is ^#t. In this case, do special processing. Look at the first subscript and	\
		 * bind to the region mapped to by that global name (not ^#t). Also since V62 will	\
		 * never receive ^#t records from V62 source (it will receive TLGTRIG/ULGTRIG logical	\
		 * journal records only) assert accordingly.						\
		 */											\
		assert(V24_JNL_VER > gtmrecv_local->remote_side.jnl_ver);				\
		tr_ptr = KEY;			/* Skip to the first subscript */			\
		tr_len = STRLEN(KEY);		/* Only want length to first 0, not entire length */	\
		assert(tr_len < KEYLEN);	/* If ^#t, there has to be a subscript */		\
		tr_ptr += tr_len;									\
		assert((KEY_DELIMITER == *tr_ptr) && ((char)STR_SUB_PREFIX == *(tr_ptr + 1)));		\
		tr_ptr += 2;				/* Skip the 0x00 and 0xFF */			\
		assert((HASHT_GBL_CHAR1 == *tr_ptr) || ('%' == *tr_ptr) || (ISALPHA_ASCII(*tr_ptr)));	\
		if (HASHT_GBL_CHAR1 != *tr_ptr)								\
		{											\
			gvname1.var_name.addr = tr_ptr;							\
			gvname1.var_name.len = STRLEN(tr_ptr);						\
			COMPUTE_HASH_MNAME(&gvname1);							\
			GV_BIND_NAME_ONLY(GD_HEADER, &gvname1, GVNH_REG);				\
			csa = cs_addrs;									\
			SET_GVTARGET_TO_HASHT_GBL(csa);							\
			dollar_ztrigger_invoked = TRUE;							\
			csa->incr_db_trigger_cycle = TRUE;						\
			csa->db_dztrigger_cycle++;							\
		} else											\
		{											\
			SWITCH_TO_DEFAULT_REGION;							\
			/* Special case for ^#t. Set GVNH_REG to NULL. Caller needs to check this */	\
			GVNH_REG = NULL;								\
		}											\
		INITIAL_HASHT_ROOT_SEARCH_IF_NEEDED;							\
	} else												\
		UPD_GV_BIND_NAME(GD_HEADER, GVNAME, GVNH_REG);						\
}
#else
#define UPD_GV_BIND_NAME_APPROPRIATE(GD_HEADER, GVNAME, KEY, KEYLEN, GVNH_REG)	UPD_GV_BIND_NAME(GD_HEADER, GVNAME, GVNH_REG)
#endif

int		updproc(void);
void		updproc_actions(gld_dbname_list *gld_db_files);
int		updproc_init(gld_dbname_list **gld_db_files , seq_num *start_jnl_seqno);
int		upd_log_init(recvpool_user);
boolean_t	updproc_open_files(gld_dbname_list **gld_db_files, seq_num *start_jnl_seqno);
void		updproc_stop(boolean_t exit);
void		updproc_sigstop(void);
void		updproc_end(void);
void		updhelper_init(recvpool_user);
int 		updhelper_reader(void);
boolean_t	updproc_preread(void);
void		updhelper_reader_end(void);
void		updhelper_reader_sigstop(void);
int		updhelper_writer(void);
void		updhelper_writer_end(void);
void		updhelper_writer_sigstop(void);

#endif /* UPDPROC_INCLUDED */
