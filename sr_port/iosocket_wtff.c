/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* iosocket_wtff.c */

#include "mdef.h"
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "io.h"
#include "gt_timer.h"
#include "iosocketdef.h"

GBLREF io_pair		io_curr_device;

error_def(ERR_CURRSOCKOFR);
error_def(ERR_NOSOCKETINDEV);

void iosocket_wtff(void)
{
	io_desc		*iod;
	socket_struct	*socketptr;
	d_socket_struct	*dsocketptr;

	iod = io_curr_device.out;
	assert(gtmsocket == iod->type);
	iod->esc_state = START;
	dsocketptr = (d_socket_struct *)iod->dev_sp;
	if (0 >= dsocketptr->n_socket)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOSOCKETINDEV);
		return;
	}
	if (dsocketptr->current_socket >= dsocketptr->n_socket)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CURRSOCKOFR, 2, dsocketptr->current_socket, dsocketptr->n_socket);
		return;
	}
	socketptr = dsocketptr->socket[dsocketptr->current_socket];
	if (socketptr->zff.len)
		iosocket_write_real(&socketptr->zff, FALSE);
	iosocket_flush(iod);
	iod->dollar.x = 0;
	iod->dollar.y = 0;
	return;
}
