/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System calls
 *
 * Copyright 2012, 2017 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Aleksander Kaminski, Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include HAL
#include "../include/errno.h"
#include "../include/sysinfo.h"
#include "../include/fcntl.h"
#include "../include/mman.h"
#include "../include/syscalls.h"
#include "../include/poll.h"
#include "../include/socket.h"
#include "lib/lib.h"
#include "proc/proc.h"
#include "proc/event.h"
#include "vm/object.h"

#define SYSCALLS_NAME(name) syscalls_##name,
#define SYSCALLS_STRING(name) #name,

/*
 * Kernel
 */


void syscalls_debug(void *ustack)
{
	char *s;

	GETFROMSTACK(ustack, char *, s, 0);
	hal_consolePrint(ATTR_USER, s);
}


/*
 * Memory management
 */

int syscalls_memMap(void *ustack)
{
	void **vaddr;
	size_t size;
	int prot, flags, fd;
	off_t offs;
	iodes_t *file;
	vm_object_t *o;
	int err;

	GETFROMSTACK(ustack, void **, vaddr, 0);
	GETFROMSTACK(ustack, size_t, size, 1);
	GETFROMSTACK(ustack, int, prot, 2);
	GETFROMSTACK(ustack, int, flags, 3);
	GETFROMSTACK(ustack, int, fd, 4);
	GETFROMSTACK(ustack, off_t, offs, 5);

	if (flags & MAP_ANONYMOUS) {
		o = NULL;
	}
	else if (fd == FD_PHYSMEM) {
		o = (void *)-1;
	}
	else if (fd == FD_CONTIGUOUS) {
		if ((err = vm_objectContiguous(&o, size)) < 0)
			return err;
	}
	else if ((file = file_get(proc_current()->process, fd)) != NULL) {
		err = vm_objectGet(&o, file);
		file_put(file);

		if (err != EOK)
			return err;
	}
	else {
		return -EBADF;
	}

	if ((*vaddr = vm_mmap(proc_current()->process->mapp, *vaddr, NULL, size, PROT_USER | prot, o, (o == NULL) ? -1 : offs, flags)) == NULL) {
		*vaddr = (void *)-1;
		err = -ENOMEM; /* TODO: get real error from mmap */
	}

	vm_objectPut(o);
	return EOK;
}


void syscalls_memUnmap(void *ustack)
{
	void *vaddr;
	size_t size;

	GETFROMSTACK(ustack, void *, vaddr, 0);
	GETFROMSTACK(ustack, size_t, size, 1);

	vm_munmap(proc_current()->process->mapp, vaddr, size);
}


/*
 * Process management
 */


int syscalls_vforksvc(void *ustack)
{
	return proc_vfork();
}


int syscalls_ProcFork(void *ustack)
{
	return proc_fork();
}


int syscalls_release(void *ustack)
{
	return proc_release();
}


int syscalls_sys_spawn(void *ustack)
{
	char *path;
	char **argv;
	char **envp;

	GETFROMSTACK(ustack, char *, path, 0);
	GETFROMSTACK(ustack, char **, argv, 1);
	GETFROMSTACK(ustack, char **, envp, 2);

	return proc_fileSpawn(path, argv, envp);
}


int syscalls_ProcExec(void *ustack)
{
	int dirfd;
	char *path;
	char **argv;
	char **envp;

	GETFROMSTACK(ustack, int, dirfd, 0);
	GETFROMSTACK(ustack, char *, path, 1);
	GETFROMSTACK(ustack, char **, argv, 2);
	GETFROMSTACK(ustack, char **, envp, 3);

	return proc_exec(dirfd, path, argv, envp);
}


int syscalls_ProcExit(void *ustack)
{
	int code;

	GETFROMSTACK(ustack, int, code, 0);

	proc_exit(code);
	return EOK;
}


int syscalls_ProcWait(void *ustack)
{
	pid_t pid;
	int *stat, options;

	GETFROMSTACK(ustack, pid_t, pid, 0);
	GETFROMSTACK(ustack, int *, stat, 1);
	GETFROMSTACK(ustack, int, options, 2);

	return proc_waitpid(pid, stat, options);
}


int syscalls_threadJoin(void *ustack)
{
	time_t timeout;

	GETFROMSTACK(ustack, time_t, timeout, 0);

	return proc_join(timeout);
}


int syscalls_getpid(void *ustack)
{
	return proc_current()->process->id;
}


int syscalls_getppid(void *ustack)
{
	return proc_current()->process->ppid;
}


/*
 * Thread management
 */


int syscalls_gettid(void *ustack)
{
	return (int)proc_current()->id;
}


int syscalls_beginthreadex(void *ustack)
{
	void (*start)(void *);
	unsigned int priority, stacksz;
	void *stack, *arg;
	unsigned int *id;
	process_t *p;

	GETFROMSTACK(ustack, void *, start, 0);
	GETFROMSTACK(ustack, unsigned int, priority, 1);
	GETFROMSTACK(ustack, void *, stack, 2);
	GETFROMSTACK(ustack, unsigned int, stacksz, 3);
	GETFROMSTACK(ustack, void *, arg, 4);
	GETFROMSTACK(ustack, unsigned int *, id, 5);

	if ((p = proc_current()->process) != NULL)
		proc_get(p);

	return proc_threadCreate(p, start, id, priority, SIZE_KSTACK, stack, stacksz, arg);
}


int syscalls_endthread(void *ustack)
{
	proc_threadEnd();
	return EOK;
}


int syscalls_usleep(void *ustack)
{
	unsigned int us;

	GETFROMSTACK(ustack, unsigned int, us, 0);
	return proc_threadSleep((unsigned long long)us);
}


int syscalls_priority(void *ustack)
{
	int priority;
	thread_t *thread;

	GETFROMSTACK(ustack, int, priority, 0);

	thread = proc_current();

	if (priority == -1)
		return thread->priority;
	else if (priority >= 0 && priority <= 7)
		return thread->priority = priority;

	return -EINVAL;
}


/*
 * System state info
 */


int syscalls_threadsinfo(void *ustack)
{
	int n;
	threadinfo_t *info;

	GETFROMSTACK(ustack, int, n, 0);
	GETFROMSTACK(ustack, threadinfo_t *, info, 1);

	n = proc_threadsList(n, info);

	return n;
}


void syscalls_meminfo(void *ustack)
{
	meminfo_t *info;

	GETFROMSTACK(ustack, meminfo_t *, info, 0);

	vm_meminfo(info);
}


int syscalls_syspageprog(void *ustack)
{
	int i;
	syspageprog_t *prog;

	GETFROMSTACK(ustack, syspageprog_t *, prog, 0);
	GETFROMSTACK(ustack, int, i, 1);

	if (i < 0)
		return syspage->progssz;

	if (i >= syspage->progssz)
		return -EINVAL;

	prog->addr = syspage->progs[i].start;
	prog->size = syspage->progs[i].end - syspage->progs[i].start;
	hal_memcpy(prog->name, syspage->progs[i].cmdline, sizeof(syspage->progs[i].cmdline));

	return EOK;
}


int syscalls_perf_start(void *ustack)
{
	unsigned pid;

	GETFROMSTACK(ustack, unsigned, pid, 0);

	return perf_start(pid);
}


int syscalls_perf_read(void *ustack)
{
	void *buffer;
	size_t sz;

	GETFROMSTACK(ustack, void *, buffer, 0);
	GETFROMSTACK(ustack, size_t, sz, 1);

	return perf_read(buffer, sz);
}


int syscalls_perf_finish(void *ustack)
{
	return perf_finish();
}

/*
 * Mutexes
 */


int syscalls_mutexCreate(void *ustack)
{
	unsigned int *h;
	int res;

	GETFROMSTACK(ustack, unsigned int *, h, 0);

	if ((res = proc_mutexCreate()) < 0)
		return res;

	*h = res;
	return EOK;
}


int syscalls_phMutexLock(void *ustack)
{
	unsigned int h;

	GETFROMSTACK(ustack, unsigned int, h, 0);
	return proc_mutexLock(h);
}


int syscalls_mutexTry(void *ustack)
{
	unsigned int h;

	GETFROMSTACK(ustack, unsigned int, h, 0);
	return proc_mutexTry(h);
}


int syscalls_mutexUnlock(void *ustack)
{
	unsigned int h;

	GETFROMSTACK(ustack, unsigned int, h, 0);
	return proc_mutexUnlock(h);
}


/*
 * Conditional variables
 */


int syscalls_condCreate(void *ustack)
{
	unsigned int *h;
	int res;

	GETFROMSTACK(ustack, unsigned int *, h, 0);

	if ((res = proc_condCreate()) < 0)
		return res;

	*h = res;
	return EOK;
}


int syscalls_phCondWait(void *ustack)
{
	unsigned int h;
	unsigned int m;
	time_t timeout;

	GETFROMSTACK(ustack, unsigned int, h, 0);
	GETFROMSTACK(ustack, unsigned int, m, 1);
	GETFROMSTACK(ustack, time_t, timeout, 2);

	return proc_condWait(h, m, timeout);
}


int syscalls_condSignal(void *ustack)
{
	unsigned int h;

	GETFROMSTACK(ustack, unsigned int, h, 0);
	return proc_condSignal(h);
}


int syscalls_condBroadcast(void *ustack)
{
	unsigned int h;

	GETFROMSTACK(ustack, unsigned int, h, 0);
	return proc_condBroadcast(h);
}


/*
 * Resources
 */


int syscalls_resourceDestroy(void *ustack)
{
	unsigned int h;

	GETFROMSTACK(ustack, unsigned int, h, 0);
	return proc_resourceDestroy(proc_current()->process, h);
}


/*
 * Interrupt management
 */



int syscalls_interrupt(void *ustack)
{
	unsigned int n;
	void *f;
	void *data;
	unsigned int cond;
	unsigned int *handle;
	int res;

	GETFROMSTACK(ustack, unsigned int, n, 0);
	GETFROMSTACK(ustack, void *, f, 1);
	GETFROMSTACK(ustack, void *, data, 2);
	GETFROMSTACK(ustack, unsigned int, cond, 3);
	GETFROMSTACK(ustack, unsigned int *, handle, 4);

	if ((res = userintr_setHandler(n, f, data, cond)) < 0)
		return res;

	*handle = res;
	return EOK;
}


/*
 * Message passing
 */


int syscalls_portCreate(void *ustack)
{
	u32 port;

	GETFROMSTACK(ustack, u32, port, 0);

	return proc_portCreate(port);
}


int syscalls_portGet(void *ustack)
{
	u32 port;

	GETFROMSTACK(ustack, u32, port, 0);

	return proc_portGet(port);
}


u32 syscalls_portRegister(void *ustack)
{
#if 0
	unsigned int port;
	char *name;
	oid_t *oid;

	GETFROMSTACK(ustack, unsigned int, port, 0);
	GETFROMSTACK(ustack, char *, name, 1);
	GETFROMSTACK(ustack, oid_t *, oid, 2);
#endif
	return -ENOSYS; //proc_portRegister(port, name, oid);
}


int syscalls_portEvent(char *ustack)
{
	int porth;
	id_t id;
	int events;

	GETFROMSTACK(ustack, int, porth, 0);
	GETFROMSTACK(ustack, id_t, id, 1);
	GETFROMSTACK(ustack, int, events, 2);

	return proc_event(porth, id, events);
}


int syscalls_msgSend(void *ustack)
{
	int porth;
	msg_t *msg;

	GETFROMSTACK(ustack, int, porth, 0);
	GETFROMSTACK(ustack, msg_t *, msg, 1);

	return proc_msgSend(porth, msg);
}


int syscalls_portRecv(void *ustack)
{
	int porth;
	msg_t *msg;

	GETFROMSTACK(ustack, int, porth, 0);
	GETFROMSTACK(ustack, msg_t *, msg, 1);

	return proc_msgRecv(porth, msg);
}


int syscalls_msgRespond(void *ustack)
{
	int porth;
	msg_t *msg;
	unsigned int msgh;
	int error;

	GETFROMSTACK(ustack, int, porth, 0);
	GETFROMSTACK(ustack, int, error, 1);
	GETFROMSTACK(ustack, msg_t *, msg, 2);
	GETFROMSTACK(ustack, int, msgh, 3);

	return proc_msgRespond(porth, msgh, error, msg);
}


int syscalls_lookup(void *ustack)
{
#if 0
	char *name;
	oid_t *file, *dev;

	GETFROMSTACK(ustack, char *, name, 0);
	GETFROMSTACK(ustack, oid_t *, file, 1);
	GETFROMSTACK(ustack, oid_t *, dev, 2);
#endif
	return -ENOSYS; //proc_portLookup(name, file, dev);
}


/*
 * Time management
 */


int syscalls_gettime(void *ustack)
{
	time_t *praw, *poffs, raw, offs;

	GETFROMSTACK(ustack, time_t *, praw, 0);
	GETFROMSTACK(ustack, time_t *, poffs, 1);

	proc_gettime(&raw, &offs);

	if (praw != NULL)
		(*praw) = raw;

	if (poffs != NULL)
		(*poffs) = offs;

	return EOK;
}


int syscalls_settime(void *ustack)
{
	time_t offs;

	GETFROMSTACK(ustack, time_t, offs, 0);

	return proc_settime(offs);
}


/*
 * Power management
 */


void syscalls_keepidle(void *ustack)
{
#ifdef CPU_STM32
	int t;

	GETFROMSTACK(ustack, int, t, 0);
	hal_cpuSetDevBusy(t);
#endif
}


/*
 * Memory map dump
 */


void syscalls_mmdump(void *ustack)
{
	vm_mapDump(NULL);
}


/*
 * Platform specific call
 */


int syscalls_platformctl(void *ustack)
{
	void *ptr;
	GETFROMSTACK(ustack, void *, ptr, 0);
	return hal_platformctl(ptr);
}


/*
 * Watchdog
 */


void syscalls_wdgreload(void *ustack)
{
	hal_wdgReload();
}


addr_t syscalls_va2pa(void *ustack)
{
	void *va;

	GETFROMSTACK(ustack, void *, va, 0);

	return (pmap_resolve(&proc_current()->process->mapp->pmap, (void *)((unsigned long)va & ~0xfff)) & ~0xfff) + ((unsigned long)va & 0xfff);
}


int syscalls_sys_sigaction(void *ustack)
{
	int sig;
	const struct sigaction *act;
	struct sigaction *oact;

	GETFROMSTACK(ustack, int, sig, 0);
	GETFROMSTACK(ustack, const struct sigaction *, act, 1);
	GETFROMSTACK(ustack, struct sigaction *, oact, 2);

	return proc_sigaction(sig, act, oact);
}


void syscalls_signalHandle(void *ustack)
{
	void *handler;
	unsigned mask, mmask;
	thread_t *thread;

	GETFROMSTACK(ustack, void *, handler, 0);
	GETFROMSTACK(ustack, unsigned, mask, 1);
	GETFROMSTACK(ustack, unsigned, mmask, 2);

	thread = proc_current();
	thread->process->sigmask = (mask & mmask) | (thread->process->sigmask & ~mmask);
	thread->process->sigtrampoline = handler;
}


int syscalls_signalPost(void *ustack)
{
	int pid, tid, signal, err;
	process_t *proc;
	thread_t *t = NULL;

	GETFROMSTACK(ustack, int, pid, 0);
	GETFROMSTACK(ustack, int, tid, 1);
	GETFROMSTACK(ustack, int, signal, 2);

	if ((proc = proc_find(pid)) == NULL)
		return -EINVAL;

	if (tid >= 0 && (t = threads_findThread(tid)) == NULL) {
		proc_put(proc);
		return -EINVAL;
	}

	if (t != NULL && t->process != proc) {
		proc_put(proc);
		threads_put(t);
		return -EINVAL;
	}

	err = threads_sigpost(proc, t, signal);

	proc_put(proc);
	threads_put(t);
	hal_cpuReschedule(NULL);
	return err;
}


unsigned int syscalls_signalMask(void *ustack)
{
	unsigned mask, mmask, old;
	thread_t *t;

	GETFROMSTACK(ustack, unsigned, mask, 0);
	GETFROMSTACK(ustack, unsigned, mmask, 1);

	t = proc_current();

	old = t->sigmask;
	t->sigmask = (mask & mmask) | (t->sigmask & ~mmask);

	return old;
}


int syscalls_signalSuspend(void *ustack)
{
	unsigned int mask, old;
	int ret = 0;
	thread_t *t;

	GETFROMSTACK(ustack, unsigned, mask, 0);

	t = proc_current();

	old = t->sigmask;
	t->sigmask = mask;

	while (ret != -EINTR)
		ret = proc_threadSleep(1ULL << 52);
	t->sigmask = old;

	return ret;
}

/* POSIX compatibility syscalls */
int syscalls_SetRoot(char *ustack)
{
	int port;
	id_t id;
	mode_t mode;
	GETFROMSTACK(ustack, int, port, 0);
	GETFROMSTACK(ustack, id_t, id, 1);
	GETFROMSTACK(ustack, mode_t, mode, 2);
	return proc_filesSetRoot(port, id, mode);
}


int syscalls_sys_openat(char *ustack)
{
	const char *filename;
	int flags, dirfd;
	mode_t mode;

	GETFROMSTACK(ustack, int, dirfd, 0);
	GETFROMSTACK(ustack, const char *, filename, 1);
	GETFROMSTACK(ustack, int, flags, 2);
	GETFROMSTACK(ustack, mode_t, mode, 3);

	return proc_fileOpen(dirfd, filename, flags, mode);
}


int syscalls_sys_open(char *ustack)
{
	const char *filename;
	int flags;
	mode_t mode;

	GETFROMSTACK(ustack, const char *, filename, 0);
	GETFROMSTACK(ustack, int, flags, 1);
	GETFROMSTACK(ustack, mode_t, mode, 2);

	return proc_fileOpen(AT_FDCWD, filename, flags, mode);
}


int syscalls_sys_close(char *ustack)
{
	int fildes;

	GETFROMSTACK(ustack, int, fildes, 0);

	return proc_fileClose(fildes);
}


int syscalls_fileRead(char *ustack)
{
	int fildes;
	void *buf;
	size_t nbyte;
	off_t *offset;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, void *, buf, 1);
	GETFROMSTACK(ustack, size_t, nbyte, 2);
	GETFROMSTACK(ustack, off_t, *offset, 3);

	return proc_fileRead(fildes, buf, nbyte, offset);
}


int syscalls_fileWrite(char *ustack)
{
	int fildes;
	void *buf;
	size_t nbyte;
	off_t *offset;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, void *, buf, 1);
	GETFROMSTACK(ustack, size_t, nbyte, 2);
	GETFROMSTACK(ustack, off_t, *offset, 3);

	return proc_fileWrite(fildes, buf, nbyte, offset);
}


int syscalls_sys_dup3(char *ustack)
{
	int fildes, fildes2, flags;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, int, fildes2, 1);
	GETFROMSTACK(ustack, int, flags, 2);

	return proc_fileDup(fildes, fildes2, flags);
}


int syscalls_fileLink(char *ustack)
{
	int fildes, dirfd, flags;
	const char *name, *path;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, const char *, path, 1);
	GETFROMSTACK(ustack, int, dirfd, 2);
	GETFROMSTACK(ustack, const char *, name, 3);
	GETFROMSTACK(ustack, int, flags, 4);

	return proc_fileLink(fildes, path, dirfd, name, flags);
}


int syscalls_fileUnlink(char *ustack)
{
	const char *path;
	int dirfd, flags;

	GETFROMSTACK(ustack, int, dirfd, 0);
	GETFROMSTACK(ustack, const char *, path, 2);
	GETFROMSTACK(ustack, int, flags, 3);

	return proc_fileUnlink(dirfd, path, flags);
}


int syscalls_fileSeek(char *ustack)
{
	int fildes;
	off_t *offset;
	int whence;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, off_t *, offset, 1);
	GETFROMSTACK(ustack, int, whence, 2);

	return proc_fileSeek(fildes, offset, whence);
}


int syscalls_sys_ftruncate(char *ustack)
{
	int fildes;
	off_t length;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, off_t, length, 1);

	return proc_fileTruncate(fildes, length);
}


int syscalls_sys_fcntl(char *ustack)
{
	int fildes, cmd;
	long arg;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, int, cmd, 1);
	GETFROMSTACK(ustack, long, arg, 2);

	return proc_fileControl(fildes, cmd, arg);
}


int syscalls_fileStat(char *ustack)
{
	int fildes, flags;
	file_stat_t *buf;
	const char *path;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, const char *, path, 1);
	GETFROMSTACK(ustack, file_stat_t *, buf, 2);
	GETFROMSTACK(ustack, int, flags, 3);

	return proc_fileStat(fildes, path, buf, flags);
}


int syscalls_sys_fchmod(char *ustack)
{
	int fildes;
	mode_t mode;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, mode_t, mode, 1);

	return proc_fileChmod(fildes, mode);
}


int syscalls_procChangeDir(char *ustack)
{
	int fildes;
	const char *path;
	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, const char *, path, 1);
	return proc_changeDir(fildes, path);
}


int syscalls_fileIoctl(char *ustack)
{
	int fildes;
	unsigned long request;
	char *outdata;
	const char *indata;
	size_t insz, outsz;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, unsigned long, request, 1);
	GETFROMSTACK(ustack, const char *, indata, 2);
	GETFROMSTACK(ustack, size_t, insz, 3);
	GETFROMSTACK(ustack, char *, outdata, 4);
	GETFROMSTACK(ustack, size_t, outsz, 5);

	return proc_fileIoctl(fildes, request, indata, insz, outdata, outsz);
}


int syscalls_sys_poll(char *ustack)
{
	struct pollfd *fds;
	nfds_t nfds;
	int timeout_ms;

	GETFROMSTACK(ustack, struct pollfd *, fds, 0);
	GETFROMSTACK(ustack, nfds_t, nfds, 1);
	GETFROMSTACK(ustack, int, timeout_ms, 2);

	return proc_poll(fds, nfds, timeout_ms);
}


int syscalls_eventRegister(char *ustack)
{
	const oid_t *oid;
	unsigned types;

	GETFROMSTACK(ustack, const oid_t *, oid, 0);
	GETFROMSTACK(ustack, unsigned, types, 1);

	return -ENOSYS;
}


int syscalls_queueCreate(char *ustack)
{
	return -ENOSYS;
}


int syscalls_queueWait(char *ustack)
{
#if 0
	int fd, subcnt, evcnt;
	time_t timeout;
	const struct _event_t *subs;
	struct _event_t *events;

	GETFROMSTACK(ustack, int, fd, 0);
	GETFROMSTACK(ustack, const struct _event_t *, subs, 1);
	GETFROMSTACK(ustack, int, subcnt, 2);
	GETFROMSTACK(ustack, struct _event_t *, events, 3);
	GETFROMSTACK(ustack, int, evcnt, 4);
	GETFROMSTACK(ustack, time_t, timeout, 5);
#endif
	return -ENOSYS;
}


int syscalls_sys_pipe(char *ustack)
{
	int *fds;
	int flags;

	GETFROMSTACK(ustack, int *, fds, 0);
	GETFROMSTACK(ustack, int, flags, 1);
	return proc_pipeCreate(fds, flags);
}


int syscalls_fifoCreate(char *ustack)
{
	int dirfd;
	const char *path;
	mode_t mode;

	GETFROMSTACK(ustack, int, dirfd, 0);
	GETFROMSTACK(ustack, const char *, path, 1);
	GETFROMSTACK(ustack, mode_t, mode, 2);
	return proc_fifoCreate(dirfd, path, mode);
}


int syscalls_deviceCreate(char *ustack)
{
	int dirfd, portfd;
	id_t id;
	const char *path;
	mode_t mode;

	GETFROMSTACK(ustack, int, dirfd, 0);
	GETFROMSTACK(ustack, const char *, path, 1);
	GETFROMSTACK(ustack, int, portfd, 2);
	GETFROMSTACK(ustack, id_t, id, 3);
	GETFROMSTACK(ustack, mode_t, mode, 4);

	return proc_deviceCreate(dirfd, path, portfd, id, mode);
}


int syscalls_fsMount(char *ustack)
{
	const char *type, *devpath;
	int devfd;
	unsigned int port;

	GETFROMSTACK(ustack, const char *, type, 0);
	GETFROMSTACK(ustack, int, devfd, 1);
	GETFROMSTACK(ustack, const char *, devpath, 2);
	GETFROMSTACK(ustack, unsigned int, port, 3);

	return proc_fsMount(devfd, devpath, type, port);
}


int syscalls_fsBind(char *ustack)
{
	int dirfd, fsfd;
	const char *dirpath, *fspath;

	GETFROMSTACK(ustack, int, dirfd, 0);
	GETFROMSTACK(ustack, const char *, dirpath, 1);
	GETFROMSTACK(ustack, int, fsfd, 2);
	GETFROMSTACK(ustack, const char *, fspath, 3);

	return proc_fsBind(dirfd, dirpath, fsfd, fspath);
}


int syscalls_sys_accept4(char *ustack)
{
	int socket;
	struct sockaddr *address;
	socklen_t *address_len;
	int flags;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, struct sockaddr *, address, 1);
	GETFROMSTACK(ustack, socklen_t *,address_len, 2);
	GETFROMSTACK(ustack, int, flags, 3);

	return proc_netAccept4(socket, address, address_len, flags);
}


int syscalls_sys_bind(char *ustack)
{
	int socket;
	const struct sockaddr *address;
	socklen_t address_len;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, const struct sockaddr *, address, 1);
	GETFROMSTACK(ustack, socklen_t, address_len, 2);

	return proc_netBind(socket, address, address_len);
}


int syscalls_sys_connect(char *ustack)
{
	int socket;
	const struct sockaddr *address;
	socklen_t address_len;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, const struct sockaddr *, address, 1);
	GETFROMSTACK(ustack, socklen_t, address_len, 2);

	return proc_netConnect(socket, address, address_len);
}


int syscalls_sys_getpeername(char *ustack)
{
	int socket;
	struct sockaddr *address;
	socklen_t *address_len;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, struct sockaddr *, address, 1);
	GETFROMSTACK(ustack, socklen_t *, address_len, 2);

	return proc_netGetpeername(socket, address, address_len);
}


int syscalls_sys_getsockname(char *ustack)
{
	int socket;
	struct sockaddr *address;
	socklen_t *address_len;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, struct sockaddr *, address, 1);
	GETFROMSTACK(ustack, socklen_t *, address_len, 2);

	return proc_netGetsockname(socket, address, address_len);
}


int syscalls_sys_getsockopt(char *ustack)
{
	int socket;
	int level;
	int optname;
	void *optval;
	socklen_t *optlen;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, int, level, 1);
	GETFROMSTACK(ustack, int, optname, 2);
	GETFROMSTACK(ustack, void *, optval, 3);
	GETFROMSTACK(ustack, socklen_t *, optlen, 4);

	return proc_netGetsockopt(socket, level, optname, optval, optlen);
}


int syscalls_sys_listen(char *ustack)
{
	int socket;
	int backlog;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, int, backlog, 1);

	return proc_netListen(socket, backlog);
}


ssize_t syscalls_sys_recvmsg(char *ustack)
{
	int socket, flags;
	struct msghdr *msg;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, struct msghdr *, msg, 1);
	GETFROMSTACK(ustack, int, flags, 2);

	return proc_recvmsg(socket, msg, flags);
}


ssize_t syscalls_sys_sendmsg(char *ustack)
{
	int socket, flags;
	struct msghdr *msg;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, struct msghdr *, msg, 1);
	GETFROMSTACK(ustack, int, flags, 2);

	return proc_sendmsg(socket, msg, flags);
}


ssize_t syscalls_sys_recvfrom(char *ustack)
{
	return -ENOSYS; //proc_netRecvfrom(socket, message, length, flags, src_addr, src_len);
}


ssize_t syscalls_sys_sendto(char *ustack)
{
	return -ENOSYS; //proc_netSendto(socket, message, length, flags, dest_addr, dest_len);
}


int syscalls_sys_socket(char *ustack)
{
	int domain;
	int type;
	int protocol;

	GETFROMSTACK(ustack, int, domain, 0);
	GETFROMSTACK(ustack, int, type, 1);
	GETFROMSTACK(ustack, int, protocol, 2);

	return proc_netSocket(domain, type, protocol);
}


int syscalls_sys_socketPair(char *ustack)
{
	int domain;
	int type;
	int protocol;
	int flags;
	int *sv;

	GETFROMSTACK(ustack, int, domain, 0);
	GETFROMSTACK(ustack, int, type, 1);
	GETFROMSTACK(ustack, int, protocol, 2);
	GETFROMSTACK(ustack, int, flags, 3);
	GETFROMSTACK(ustack, int *, sv, 4);

	return sun_pair(domain, type, protocol, flags, sv);
}


int syscalls_sys_shutdown(char *ustack)
{
	int socket;
	int how;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, int, how, 1);

	return proc_netShutdown(socket, how);
}


int syscalls_sys_setsockopt(char *ustack)
{
	int socket;
	int level;
	int optname;
	const void *optval;
	socklen_t optlen;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, int, level, 1);
	GETFROMSTACK(ustack, int, optname, 2);
	GETFROMSTACK(ustack, const void *, optval, 3);
	GETFROMSTACK(ustack, socklen_t, optlen, 4);

	return proc_netSetsockopt(socket, level, optname, optval, optlen);
}


int syscalls_sys_setpgid(char *ustack)
{
	pid_t pid, pgid;
	GETFROMSTACK(ustack, pid_t, pid, 0);
	GETFROMSTACK(ustack, pid_t, pgid, 1);
	return proc_setpgid(proc_current()->process, pid, pgid);
}


pid_t syscalls_sys_getpgid(char *ustack)
{
	pid_t pid;
	GETFROMSTACK(ustack, pid_t, pid, 0);
	return proc_getpgid(proc_current()->process, pid);
}


pid_t syscalls_sys_setsid(char *ustack)
{
	return proc_setsid(proc_current()->process);
}


pid_t syscalls_sys_getsid(char *ustack)
{
	pid_t pid;
	GETFROMSTACK(ustack, pid_t, pid, 0);
	return proc_getsid(proc_current()->process, pid);
}


int syscalls_threadKill(char *ustack)
{
	pid_t pid;
	int tid;
	int sig;

	GETFROMSTACK(ustack, pid_t, pid, 0);
	GETFROMSTACK(ustack, int, tid, 1);
	GETFROMSTACK(ustack, int, sig, 2);

	return proc_threadKill(pid, tid, sig);
}


/*
 * Empty syscall
 */


int syscalls_notimplemented(void)
{
	return -ENOTTY;
}


const void * const syscalls[] = { SYSCALLS(SYSCALLS_NAME) };
const char * const syscall_strings[] = { SYSCALLS(SYSCALLS_STRING) };


void *syscalls_dispatch(int n, char *ustack)
{
	void *retval;
	thread_t *current;

	if (n >= sizeof(syscalls) / sizeof(syscalls[0]))
		return (void *)-EINVAL;

	retval = ((void *(*)(char *))syscalls[n])(ustack);

	current = proc_current();

	while (current->exit || current->stop) {
		if (current->exit)
			proc_threadEnd();
		else if (current->stop)
			proc_threadStop();
	}

	return retval;
}


void _syscalls_init(void)
{
	lib_printf("syscalls: Initializing syscall table [%d]\n", sizeof(syscalls) / sizeof(syscalls[0]));
}
