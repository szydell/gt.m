#################################################################
#								#
# Copyright (c) 2001-2020 Fidelity National Information		#
# Services, Inc. and/or its subsidiaries. All rights reserved.	#
#								#
#	This source code contains the intellectual property	#
#	of its copyright holder(s), and is made available	#
#	under a license.  If you do not know the terms of	#
#	the license, please stop and do not read further.	#
#								#
#################################################################

#
##################################################################
#
#	buildbta.csh - Build bta images.
#
#	Argument:
#		$1 -	Version number or code (i.e., b, d, or p).
#
##################################################################

if ( $1 == "" ) then
	echo "buildbta-E-needp1, Usage: $shell buildbta.csh <version>"
	exit -1
endif

set setactive_parms = ( $1 b ) ; source $gtm_tools/setactive.csh
$gtm_tools/buildbdp.csh $1 bta $gtm_ver/bta
exit $status
