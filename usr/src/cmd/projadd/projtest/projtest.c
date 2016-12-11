/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libintl.h>
#include <locale.h>
#include <errno.h>
#include <string.h>
#include <stddef.h>
#include <sys/types.h>

#include "projent.h"
#include "util.h"


/*
 * file1, parse error (extra ":") on group.staff project.
 */
#define FILE1 "test1:123:project one:root,bin:adm:attr1=a;attr2=b\n"	\
"user.test2:456:project two:adm,uucp:staff:attr1=p;attr2=q\n"		\
"group.test3:678:project three::root,nobody:root,lp:attr1=y;attr2=z\n"	\
"test4:678:project four:root:root:\n"					\
"test5:679:project five::sys:\n"					\
"test6:690::::\n"

/*
 * file2, duplicate project names.
 */
#define FILE2 "test1:123:project one:root,bin:adm:attr1=a;attr2=b\n"	\
"user.test2:456:project two:adm,uucp:staff:attr1=p;attr2=q\n"		\
"group.test3:677:project three:root,nobody:root,lp:attr1=y;attr2=z\n"	\
"test1:678:project four:root:root:\n"					\
"test5:679:project five::sys:\n"					\
"test6:690::::\n"

/*
 * file3, duplicate project ids.
 */
#define FILE3 "test1:123:project one:root,bin:adm:attr1=a;attr2=b\n"	\
"user.test2:456:project two:adm,uucp:staff:attr1=p;attr2=q\n"		\
"group.test3:677:project three:root,nobody:root,lp:attr1=y;attr2=z\n"	\
"test4:678:project four:root:root:\n"					\
"test5:678:project five::sys:\n"					\
"test6:690::::\n"

/*
 * file4, system project ids.
 */
#define FILE4 "system:0::::\n"						\
"user.root:1::::\n"							\
"noproject:2::::\n"							\
"default:3::::\n"							\
"group.staff:10::::\n"							\
"test1:123:project one:root,bin:adm:attr1=a;attr2=b\n"			\
"user.test2:456:project two:adm,uucp:staff:attr1=p;attr2=q\n"		\
"group.test3:677:project three:root,nobody:root,lp:attr1=y;attr2=z\n"	\
"test4:678:project four:root:root:\n"					\
"test5:679:project five::sys:\n"					\
"test6:690::::\n"

/*
 * file5, all unique user projects.
 */
#define FILE5 "test1:123:project one:root,bin:adm:attr1=a;attr2=b\n"	\
"user.test2:456:project two:adm,uucp:staff:attr1=p;attr2=q\n"		\
"group.test3:677:project three:root,nobody:root,lp:attr1=y;attr2=z\n"	\
"test4:678:project four:root:root:\n"					\
"test5:679:project five::sys:\n"					\
"test6:690::::\n"


#define FLAG1	(0)
#define FLAG2	(F_PAR_VLD)
#define FLAG3	(F_PAR_VLD | F_PAR_RES)
#define FLAG4	(F_PAR_VLD | F_PAR_DUP)


typedef struct {
	char *file;
	int flags;
	int res;
} read_test;

#define	READ_TESTS_SIZE	20
read_test rtests[READ_TESTS_SIZE] = {
	{FILE1, FLAG1, 1},
	{FILE1, FLAG2, 1},
	{FILE1, FLAG3, 1},
	{FILE1, FLAG4, 1},
	{FILE2, FLAG1, 0},
	{FILE2, FLAG2, 1},
	{FILE2, FLAG3, 1},
	{FILE2, FLAG4, 1},
	{FILE3, FLAG1, 0},
	{FILE3, FLAG2, 1},
	{FILE3, FLAG3, 1},
	{FILE3, FLAG4, 0},
	{FILE4, FLAG1, 0},
	{FILE4, FLAG2, 1},
	{FILE4, FLAG3, 0},
	{FILE4, FLAG4, 1},
	{FILE5, FLAG1, 0},
	{FILE5, FLAG2, 0},
	{FILE5, FLAG3, 0},
	{FILE5, FLAG4, 0},
};


typedef struct {
	int pres;
	int vres;
	int flags;
	char *pline;
} parse_validate_test;



#define FLAG_000 0
#define FLAG_RES F_PAR_RES
#define FLAG_SPC F_PAR_SPC
#define FLAG_UNT F_PAR_UNT

#define PARSE_VALIDATE_TESTS_SIZE	311
parse_validate_test pvtests[PARSE_VALIDATE_TESTS_SIZE] = {
/* positive */
	{ 0, 0, FLAG_RES, "system:0::::"},
	{ 0, 0, FLAG_RES, "user.root:1::::"},
	{ 0, 0, FLAG_RES, "noproject:2::::"},
	{ 0, 0, FLAG_RES, "default:3::::"},
	{ 0, 0, FLAG_RES, "group.staff:10::::"},
	{ 0, 0, FLAG_000, "long:100::::aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},
	{ 0, 0, FLAG_000, "Validname:101::::"},
	{ 0, 0, FLAG_000, "Validname2:102::::"},
	{ 0, 0, FLAG_000, "valid3name:103::::"},
	{ 0, 0, FLAG_000, "VALIDNAME:104::::"},
	{ 0, 0, FLAG_000, "VALIDNAME5:105::::"},
	{ 0, 0, FLAG_000, "vAlid5name:106::::"},
	{ 0, 0, FLAG_000, "valid.name:107::::"},
	{ 0, 0, FLAG_000, "valid8.NAME:108::::"},
	{ 0, 0, FLAG_000, "Valid_name9:109::::"},
	{ 0, 0, FLAG_000, "V_alid.name10:110::::"},
	{ 0, 0, FLAG_000, "valid12345678901234567890123456789012345678901234567890123456789:111::::"},
	{ 0, 0, FLAG_000, "projid:2147483647::::"},
	{ 0, 0, FLAG_000, "comment:111: this is ! & my crazy  !@#$%^&*()_+|~`\\=-]{ 0, 0, FLAG_000,}{';\\\"/.,?>< comment:::"},
	{ 0, 0, FLAG_000, "user1:112::*::"},
	{ 0, 0, FLAG_000, "user2:113::!*::"},
	{ 0, 0, FLAG_000, "user3:114::root::"},
	{ 0, 0, FLAG_000, "user4:115::!root::"},
	{ 0, 0, FLAG_000, "user5:116::*,!sys::"},
	{ 0, 0, FLAG_000, "user6:117::!*,daemon::"},
	{ 0, 0, FLAG_000, "user7:118::root,sys,daemon,bin::"},
	{ 0, 0, FLAG_000, "user8:119::root,!sys,daemon,!bin::"},
	{ 0, 0, FLAG_SPC, "user9:116::*, !sys::"},
	{ 0, 0, FLAG_SPC, "user10:117::!* ,daemon::"},
	{ 0, 0, FLAG_SPC, "user11:118::root ,sys ,daemon, bin::"},
	{ 0, 0, FLAG_SPC, "user12:119::root, !sys, daemon ,!bin::"},
	{ 0, 0, FLAG_000, "group1:120:::*:"},
	{ 0, 0, FLAG_000, "group2:121:::!*:"},
	{ 0, 0, FLAG_000, "group3:122:::root:"},
	{ 0, 0, FLAG_000, "group4:123:::!root:"},
	{ 0, 0, FLAG_000, "group5:124:::*,!sys:"},
	{ 0, 0, FLAG_000, "group6:125:::!*,daemon:"},
	{ 0, 0, FLAG_000, "group7:126:::root,sys,daemon,bin:"},
	{ 0, 0, FLAG_000, "group8:127:::root,!sys,daemon,!bin:"},
	{ 0, 0, FLAG_SPC, "group9:124:::*, !sys:"},
	{ 0, 0, FLAG_SPC, "group10:125:::!* ,daemon:"},
	{ 0, 0, FLAG_SPC, "group11:126:::root, sys ,daemon, bin:"},
	{ 0, 0, FLAG_SPC, "group12:127:::root ,!sys, daemon ,!bin:"},
	{ 0, 0, FLAG_000, "group9:128:::sys:"},
	{ 0, 0, FLAG_000, "attrib1:129::::one"},
	{ 0, 0, FLAG_000, "attrib2:130::::One"},
	{ 0, 0, FLAG_000, "attrib3:131::::ONE"},
	{ 0, 0, FLAG_000, "attrib4:132::::attrib10"},
	{ 0, 0, FLAG_000, "attrib5:133::::attrib.attrib="},
	{ 0, 0, FLAG_000, "attrib6:134::::attib_"},
	{ 0, 0, FLAG_000, "attrib7:135::::a10-._attib"},
	{ 0, 0, FLAG_000, "attrib8:136::::SUNW,attrib"},
	{ 0, 0, FLAG_000, "attrib9:137::::A,A10="},
	{ 0, 0, FLAG_000, "attrib10:138::::FIVEE,name"},
	{ 0, 0, FLAG_000, "attrib11:139::::one;two"},
	{ 0, 0, FLAG_000, "attrib12:140::::one=1;two=four"},
	{ 0, 0, FLAG_000, "attrib13:141::::one;two=;three=four"},
	{ 0, 0, FLAG_000, "value1:142::::one=foo,bar"},
	{ 0, 0, FLAG_000, "value2:143::::one=,bar,"},
	{ 0, 0, FLAG_000, "value3:144::::one=(foo,bar)"},
	{ 0, 0, FLAG_000, "value4:145::::one=(foo,bar,baz),boo"},
	{ 0, 0, FLAG_000, "value5:146::::one;two=bar,(baz),foo,((baz)),(,)"},
	{ 0, 0, FLAG_000, "value6:147::::one=100/200"},
	{ 0, 0, FLAG_000, "value7:148::::two=.-_/="},
	{ 0, 0, FLAG_000, "value8:149::::name=one=two"},
	{ 0, 0, FLAG_UNT, "value9:150::::task.max-lwps=(priv,1000M,deny,signal=SIGHUP),(priv,1000k,deny,signal=SIGKILL)"},
	{ 0, 0, FLAG_000, "comma1:151::,::"},
	{ 0, 0, FLAG_000, "comma2:152::,,::"},
	{ 0, 0, FLAG_000, "comma3:153::root,::"},
	{ 0, 0, FLAG_000, "comma4:154::bin,root,,::"},
	{ 0, 0, FLAG_000, "comma5:155:::,:"},
	{ 0, 0, FLAG_000, "comma6:156:::,,:"},
	{ 0, 0, FLAG_000, "comma7:157:::bin,root,:"},
	{ 0, 0, FLAG_000, "comma8:158:::root,,:"},
	{ 0, 0, FLAG_000, "semi1:159::::;"},
	{ 0, 0, FLAG_000, "semi2:160::::;;"},
	{ 0, 0, FLAG_000, "semi3:161::::foo=(one,two);"},
	{ 0, 0, FLAG_000, "semi4:162::::foo;;"},
	{ 0, 0, FLAG_UNT, "rctl1:163::::task.max-lwps=(priv,1000,deny,signal=HUP),(priv,1000k,deny,signal=15)"},
	{ 0, 0, FLAG_000, "rctl1:163::::task.max-lwps=(priv,1000,deny,signal=HUP),(priv,10001,deny,signal=15)"},
	{ 0, 0, FLAG_000, "rctl2:164::::process.max-port-events=(basic,1000,deny)"},
	{ 0, 0, FLAG_UNT, "rctl3:165::::project.max-crypto-memory=(priv,2.2gb,deny)"},
	{ 0, 0, FLAG_000, "rctl3:165::::project.max-crypto-memory=(priv,10,deny)"},
	{ 0, 0, FLAG_UNT, "rctl4:166::::project.max-crypto-memory=(privileged,100m,deny)"},
	{ 0, 0, FLAG_000, "rctl4:166::::project.max-crypto-memory=(privileged,100,deny)"},
	{ 0, 0, FLAG_UNT, "rctl5:167::::project.max-crypto-memory=(priv,1000m,deny)"},
	{ 0, 0, FLAG_000, "rctl5:167::::project.max-crypto-memory=(priv,1000,deny)"},
	{ 0, 0, FLAG_UNT, "rctl6:168::::project.max-crypto-memory=(priv,1000k,deny)"},
	{ 0, 0, FLAG_UNT, "rctl6:168::::project.max-crypto-memory=(priv,1000m,deny)"},
	{ 0, 0, FLAG_000, "rctl7:169::::process.max-msg-messages=(priv,10,deny)"},
	{ 0, 0, FLAG_UNT, "rctl8:170::::process.max-msg-qbytes=(priv,10000kb,deny)"},
	{ 0, 0, FLAG_000, "rctl8:170::::process.max-msg-qbytes=(priv,10000,deny)"},
	{ 0, 0, FLAG_000, "rctl9:171::::process.max-sem-ops=(priv,10000000,deny)"},
	{ 0, 0, FLAG_000, "rctl10:172::::process.max-sem-nsems=(basic,1,deny)"},
	{ 0, 0, FLAG_UNT, "rctl11:173::::process.max-address-space=(priv,2GB,deny)"},
	{ 0, 0, FLAG_UNT, "rctl12:174::::process.max-file-descriptor=(basic,1K,deny),(basic,2K,deny)"},
	{ 0, 0, FLAG_UNT, "rctl13:175::::process.max-core-size=(priv,10Mb,deny),(priv,2GB,deny)"},
	{ 0, 0, FLAG_UNT, "rctl14:176::::process.max-stack-size=(priv,1.8Gb,deny),(priv,100MB,deny)"},
	{ 0, 0, FLAG_000, "rctl15:177::::process.max-data-size=(priv,1010100101,deny)"},
	{ 0, 0, FLAG_UNT, "rctl16:178::::process.max-file-size=(priv,100mb,deny,signal=SIGXFSZ),(priv,1000mb,deny,signal=31)"},
	{ 0, 0, FLAG_UNT, "rctl17:179::::process.max-cpu-time=(priv,1t,signal=XCPU),(priv,100ms,sig=30)"},
	{ 0, 0, FLAG_UNT, "rctl18:180::::task.max-cpu-time=(priv,1M,sig=SIGKILL)"},
	{ 0, 0, FLAG_UNT, "rctl19:181::::task.max-lwps=(basic,10,signal=1),(priv,100,deny,signal=KILL)"},
	{ 0, 0, FLAG_000, "rctl20:182::::project.max-device-locked-memory=(priv,1000,deny,sig=TERM)"},
	{ 0, 0, FLAG_000, "rctl21:183::::project.max-port-ids=(priv,100,deny)"},
	{ 0, 0, FLAG_UNT, "rctl22:184::::project.max-shm-memory=(priv,1000mb,deny)"},
	{ 0, 0, FLAG_UNT, "rctl23:185::::project.max-shm-ids=(priv,1k,deny,signal=SIGSTOP)"},
	{ 0, 0, FLAG_UNT, "rctl24:186::::project.max-msg-ids=(priv,1m,deny,signal=XRES)"},
	{ 0, 0, FLAG_000, "rctl25:187::::project.max-sem-ids=(priv,10,deny,signal=ABRT)"},
	{ 0, 0, FLAG_UNT, "rctl26:188::::project.cpu-shares=(priv,63k,none)"},
	{ 0, 0, FLAG_UNT, "rctl27:189::::zone.cpu-shares=(priv,20k,none)"},
	{ 0, 0, FLAG_000, "rctl28:190::::zone.cpu-shares=(priv,100,none)"},
	{ 0, 0, FLAG_UNT, "rctl29:191::::project.max-shm-memory=(priv,200G,deny)"},
	{ 0, 0, FLAG_UNT, "rctl30:192::::project.max-shm-memory=(priv,200Gb,deny)"},
	{ 0, 0, FLAG_UNT, "rctl31:193::::project.max-shm-memory=(priv,2000B,deny)"},
	{ 0, 0, FLAG_000, "rctl32:194::::project.max-shm-memory=(priv,2000,deny)"},
	{ 0, 0, FLAG_000, "rctl33:195::::task.max-cpu-time=(priv,2000,none)"},
	{ 0, 0, FLAG_UNT, "rctl34:196::::task.max-cpu-time=(priv,2000s,none)"},
	{ 0, 0, FLAG_UNT, "rctl35:197::::task.max-cpu-time=(priv,20.1ps,none)"},
	{ 0, 0, FLAG_UNT, "rctl36:198::::task.max-cpu-time=(priv,20T,none)"},
/* negative */
	{ 0, 1, FLAG_000, "system:0::::"},
	{ 0, 1, FLAG_000, "user.root:1::::"},
	{ 0, 1, FLAG_000, "noproject:2::::"},
	{ 0, 1, FLAG_000, "default:3::::"},
	{ 0, 1, FLAG_000, "group.staff:10::::"},
	{ 0, 1, FLAG_000, "long:100::::aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},
	{ 1, 0, FLAG_000, "extrafields:101:::::"},
	{ 1, 0, FLAG_000, "missingfields:102:::"},
	{ 1, 0, FLAG_000, "_invalidname:103::::"},
	{ 1, 0, FLAG_000, "10invlidname:104::::"},
	{ 1, 0, FLAG_000, "invalid%name:105::::"},
	{ 1, 0, FLAG_000, "invalid/name:106::::"},
	{ 1, 0, FLAG_000, ".invalidname:107::::"},
	{ 1, 0, FLAG_000, "=invalidName:108::::"},
	{ 1, 0, FLAG_000, "invalid=name:109::::"},
	{ 1, 0, FLAG_000, "invalid/name:110::::"},
	{ 1, 0, FLAG_000, "/invalidname:111::::"},
	{ 1, 0, FLAG_000, "/invalidname:112::::"},
	{ 1, 0, FLAG_000, "invalidname*:113::::"},
	{ 1, 0, FLAG_000, "invalid?name:114::::"},
	{ 1, 0, FLAG_000, ":115:invalid name comment:::"},
	{ 1, 0, FLAG_000, "invalid!name:116::::"},
	{ 1, 0, FLAG_000, "invalidname!:117::::"},
	{ 1, 0, FLAG_000, "invalid12345678901234567890123456789012345678901234567890123456789:118::::"},
	{ 1, 0, FLAG_000, "projid:-1::::"},
	{ 1, 0, FLAG_000, "projid:abc::::"},
	{ 1, 0, FLAG_000, "projid:2147483648::::"},
	{ 1, 0, FLAG_000, "projid:::::"},
	{ 1, 0, FLAG_000, "user1:118::*!::"},
	{ 1, 0, FLAG_000, "user2:119::10user::"},
	{ 0, 1, FLAG_000, "user3:120::NOLOWER::"},
	{ 0, 1, FLAG_000, "user4:121::toooolong::"},
	{ 1, 0, FLAG_000, "user5:122::root!::"},
	{ 1, 0, FLAG_000, "user6:123::root;sys::"},
	{ 0, 1, FLAG_000, "user7:124::sys,NOLOWER::"},
	{ 1, 0, FLAG_000, "user8:125::sys/bin,root::"},
	{ 1, 0, FLAG_000, "user9:116::*, !sys::"},
	{ 1, 0, FLAG_000, "user10:117::!* ,daemon::"},
	{ 1, 0, FLAG_000, "user11:118::root ,sys ,daemon, bin::"},
	{ 1, 0, FLAG_000, "user12:119::root, !sys, daemon ,!bin::"},
	{ 1, 0, FLAG_000, "group1:126:::*!:"},
	{ 0, 1, FLAG_000, "group2:127:::oneUpper:"},
	{ 0, 1, FLAG_000, "group3:128:::NOLOWER:"},
	{ 0, 1, FLAG_000, "group4:129:::toooolong:"},
	{ 1, 0, FLAG_000, "group5:130:::root!:"},
	{ 1, 0, FLAG_000, "group6:131:::root;sys:"},
	{ 0, 1, FLAG_000, "group7:132:::sys,NOLOWER:"},
	{ 1, 0, FLAG_000, "group8:133:::sys-bin,root:"},
	{ 1, 0, FLAG_000, "group9:124:::*, !sys:"},
	{ 1, 0, FLAG_000, "group10:125:::!* ,daemon:"},
	{ 1, 0, FLAG_000, "group11:126:::root, sys ,daemon, bin:"},
	{ 1, 0, FLAG_000, "group12:127:::root ,!sys, daemon ,!bin:"},
	{ 1, 0, FLAG_000, "attrib1:134::::10"},
	{ 1, 0, FLAG_000, "attrib2:135::::_foo="},
	{ 1, 0, FLAG_000, "attrib3:136::::,foo"},
	{ 1, 0, FLAG_000, "attrib4:137::::sun,foo"},
	{ 1, 0, FLAG_000, "attrib6:139::::!attrib"},
	{ 1, 0, FLAG_000, "attrib7:140::::_attrib"},
	{ 1, 0, FLAG_000, "attrib8:141::::attib,attrib"},
	{ 1, 0, FLAG_000, "attrib9:142::::attrib/attrib"},
	{ 1, 0, FLAG_000, "attrib10:143::::one;two,three"},
	{ 1, 0, FLAG_000, "attrib11:144::::one=two;three/"},
	{ 1, 0, FLAG_000, "value1:145::::one=foo%"},
	{ 1, 0, FLAG_000, "value2:146::::one= two"},
	{ 1, 0, FLAG_000, "value3:147::::var=foo?"},
	{ 1, 0, FLAG_000, "value4:148::::name=value;name=value2)"},
	{ 1, 0, FLAG_000, "value5:149::::(foo)"},
	{ 1, 0, FLAG_000, "value6:150::::name=(foo,bar"},
	{ 1, 0, FLAG_000, "value7:151::::name=(value)(value)"},
	{ 1, 0, FLAG_000, "value8:152::::name=)"},
	{ 1, 0, FLAG_000, "value9:153::::name=value,(value value)"},
	{ 1, 0, FLAG_000, "value10:154::::name=(value(value))"},
	{ 1, 0, FLAG_000, "value11:155::::name=(value)value"},
	{ 1, 0, FLAG_000, "value11:156::::name=va?lue"},
	{ 1, 0, FLAG_000, "value12:157::::name=(value,value))"},
	{ 1, 0, FLAG_000, "value13:158::::name=(value),value)"},
	{ 1, 0, FLAG_000, "space1 :159::::"},
	{ 1, 0, FLAG_000, " space2:160::::"},
	{ 1, 0, FLAG_000, "space3: 161::::"},
	{ 1, 0, FLAG_000, "space4:162 ::::"},
	{ 1, 0, FLAG_000, "space 5:163::::"},
	{ 1, 0, FLAG_000, "space6:1 64::::"},
	{ 1, 0, FLAG_000, "space7:165:: root::"},
	{ 1, 0, FLAG_000, "space8:166::root ::"},
	{ 1, 0, FLAG_000, "space9:167::daemon, root::"},
	{ 1, 0, FLAG_000, "space10:168::bin root::"},
	{ 1, 0, FLAG_000, "space11:169::daemon ,root::"},
	{ 1, 0, FLAG_000, "space12 :170::::"},
	{ 1, 0, FLAG_000, " space13:171::::"},
	{ 1, 0, FLAG_000, "space14: 172::::"},
	{ 1, 0, FLAG_000, "space15:173 ::::"},
	{ 1, 0, FLAG_000, "space 16:174::::"},
	{ 1, 0, FLAG_000, "space17:1 75::::"},
	{ 1, 0, FLAG_000, "space18:176::: root:"},
	{ 1, 0, FLAG_000, "space19:177:::root :"},
	{ 1, 0, FLAG_000, "space20:178:::daemon, root:"},
	{ 1, 0, FLAG_000, "space21:179:::bin root:"},
	{ 1, 0, FLAG_000, "space22:180:::daemon ,root:"},
	{ 1, 0, FLAG_000, "space23:181:::: foo"},
	{ 1, 0, FLAG_000, "space34:182::::foo =one"},
	{ 1, 0, FLAG_000, "space35:183::::foo= (one)"},
	{ 1, 0, FLAG_000, "space36:184::::foo=(one, two)"},
	{ 1, 0, FLAG_000, "space37:185::::foo=(one ,two)"},
	{ 1, 0, FLAG_000, "space38:186::::foo=( one)"},
	{ 1, 0, FLAG_000, "space39:187::::foo=(one )"},
	{ 1, 0, FLAG_000, "space40:188::::foo=(one) ,two"},
	{ 1, 0, FLAG_000, "space41:189::::foo=one, (two)"},
	{ 1, 0, FLAG_000, "comma1:190::,root,bin::"},
	{ 1, 0, FLAG_000, "comma2:191::root,,bin::"},
	{ 1, 0, FLAG_000, "comma3:192::,,root,bin::"},
	{ 1, 0, FLAG_000, "comma4:193:::,root,bin:"},
	{ 1, 0, FLAG_000, "comma5:194:::root,,bin:"},
	{ 1, 0, FLAG_000, "comma6:195:::,,root,bin:"},
	{ 1, 0, FLAG_000, "semi1:196::::;foo"},
	{ 1, 0, FLAG_000, "semi2:197::::foo;;bar=1"},
	{ 1, 0, FLAG_000, "semi3:198::::;;bar=(10)"},
	{ 0, 1, FLAG_000, "rctl1:199::::task.max-lwps=,"},
	{ 0, 1, FLAG_000, "rctl2:200::::task.max-lwps="},
	{ 0, 1, FLAG_000, "rctl3:201::::task.max-lwps=priv"},
	{ 0, 1, FLAG_000, "rctl4:202::::task.max-lwps=priv,1000"},
	{ 0, 1, FLAG_000, "rctl5:203::::task.max-lwps=priv,1000,deny"},
	{ 0, 1, FLAG_000, "rctl6:204::::task.max-lwps=(priv)"},
	{ 0, 1, FLAG_000, "rctl7:205::::task.max-lwps=(priv,1000)"},
	{ 0, 1, FLAG_000, "rctl8:206::::task.max-lwps=(foo,100,deny)"},
	{ 0, 1, FLAG_000, "rctl9:207::::task.max-lwps=(priv,foo,none)"},
	{ 1, 0, FLAG_UNT, "rctl9:207::::task.max-lwps=(priv,foo,none)"},
	{ 1, 0, FLAG_UNT, "rctl10:208::::task.max-lwps=(priv,100foo,none)"},
	{ 0, 1, FLAG_000, "rctl11:209::::task.max-lwps=(priv,1000,foo)"},
	{ 0, 1, FLAG_UNT, "rctl12:210::::task.max-lwps=(priv,1000k,deny,signal)"},
	{ 0, 1, FLAG_000, "rctl13:211::::task.max-lwps=(priv,1000,deny,signal=)"},
	{ 0, 1, FLAG_000, "rctl14:212::::task.max-lwps=(priv,1000,deny,signal=foo)"},
	{ 0, 1, FLAG_000, "rctl15:213::::task.max-lwps=(priv,1000,deny,signal=1fo)"},
	{ 0, 1, FLAG_000, "rctl16:214::::task.max-lwps=(priv,1000,deny,signal=100)"},
	{ 0, 1, FLAG_000, "rctl17:215::::task.max-lwps=(priv,1000,deny,signal=SIG)"},
	{ 0, 1, FLAG_000, "rctl18:216::::task.max-lwps=(priv,1000,deny,signal=SIG1)"},
	{ 0, 1, FLAG_000, "rctl19:217::::task.max-lwps=(priv,1000,deny,signal=SIGhup)"},
	{ 0, 1, FLAG_000, "rctl20:218::::task.max-lwps=(priv,1000,deny,signal=SIGHU)"},
	{ 0, 1, FLAG_000, "rctl21:219::::task.max-lwps=(priv,1000,deny,signal=SIGHUPP)"},
	{ 0, 1, FLAG_000, "rctl22:220::::task.max-lwps=(priv,1000,deny,signal=SIGURG)"},
	{ 0, 1, FLAG_000, "rctl23:221::::task.max-lwps=(priv,1000,deny,signal=SIGXCPU)"},
	{ 0, 1, FLAG_000, "rctl24:222::::task.max-lwps=(priv,1000,deny,signal=SIGKILL,10)"},
	{ 0, 1, FLAG_000, "rctl25:223::::task.max-lwps=(priv,1000,deny,signal=SIGKILL,foo)"},
	{ 0, 1, FLAG_000, "rctl26:224::::process.max-port-events=(priv,1000,none)"},
	{ 0, 1, FLAG_UNT, "rctl27:225::::process.max-address-space=(basic,1024mb,deny,signal=TERM)"},
	{ 0, 1, FLAG_000, "rctl28:226::::process.max-cpu-time=(basic,3600,deny)"},
	{ 0, 1, FLAG_000, "rctl29:227::::task.max-lwps=()"},
	{ 0, 1, FLAG_000, "rctl30:228::::task.max-lwps=((priv),deny)"},
	{ 0, 1, FLAG_000, "rctl31:229::::task.max-lwps=((priv,1000,deny))"},
	{ 0, 1, FLAG_000, "rctl32:230::::task.max-lwps=(priv,((1000,2000,1000)),deny)"},
	{ 0, 1, FLAG_000, "rctl33:231::::task.max-lwps=(,,,)"},
	{ 0, 1, FLAG_000, "rctl34:232::::task.max-lwps=(priv,1000,(deny))"},
	{ 0, 1, FLAG_000, "rctl35:233::::task.max-lwps=(priv,1000,deny),foo"},
	{ 0, 1, FLAG_000, "rctl36:234::::task.max-lwps=(priv,1000,deny),(priv,1000)"},
	{ 1, 0, FLAG_UNT, "rctl37:235::::project.max-msg-ids=(priv,15EB,deny)"},
	{ 1, 0, FLAG_UNT, "rctl38:236::::process.max-address-space=(priv,16.1EB,deny)"},
	{ 1, 0, FLAG_UNT, "rctl39:237::::process.max-address-space=(priv,18000000000gb,deny)"},
	{ 1, 0, FLAG_UNT, "rctl40:238::::zone.cpu-shares=(priv,10kb,none)"},
	{ 1, 0, FLAG_UNT, "rctl41:239::::zone.cpu-shares=(priv,10Ks,none)"},
	{ 1, 0, FLAG_UNT, "rctl42:240::::zone.cpu-shares=(priv,10s,none)"},
	{ 1, 0, FLAG_UNT, "rctl43:241::::zone.cpu-shares=(priv,100000b,none)"},
	{ 1, 0, FLAG_UNT, "rctl44:242::::project.max-shm-memory=(priv,200Ts,deny)"},
	{ 1, 0, FLAG_UNT, "rctl45:243::::project.max-shm-memory=(priv,200s,deny)"},
	{ 1, 0, FLAG_UNT, "rctl46:244::::task.max-cpu-time=(priv,20B,none)"},
	{ 1, 0, FLAG_UNT, "rctl47:245::::task.max-cpu-time=(priv,20Kb,none)"},
	{ 0, 1, FLAG_UNT, "rctl48:246::::project.cpu-shares=(priv,100k,none)"},
	{ 0, 1, FLAG_000, "rctl147:150::::task.max-lwps=(priv,1000M,deny,signal=SIGHUP),(priv,1000k,deny,signal=SIGKILL)"},
	{ 0, 1, FLAG_000, "rctl148:163::::task.max-lwps=(priv,1000,deny,signal=HUP),(priv,1000k,deny,signal=15)"},
	{ 0, 1, FLAG_000, "rctl3:165::::project.max-crypto-memory=(priv,10eb,deny)"},
	{ 0, 1, FLAG_000, "rctl4:166::::project.max-crypto-memory=(privileged,100p,deny)"},
	{ 0, 1, FLAG_000, "rctl5:167::::project.max-crypto-memory=(priv,1000t,deny)"},
	{ 0, 1, FLAG_000, "rctl6:168::::project.max-crypto-memory=(priv,1000g,deny)"},
	{ 0, 1, FLAG_000, "rctl7:169::::process.max-msg-messages=(priv,10m,deny)"},
	{ 0, 1, FLAG_000, "rctl8:170::::process.max-msg-qbytes=(priv,10000kb,deny)"},
	{ 0, 1, FLAG_000, "rctl11:173::::process.max-address-space=(priv,10EB,deny)"},
	{ 0, 1, FLAG_000, "rctl12:174::::process.max-file-descriptor=(basic,1K,deny),(basic,2K,deny)"},
	{ 0, 1, FLAG_000, "rctl13:175::::process.max-core-size=(priv,1Eb,deny),(priv,10PB,deny)"},
	{ 0, 1, FLAG_000, "rctl14:176::::process.max-stack-size=(priv,10Tb,deny),(priv,10TB,deny)"},
	{ 0, 1, FLAG_000, "rctl16:178::::process.max-file-size=(priv,100mb,deny,signal=SIGXFSZ),(priv,1000mb,deny,signal=31)"},
	{ 0, 1, FLAG_000, "rctl17:179::::process.max-cpu-time=(priv,1t,signal=XCPU),(priv,100ms,sig=30)"},
	{ 0, 1, FLAG_000, "rctl18:180::::task.max-cpu-time=(priv,1M,sig=SIGKILL)"},
	{ 0, 1, FLAG_000, "rctl22:184::::project.max-shm-memory=(priv,1000mb,deny)"},
	{ 0, 1, FLAG_000, "rctl23:185::::project.max-shm-ids=(priv,1k,deny,signal=SIGSTOP)"},
	{ 0, 1, FLAG_000, "rctl24:186::::project.max-msg-ids=(priv,1m,deny,signal=XRES)"},
	{ 0, 1, FLAG_000, "rctl26:188::::project.cpu-shares=(priv,63k,none)"},
	{ 0, 1, FLAG_000, "rctl27:189::::zone.cpu-shares=(priv,20k,none)"},
	{ 0, 1, FLAG_000, "rctl29:191::::project.max-shm-memory=(priv,200G,deny)"},
	{ 0, 1, FLAG_000, "rctl30:192::::project.max-shm-memory=(priv,200Gb,deny)"},
	{ 0, 1, FLAG_000, "rctl31:193::::project.max-shm-memory=(priv,2000B,deny)"},
	{ 0, 1, FLAG_000, "rctl34:196::::task.max-cpu-time=(priv,2000s,none)"},
	{ 0, 1, FLAG_000, "rctl35:197::::task.max-cpu-time=(priv,20.1ps,none)"},
	{ 0, 1, FLAG_000, "rctl36:198::::task.max-cpu-time=(priv,20T,none)"},
};


boolean_t vflag = B_FALSE;
boolean_t kflag = B_FALSE;

int
projtest_compare_two_files(char *file1, char *file2, lst_t *errlst)
{
	FILE *fp1, *fp2;
	int c1, c2;

	if((fp1 = fopen(file1, "r")) == NULL) {
		util_add_errmsg(errlst, "failed to open \"%s\"", file1);
		return (1);
	}

	if((fp2 = fopen(file2, "r")) == NULL) {
		util_add_errmsg(errlst, "failed to open \"%s\"", file2);
		return (1);
	}

	c1 = getc(fp1);
	c2 = getc(fp2);
	while((c1 != EOF) && (c2 != EOF) && (c1 == c2)) {
		c1 = getc(fp1);
		c2 = getc(fp2);
	}
	fclose(fp1);
	fclose(fp2);

	if (c1 != c2) {
		util_add_errmsg(errlst, "files differ \"%s\" \"%s\"",
		    file1, file2);
		return (1);
	}

	return (0);
}

void
projtest_free_errlst(lst_t *errlst)
{
	char *msg;
	while(!lst_is_empty(errlst)) {
		msg = lst_at(errlst,  0);
		lst_remove(errlst, msg);
		free(msg);
	}
}

void
do_read_test(read_test *rt, int tnum)
{
	FILE *fp;
	char *projfile, *projwrite;
	lst_t *plst;
	lst_t errlst;
	lst_create(&errlst);


	asprintf(&projfile, "/tmp/project_%d_%d_tmp", getpid(), tnum);
	unlink(projfile);
	if ((fp = fopen(projfile, "wx")) == NULL) {
		fprintf(stderr, "error opening %s\n", projfile);
		exit(1);
	}
	fputs(rt->file, fp);
	fclose(fp);

	asprintf(&projwrite, "/tmp/project_%d_%d_wrt", getpid(), tnum);
	if ((fp = fopen(projwrite, "wx")) == NULL) {
		fprintf(stderr, "error opening %s\n", projfile);
		exit(1);
	}
	fclose(fp);


	plst = projent_get_lst(projfile, rt->flags, &errlst);


	if (plst == NULL && rt->res == 1) {
		printf("Read test[%02d]: **************[-SUCCESS-]\n", tnum);
	} else if (plst != NULL && rt->res == 0) {

		projent_put_lst(projwrite, plst, &errlst);
		projtest_compare_two_files(projfile, projwrite, &errlst);
		printf("Read test[%02d]: **************[%s]\n", tnum,
		    lst_is_empty(&errlst) ? "-SUCCESS-" : "*FAILURE*");

	} else {
		printf("Read test[%02d]: **************[*FAILURE*]\n", tnum);
	}

	if (vflag) {
		util_print_errmsgs(&errlst);
	}

	if (!kflag) {
		unlink(projfile);
		unlink(projwrite);
	}

	projent_free_lst(plst);
	projtest_free_errlst(&errlst);
}


void
do_parse_validate_test(parse_validate_test *pvt, int tnum)
{
	lst_t errlst;
	projent_t *ent;
	int vres;

	lst_create(&errlst);

	ent = projent_parse(pvt->pline, pvt->flags, &errlst);

	if (ent == NULL && pvt->pres == 1) {
		printf("Prse test[%02d]: **************[-SUCCESS-]\n", tnum);
	} else if (ent != NULL && pvt->pres == 0) {
		printf("Prse test[%02d]: **************[-SUCCESS-]\n", tnum);
		vres = projent_validate(ent, pvt->flags, &errlst);
		printf("Vdte test[%02d]: **************[%s]\n", tnum,
		    ((vres == 0 && pvt->vres == 0) || (vres != 0 && pvt->vres != 0)) ? "-SUCCESS-" : "*FAILURE*");
	} else {
		printf("Prse test[%02d]: **************[*FAILURE*]\n", tnum);
	}

	
	if (vflag) {
		util_print_errmsgs(&errlst);
	}

	if (ent != NULL) {
		projent_free(ent);
		free(ent);
	}
}

void
start_read_test(void)
{
	int i;
	for (i = 0; i < READ_TESTS_SIZE; i++) {
		do_read_test(&rtests[i], i);
	}
}

void
start_parse_validate_test(void)
{
	int i;
	for (i = 0; i < PARSE_VALIDATE_TESTS_SIZE; i++) {
		do_parse_validate_test(&pvtests[i], i);
	}
}

void
usage(void)
{
	(void) fprintf(stderr,
	    "Usage:\n"
	    "projtest [-k] [-v]\n");
}

int
main(int argc, char **argv)
{
	int c;

	while((c = getopt(argc, argv, "vk")) != EOF) {
		switch(c) {
			case 'v':
				vflag = B_TRUE;
				break;
			case 'k':
				kflag = B_TRUE;
				break;
			default:
				usage();
				exit(1);
				break;
		}
	}

	start_read_test();
	start_parse_validate_test();
	return (0);
}
