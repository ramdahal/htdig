/*-
 * See the file LICENSE for redistribution information.
 *
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Sleepycat Software.  All rights reserved.
 */

#include "htconfig.h"

#ifndef lint
static const char revid[] = "$Id: os_dir.c,v 1.1.2.3 2000/09/17 01:35:07 ghutchis Exp $";
#endif /* not lint */

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#if HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#endif

#include "db_int.h"
#include "os_jump.h"

/*
 * CDB___os_dirlist --
 *	Return a list of the files in a directory.
 *
 * PUBLIC: int CDB___os_dirlist __P((DB_ENV *, const char *, char ***, int *));
 */
int
CDB___os_dirlist(dbenv, dir, namesp, cntp)
	DB_ENV *dbenv;
	const char *dir;
	char ***namesp;
	int *cntp;
{
	struct dirent *dp;
	DIR *dirp;
	int arraysz, cnt, ret;
	char **names;

	if (CDB___db_jump.j_dirlist != NULL)
		return (CDB___db_jump.j_dirlist(dir, namesp, cntp));

	if ((dirp = opendir(dir)) == NULL)
		return (CDB___os_get_errno());
	names = NULL;
	for (arraysz = cnt = 0; (dp = readdir(dirp)) != NULL; ++cnt) {
		if (cnt >= arraysz) {
			arraysz += 100;
			if ((ret = CDB___os_realloc(dbenv,
			    arraysz * sizeof(names[0]), NULL, &names)) != 0)
				goto nomem;
		}
		if ((ret = CDB___os_strdup(dbenv, dp->d_name, &names[cnt])) != 0)
			goto nomem;
	}
	(void)closedir(dirp);

	*namesp = names;
	*cntp = cnt;
	return (0);

nomem:	if (names != NULL)
		CDB___os_dirfree(names, cnt);
	if (dirp != NULL)
		(void)closedir(dirp);
	return (ret);
}

/*
 * CDB___os_dirfree --
 *	Free the list of files.
 *
 * PUBLIC: void CDB___os_dirfree __P((char **, int));
 */
void
CDB___os_dirfree(names, cnt)
	char **names;
	int cnt;
{
	if (CDB___db_jump.j_dirfree != NULL)
		CDB___db_jump.j_dirfree(names, cnt);
	else {
		while (cnt > 0)
			CDB___os_free(names[--cnt], 0);
		CDB___os_free(names, 0);
	}
}
