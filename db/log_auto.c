/* Do not edit: automatically built by gen_rec.awk. */
#include "htconfig.h"

#ifndef NO_SYSTEM_INCLUDES
#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <string.h>
#endif

#include "db_int.h"
#include "db_page.h"
#include "db_dispatch.h"
#include "db_am.h"
#include "log.h"
#include "txn.h"

int
CDB___log_register1_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__log_register1_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused2 = 0;
	notused3 = NULL;

	if ((ret = CDB___log_register1_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]log_register1: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\topcode: %lu\n", (u_long)argp->opcode);
	printf("\tname: ");
	for (i = 0; i < argp->name.size; i++) {
		ch = ((u_int8_t *)argp->name.data)[i];
		if (isprint(ch) || ch == 0xa)
			putchar(ch);
		else
			printf("%#x ", ch);
	}
	printf("\n");
	printf("\tuid: ");
	for (i = 0; i < argp->uid.size; i++) {
		ch = ((u_int8_t *)argp->uid.data)[i];
		if (isprint(ch) || ch == 0xa)
			putchar(ch);
		else
			printf("%#x ", ch);
	}
	printf("\n");
	printf("\tfileid: %ld\n", (long)argp->fileid);
	printf("\tftype: 0x%lx\n", (u_long)argp->ftype);
	printf("\n");
	CDB___os_free(argp, 0);
	return (0);
}

int
CDB___log_register1_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__log_register1_args **argpp;
{
	__log_register1_args *argp;
	u_int8_t *bp;
	int ret;

	ret = CDB___os_malloc(dbenv, sizeof(__log_register1_args) +
	    sizeof(DB_TXN), NULL, &argp);
	if (ret != 0)
		return (ret);
	argp->txnid = (DB_TXN *)&argp[1];
	bp = recbuf;
	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);
	memcpy(&argp->txnid->txnid,  bp, sizeof(argp->txnid->txnid));
	bp += sizeof(argp->txnid->txnid);
	memcpy(&argp->prev_lsn, bp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	memcpy(&argp->opcode, bp, sizeof(argp->opcode));
	bp += sizeof(argp->opcode);
	memset(&argp->name, 0, sizeof(argp->name));
	memcpy(&argp->name.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->name.data = bp;
	bp += argp->name.size;
	memset(&argp->uid, 0, sizeof(argp->uid));
	memcpy(&argp->uid.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->uid.data = bp;
	bp += argp->uid.size;
	memcpy(&argp->fileid, bp, sizeof(argp->fileid));
	bp += sizeof(argp->fileid);
	memcpy(&argp->ftype, bp, sizeof(argp->ftype));
	bp += sizeof(argp->ftype);
	*argpp = argp;
	return (0);
}

int CDB___log_register_log(dbenv, txnid, ret_lsnp, flags,
	opcode, name, uid, fileid, ftype, meta_pgno)
	DB_ENV *dbenv;
	DB_TXN *txnid;
	DB_LSN *ret_lsnp;
	u_int32_t flags;
	u_int32_t opcode;
	const DBT *name;
	const DBT *uid;
	int32_t fileid;
	DBTYPE ftype;
	db_pgno_t meta_pgno;
{
	DBT logrec;
	DB_LSN *lsnp, null_lsn;
	u_int32_t zero;
	u_int32_t rectype, txn_num;
	int ret;
	u_int8_t *bp;

	if (txnid != NULL &&
	    TAILQ_FIRST(&txnid->kids) != NULL && CDB___txn_activekids(txnid) != 0)
		return (CDB___db_child_active_err(dbenv));
	rectype = DB_log_register;
	txn_num = txnid == NULL ? 0 : txnid->txnid;
	if (txnid == NULL) {
		ZERO_LSN(null_lsn);
		lsnp = &null_lsn;
	} else
		lsnp = &txnid->last_lsn;
	logrec.size = sizeof(rectype) + sizeof(txn_num) + sizeof(DB_LSN)
	    + sizeof(opcode)
	    + sizeof(u_int32_t) + (name == NULL ? 0 : name->size)
	    + sizeof(u_int32_t) + (uid == NULL ? 0 : uid->size)
	    + sizeof(fileid)
	    + sizeof(ftype)
	    + sizeof(meta_pgno);
	if ((ret = CDB___os_malloc(dbenv, logrec.size, NULL, &logrec.data)) != 0)
		return (ret);

	bp = logrec.data;
	memcpy(bp, &rectype, sizeof(rectype));
	bp += sizeof(rectype);
	memcpy(bp, &txn_num, sizeof(txn_num));
	bp += sizeof(txn_num);
	memcpy(bp, lsnp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	memcpy(bp, &opcode, sizeof(opcode));
	bp += sizeof(opcode);
	if (name == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &name->size, sizeof(name->size));
		bp += sizeof(name->size);
		memcpy(bp, name->data, name->size);
		bp += name->size;
	}
	if (uid == NULL) {
		zero = 0;
		memcpy(bp, &zero, sizeof(u_int32_t));
		bp += sizeof(u_int32_t);
	} else {
		memcpy(bp, &uid->size, sizeof(uid->size));
		bp += sizeof(uid->size);
		memcpy(bp, uid->data, uid->size);
		bp += uid->size;
	}
	memcpy(bp, &fileid, sizeof(fileid));
	bp += sizeof(fileid);
	memcpy(bp, &ftype, sizeof(ftype));
	bp += sizeof(ftype);
	memcpy(bp, &meta_pgno, sizeof(meta_pgno));
	bp += sizeof(meta_pgno);
	DB_ASSERT((u_int32_t)(bp - (u_int8_t *)logrec.data) == logrec.size);
	ret = CDB___log_put(dbenv, ret_lsnp, (DBT *)&logrec, flags);
	if (txnid != NULL)
		txnid->last_lsn = *ret_lsnp;
	CDB___os_free(logrec.data, logrec.size);
	return (ret);
}

int
CDB___log_register_print(dbenv, dbtp, lsnp, notused2, notused3)
	DB_ENV *dbenv;
	DBT *dbtp;
	DB_LSN *lsnp;
	db_recops notused2;
	void *notused3;
{
	__log_register_args *argp;
	u_int32_t i;
	u_int ch;
	int ret;

	i = 0;
	ch = 0;
	notused2 = 0;
	notused3 = NULL;

	if ((ret = CDB___log_register_read(dbenv, dbtp->data, &argp)) != 0)
		return (ret);
	printf("[%lu][%lu]CDB_log_register: rec: %lu txnid %lx prevlsn [%lu][%lu]\n",
	    (u_long)lsnp->file,
	    (u_long)lsnp->offset,
	    (u_long)argp->type,
	    (u_long)argp->txnid->txnid,
	    (u_long)argp->prev_lsn.file,
	    (u_long)argp->prev_lsn.offset);
	printf("\topcode: %lu\n", (u_long)argp->opcode);
	printf("\tname: ");
	for (i = 0; i < argp->name.size; i++) {
		ch = ((u_int8_t *)argp->name.data)[i];
		if (isprint(ch) || ch == 0xa)
			putchar(ch);
		else
			printf("%#x ", ch);
	}
	printf("\n");
	printf("\tuid: ");
	for (i = 0; i < argp->uid.size; i++) {
		ch = ((u_int8_t *)argp->uid.data)[i];
		if (isprint(ch) || ch == 0xa)
			putchar(ch);
		else
			printf("%#x ", ch);
	}
	printf("\n");
	printf("\tfileid: %ld\n", (long)argp->fileid);
	printf("\tftype: 0x%lx\n", (u_long)argp->ftype);
	printf("\tmeta_pgno: %lu\n", (u_long)argp->meta_pgno);
	printf("\n");
	CDB___os_free(argp, 0);
	return (0);
}

int
CDB___log_register_read(dbenv, recbuf, argpp)
	DB_ENV *dbenv;
	void *recbuf;
	__log_register_args **argpp;
{
	__log_register_args *argp;
	u_int8_t *bp;
	int ret;

	ret = CDB___os_malloc(dbenv, sizeof(__log_register_args) +
	    sizeof(DB_TXN), NULL, &argp);
	if (ret != 0)
		return (ret);
	argp->txnid = (DB_TXN *)&argp[1];
	bp = recbuf;
	memcpy(&argp->type, bp, sizeof(argp->type));
	bp += sizeof(argp->type);
	memcpy(&argp->txnid->txnid,  bp, sizeof(argp->txnid->txnid));
	bp += sizeof(argp->txnid->txnid);
	memcpy(&argp->prev_lsn, bp, sizeof(DB_LSN));
	bp += sizeof(DB_LSN);
	memcpy(&argp->opcode, bp, sizeof(argp->opcode));
	bp += sizeof(argp->opcode);
	memset(&argp->name, 0, sizeof(argp->name));
	memcpy(&argp->name.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->name.data = bp;
	bp += argp->name.size;
	memset(&argp->uid, 0, sizeof(argp->uid));
	memcpy(&argp->uid.size, bp, sizeof(u_int32_t));
	bp += sizeof(u_int32_t);
	argp->uid.data = bp;
	bp += argp->uid.size;
	memcpy(&argp->fileid, bp, sizeof(argp->fileid));
	bp += sizeof(argp->fileid);
	memcpy(&argp->ftype, bp, sizeof(argp->ftype));
	bp += sizeof(argp->ftype);
	memcpy(&argp->meta_pgno, bp, sizeof(argp->meta_pgno));
	bp += sizeof(argp->meta_pgno);
	*argpp = argp;
	return (0);
}

int
CDB___log_init_print(dbenv)
	DB_ENV *dbenv;
{
	int ret;

	if ((ret = CDB___db_add_recovery(dbenv,
	    CDB___log_register1_print, DB_log_register1)) != 0)
		return (ret);
	if ((ret = CDB___db_add_recovery(dbenv,
	    CDB___log_register_print, DB_log_register)) != 0)
		return (ret);
	return (0);
}

int
CDB___log_init_recover(dbenv)
	DB_ENV *dbenv;
{
	int ret;

	if ((ret = CDB___db_add_recovery(dbenv,
	    CDB___deprecated_recover, DB_log_register1)) != 0)
		return (ret);
	if ((ret = CDB___db_add_recovery(dbenv,
	    CDB___log_register_recover, DB_log_register)) != 0)
		return (ret);
	return (0);
}

