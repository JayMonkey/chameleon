/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * Copyright (c) 1997 Tobias Weingartner
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Tobias Weingartner.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _CMD_H
#define _CMD_H

/* Includes */
#include "disk.h"
#include "mbr.h"


/* Constants (returned by cmd funs) */
#define CMD_EXIT	0x0000
#define CMD_SAVE	0x0001
#define CMD_CONT	0x0002
#define CMD_CLEAN	0x0003
#define CMD_DIRTY	0x0004


/* Data types */
struct _cmd_table_t;
typedef struct _cmd_t {
	struct _cmd_table_t *table;
	char cmd[10];
	char args[100];
} cmd_t;

typedef struct _cmd_table_t {
	char *cmd;
	int (*fcn)(cmd_t *, disk_t *, mbr_t *, mbr_t *, int);
	char *help;
} cmd_table_t;


/* Prototypes */
int Xerase __P((cmd_t *, disk_t *, mbr_t *, mbr_t *, int));
int Xreinit __P((cmd_t *, disk_t *, mbr_t *, mbr_t *, int));
int Xauto __P((cmd_t *, disk_t *, mbr_t *, mbr_t *, int));
int Xdisk __P((cmd_t *, disk_t *, mbr_t *, mbr_t *, int));
int Xmanual __P((cmd_t *, disk_t *, mbr_t *, mbr_t *, int));
int Xedit __P((cmd_t *, disk_t *, mbr_t *, mbr_t *, int));
int Xsetpid __P((cmd_t *, disk_t *, mbr_t *, mbr_t *, int));
int Xselect __P((cmd_t *, disk_t *, mbr_t *, mbr_t *, int));
int Xprint __P((cmd_t *, disk_t *, mbr_t *, mbr_t *, int));
int Xwrite __P((cmd_t *, disk_t *, mbr_t *, mbr_t *, int));
int Xexit __P((cmd_t *, disk_t *, mbr_t *, mbr_t *, int));
int Xquit __P((cmd_t *, disk_t *, mbr_t *, mbr_t *, int));
int Xabort __P((cmd_t *, disk_t *, mbr_t *, mbr_t *, int));
int Xhelp __P((cmd_t *, disk_t *, mbr_t *, mbr_t *, int));
int Xflag __P((cmd_t *, disk_t *, mbr_t *, mbr_t *, int));
int Xupdate __P((cmd_t *, disk_t *, mbr_t *, mbr_t *, int));

#endif /* _CMD_H */


