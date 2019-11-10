;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;								;
; Copyright (c) 2015 Fidelity National Information		;
; Services, Inc. and/or its subsidiaries. All rights reserved.	;
;								;
;	This source code contains the intellectual property	;
;	of its copyright holder(s), and is made available	;
;	under a license.  If you do not know the terms of	;
;	the license, please stop and do not read further.	;
;								;
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; This is called by generate_help.csh to put offset, length, type and format info into the gtmhelp database
GTMDEFINEDTYPESTODB
	do Init^GTMDefinedTypesInit
	merge ^gtmtypes=gtmtypes
	merge ^gtmtypfldindx=gtmtypfldindx
	merge ^gtmstructs=gtmstructs
	merge ^gtmunions=gtmunions
	quit
