/*
 * high-performance allocator with fine-grained SHM locking
 *   (note: may perform worse than F_MALLOC at low CPS values!)
 *
 * Copyright (C) 2019 OpenSIPS Solutions
 *
 * This file is part of opensips, a free SIP server.
 *
 * opensips is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * opensips is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/* for any questions on the complex ifdef logic, see mem/f_malloc_dyn.h */

/*
 * the *_split functions try to split a fragment in an attempt
 * to minimize memory usage
 */
#if !defined INLINE_ALLOC && defined DBG_MALLOC
#define pkg_frag_split_dbg(blk, frag, sz, fl, fnc, ln) \
	do { \
		if (can_split_frag(frag, sz)) { \
			__pkg_frag_split_dbg(blk, frag, sz, fl, fnc, ln); \
			update_stats_pkg_frag_split(blk); \
		} \
	} while (0)
#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
#define pkg_frag_split(blk, frag, sz) \
	do { \
		if (can_split_frag(frag, sz)) { \
			__pkg_frag_split(blk, frag, sz); \
			update_stats_pkg_frag_split(blk); \
		} \
	} while (0)
#else
#define pkg_frag_split(blk, frag, sz, fl, fnc, ln) \
	do { \
		if (can_split_frag(frag, sz)) { \
			__pkg_frag_split(blk, frag, sz, fl, fnc, ln); \
			update_stats_pkg_frag_split(blk); \
		} \
	} while (0)
#endif


#if !defined INLINE_ALLOC && defined DBG_MALLOC
#define shm_frag_split_unsafe_dbg(blk, frag, sz, fl, fnc, ln) \
	do { \
		if (can_split_frag(frag, sz)) { \
			__shm_frag_split_unsafe_dbg(blk, frag, sz, fl, fnc, ln); \
			if (stats_are_ready()) { \
				update_stats_shm_frag_split(); \
			} else { \
				(blk)->used -= FRAG_OVERHEAD; \
				(blk)->real_used += FRAG_OVERHEAD; \
				(blk)->total_fragments++; \
			} \
		} \
	} while (0)
#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
#define shm_frag_split_unsafe(blk, frag, sz) \
	do { \
		if (can_split_frag(frag, sz)) { \
			__shm_frag_split_unsafe(blk, frag, sz); \
			if (stats_are_ready()) { \
				update_stats_shm_frag_split(); \
			} else { \
				(blk)->used -= FRAG_OVERHEAD; \
				(blk)->real_used += FRAG_OVERHEAD; \
				(blk)->total_fragments++; \
			} \
		} \
	} while (0)
#else
#define shm_frag_split_unsafe(blk, frag, sz, fl, fnc, ln) \
	do { \
		if (can_split_frag(frag, sz)) { \
			__shm_frag_split_unsafe(blk, frag, sz, fl, fnc, ln); \
			if (stats_are_ready()) { \
				update_stats_shm_frag_split(); \
			} else { \
				(blk)->used -= FRAG_OVERHEAD; \
				(blk)->real_used += FRAG_OVERHEAD; \
				(blk)->total_fragments++; \
			} \
		} \
	} while (0)
#endif


/* Note: the shm lock on "hash" must be acquired when this is called */
#if !defined INLINE_ALLOC && defined DBG_MALLOC
#define shm_frag_split_dbg(blk, frag, sz, hash, fl, fnc, ln) \
	do { \
		if (can_split_frag(frag, sz)) { \
			__shm_frag_split_dbg(blk, frag, sz, hash, fl, fnc, ln); \
			update_stats_shm_frag_split(); \
		} \
	} while (0)
#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
#define shm_frag_split(blk, frag, sz, hash) \
	do { \
		if (can_split_frag(frag, sz)) { \
			__shm_frag_split(blk, frag, sz, hash); \
			update_stats_shm_frag_split(); \
		} \
	} while (0)
#else
#define shm_frag_split(blk, frag, sz, hash, fl, fnc, ln) \
	do { \
		if (can_split_frag(frag, sz)) { \
			__shm_frag_split(blk, frag, sz, hash, fl, fnc, ln); \
			update_stats_shm_frag_split(); \
		} \
	} while (0)
#endif


#if !defined INLINE_ALLOC && defined DBG_MALLOC
void __pkg_frag_split_dbg(struct hp_block *hpb, struct hp_frag *frag,
                          unsigned long size, const char *file,
                          const char *func, unsigned int line)
#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
void __pkg_frag_split(struct hp_block *hpb, struct hp_frag *frag,
                      unsigned long size)
#else
void __pkg_frag_split(struct hp_block *hpb, struct hp_frag *frag,
                      unsigned long size, const char *file,
                      const char *func, unsigned int line)
#endif
{
	unsigned long rest;
	struct hp_frag *n;

	rest = frag->size - size;
	frag->size = size;

	/* split the fragment */
	n = FRAG_NEXT(frag);
	n->size = rest - FRAG_OVERHEAD;

#ifdef DBG_MALLOC
	/* frag created by malloc or realloc, mark it */
	n->file=file;
	n->func=func;
	n->line=line;
#ifndef STATISTICS
	hpb->used -= FRAG_OVERHEAD;
	hpb->real_used += FRAG_OVERHEAD;
	hpb->total_fragments++;
#endif
#endif

	hp_frag_attach(hpb, n);
	update_stats_pkg_frag_attach(hpb, n);

#if defined(DBG_MALLOC) && !defined(STATISTICS)
	hpb->used -= n->size;
	hpb->real_used -= n->size + FRAG_OVERHEAD;
#endif
}

#if !defined INLINE_ALLOC && defined DBG_MALLOC
void __shm_frag_split_unsafe_dbg(struct hp_block *hpb, struct hp_frag *frag,
                                 unsigned long size, const char *file,
                                 const char *func, unsigned int line)
#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
void __shm_frag_split_unsafe(struct hp_block *hpb, struct hp_frag *frag,
							unsigned long size)
#else
void __shm_frag_split_unsafe(struct hp_block *hpb, struct hp_frag *frag,
                             unsigned long size, const char *file,
                             const char *func, unsigned int line)
#endif
{
	unsigned long rest;
	struct hp_frag *n;

#ifdef HP_MALLOC_FAST_STATS
	hpb->free_hash[PEEK_HASH_RR(hpb, frag->size)].total_no--;
	hpb->free_hash[PEEK_HASH_RR(hpb, size)].total_no++;
#endif

	rest = frag->size - size;
	frag->size = size;

	/* split the fragment */
	n = FRAG_NEXT(frag);
	n->size = rest - FRAG_OVERHEAD;

#ifdef DBG_MALLOC
	/* frag created by malloc, mark it*/
	n->file=file;
	n->func=func;
	n->line=line;
#endif

#if defined(DBG_MALLOC) || defined(STATISTICS)
	if (stats_are_ready()) {
		hpb->used -= FRAG_OVERHEAD;
		hpb->real_used += FRAG_OVERHEAD;
		hpb->total_fragments++;
	}
#endif

#ifdef HP_MALLOC_FAST_STATS
	hpb->free_hash[PEEK_HASH_RR(hpb, n->size)].total_no++;
#endif

	hp_frag_attach(hpb, n);

	if (stats_are_ready()) {
		update_stats_shm_frag_attach(n);
#if defined(DBG_MALLOC) || defined(STATISTICS)
		hpb->used -= n->size;
		hpb->real_used -= n->size + FRAG_OVERHEAD;
#endif
	} else {
		hpb->used -= n->size;
		hpb->real_used -= n->size + FRAG_OVERHEAD;
	}
}

 /* size should already be rounded-up */
#if !defined INLINE_ALLOC && defined DBG_MALLOC
void __shm_frag_split_dbg(struct hp_block *hpb, struct hp_frag *frag,
                       unsigned long size, unsigned int old_hash,
                       const char *file, const char *func, unsigned int line)
#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
void __shm_frag_split(struct hp_block *hpb, struct hp_frag *frag,
					 unsigned long size, unsigned int old_hash)
#else
void __shm_frag_split(struct hp_block *hpb, struct hp_frag *frag,
                      unsigned long size, unsigned int old_hash,
                      const char *file, const char *func, unsigned int line)
#endif
{
	unsigned long rest, hash;
	struct hp_frag *n;

#ifdef HP_MALLOC_FAST_STATS
	hpb->free_hash[PEEK_HASH_RR(hpb, frag->size)].total_no--;
	hpb->free_hash[PEEK_HASH_RR(hpb, size)].total_no++;
#endif

	rest = frag->size - size;
	frag->size = size;

	/* split the fragment */
	n = FRAG_NEXT(frag);
	n->size = rest - FRAG_OVERHEAD;

#ifdef DBG_MALLOC
	/* frag created by malloc, mark it*/
	n->file=file;
	n->func=func;
	n->line=line;
#endif

#if defined(DBG_MALLOC) || defined(STATISTICS)
	hpb->used -= FRAG_OVERHEAD;
	hpb->real_used += FRAG_OVERHEAD;
	hpb->total_fragments++;
#endif

	/* insert the newly obtained hp_frag in its free list */
	hash = PEEK_HASH_RR(hpb, n->size);

	if (hash != old_hash)
		SHM_LOCK(hash);

	hp_frag_attach(hpb, n);
	update_stats_shm_frag_attach(n);

#if defined(DBG_MALLOC) || defined(STATISTICS)
	hpb->used -= n->size;
	hpb->real_used -= n->size + FRAG_OVERHEAD;
#endif

#ifdef HP_MALLOC_FAST_STATS
	hpb->free_hash[hash].total_no++;
#endif

	if (hash != old_hash)
		SHM_UNLOCK(hash);
}

#if !defined INLINE_ALLOC && defined DBG_MALLOC
void *hp_pkg_malloc_dbg(struct hp_block *hpb, unsigned long size,
						const char *file, const char *func, unsigned int line)
#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
void *hp_pkg_malloc(struct hp_block *hpb, unsigned long size)
#else
void *hp_pkg_malloc(struct hp_block *hpb, unsigned long size,
						const char *file, const char *func, unsigned int line)
#endif
{
	struct hp_frag *frag;
	unsigned int hash;

	/* size must be a multiple of ROUNDTO */
	size = ROUNDUP(size);

	/* search for a suitable free frag */
	for (hash = GET_HASH(size); hash < HP_HASH_SIZE; hash++) {
		frag = hpb->free_hash[hash].first;

		for (; frag; frag = frag->u.nxt_free)
			if (frag->size >= size)
				goto found;

		/* try in a bigger bucket */
	}

	/* out of memory... we have to shut down */
#if defined(DBG_MALLOC) || defined(STATISTICS)
	LM_ERR(oom_errorf, hpb->name, hpb->size - hpb->real_used, size,
			hpb->name[0] == 'p' ? "M" : "m");
#else
	LM_ERR(oom_nostats_errorf, hpb->name, size,
	       hpb->name[0] == 'p' ? "M" : "m");
#endif
	return NULL;

found:
	hp_frag_detach(hpb, frag);
	update_stats_pkg_frag_detach(hpb, frag);

#ifndef STATISTICS
	hpb->used += frag->size;
	hpb->real_used += frag->size + FRAG_OVERHEAD;
#endif

	/* split the fragment if possible */
	#if !defined INLINE_ALLOC && defined DBG_MALLOC
	pkg_frag_split_dbg(hpb, frag, size, file, "hp_malloc frag", line);
	#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
	pkg_frag_split(hpb, frag, size);
	#else
	pkg_frag_split(hpb, frag, size, file, "hp_malloc frag", line);
	#endif

	#ifdef DBG_MALLOC
	frag->file=file;
	frag->func=func;
	frag->line=line;
	#endif

	if (hpb->real_used > hpb->max_real_used)
		hpb->max_real_used = hpb->real_used;

	pkg_threshold_check();

	return (char *)frag + sizeof *frag;
}

/*
 * although there is a lot of duplicate code, we get the best performance:
 *
 * - the _unsafe version will not be used too much anyway (usually at startup)
 * - hp_shm_malloc is faster (no 3rd parameter, no extra if blocks)
 */
#if !defined INLINE_ALLOC && defined DBG_MALLOC
void *hp_shm_malloc_unsafe_dbg(struct hp_block *hpb, unsigned long size,
                      const char *file, const char *func, unsigned int line)
#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
void *hp_shm_malloc_unsafe(struct hp_block *hpb, unsigned long size)
#else
void *hp_shm_malloc_unsafe(struct hp_block *hpb, unsigned long size,
                      const char *file, const char *func, unsigned int line)
#endif
{
	struct hp_frag *frag;
	unsigned int init_hash, hash, sec_hash;
	int i;

	/* size must be a multiple of ROUNDTO */
	size = ROUNDUP(size);

	/*search for a suitable free frag*/

	for (hash = GET_HASH(size), init_hash = hash; hash < HP_HASH_SIZE; hash++) {
		if (!hpb->free_hash[hash].is_optimized) {
			frag = hpb->free_hash[hash].first;

			for (; frag; frag = frag->u.nxt_free)
				if (frag->size >= size)
					goto found;

		} else {
			/* optimized size. search through its own hash! */
			for (i = 0, sec_hash = HP_HASH_SIZE +
			                       hash * shm_secondary_hash_size +
				                   optimized_get_indexes[hash];
				 i < shm_secondary_hash_size;
				 i++, sec_hash = (sec_hash + 1) % shm_secondary_hash_size) {

				frag = hpb->free_hash[sec_hash].first;
				for (; frag; frag = frag->u.nxt_free)
					if (frag->size >= size) {
						/* free fragments are detached in a simple round-robin manner */
						optimized_get_indexes[hash] =
						    (optimized_get_indexes[hash] + i + 1)
						     % shm_secondary_hash_size;
						hash = sec_hash;
						goto found;
					}
			}
		}

		/* try in a bigger bucket */
	}

	/* out of memory... we have to shut down */
#if defined(DBG_MALLOC) || defined(STATISTICS)
	LM_ERR(oom_errorf, hpb->name, hpb->size - hpb->real_used, size,
			hpb->name[0] == 'p' ? "M" : "m");
#else
	LM_ERR(oom_nostats_errorf, hpb->name, size,
	       hpb->name[0] == 'p' ? "M" : "m");
#endif
	return NULL;

found:
	hp_frag_detach(hpb, frag);

	if (stats_are_ready()) {
		update_stats_shm_frag_detach(frag);
#if defined(DBG_MALLOC) || defined(STATISTICS)
		hpb->used += frag->size;
		hpb->real_used += frag->size + FRAG_OVERHEAD;
#endif
	} else {
		hpb->used += frag->size;
		hpb->real_used += frag->size + FRAG_OVERHEAD;
	}

	/* split the fragment if possible */
	#if !defined INLINE_ALLOC && defined DBG_MALLOC
	shm_frag_split_unsafe_dbg(hpb, frag, size, file, "hp_malloc frag", line);
	#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
	shm_frag_split_unsafe(hpb, frag, size);
	#else
	shm_frag_split_unsafe(hpb, frag, size, file, "hp_malloc frag", line);
	#endif

	#ifdef DBG_MALLOC
	frag->file=file;
	frag->func=func;
	frag->line=line;
	#endif

#ifndef HP_MALLOC_FAST_STATS
	if (stats_are_ready()) {
		unsigned long real_used;

		real_used = get_stat_val(shm_rused);
		if (real_used > hpb->max_real_used)
			hpb->max_real_used = real_used;
	} else
		if (hpb->real_used > hpb->max_real_used)
			hpb->max_real_used = hpb->real_used;
#endif

	if (shm_hash_usage)
		shm_hash_usage[init_hash]++;

	return (char *)frag + sizeof *frag;
}

/*
 * Note: as opposed to hp_shm_malloc_unsafe(),
 *       hp_shm_malloc() assumes that the core statistics are initialized
 */
#if !defined INLINE_ALLOC && defined DBG_MALLOC
void *hp_shm_malloc_dbg(struct hp_block *hpb, unsigned long size,
						const char *file, const char *func, unsigned int line)
#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
void *hp_shm_malloc(struct hp_block *hpb, unsigned long size)
#else
void *hp_shm_malloc(struct hp_block *hpb, unsigned long size,
						const char *file, const char *func, unsigned int line)
#endif
{
	struct hp_frag *frag;
	unsigned int init_hash, hash, sec_hash;
	int i;

	/* size must be a multiple of ROUNDTO */
	size = ROUNDUP(size);

	/*search for a suitable free frag*/

	for (hash = GET_HASH(size), init_hash = hash; hash < HP_HASH_SIZE; hash++) {
		if (!hpb->free_hash[hash].is_optimized) {
			SHM_LOCK(hash);
			frag = hpb->free_hash[hash].first;

			for (; frag; frag = frag->u.nxt_free)
				if (frag->size >= size)
					goto found;

			SHM_UNLOCK(hash);
		} else {
			/* optimized size. search through its own hash! */
			for (i = 0, sec_hash = HP_HASH_SIZE +
			                       hash * shm_secondary_hash_size +
				                   optimized_get_indexes[hash];
				 i < shm_secondary_hash_size;
				 i++, sec_hash = (sec_hash + 1) % shm_secondary_hash_size) {

				SHM_LOCK(sec_hash);
				frag = hpb->free_hash[sec_hash].first;
				for (; frag; frag = frag->u.nxt_free)
					if (frag->size >= size) {
						/* free fragments are detached in a simple round-robin manner */
						optimized_get_indexes[hash] =
						    (optimized_get_indexes[hash] + i + 1)
						     % shm_secondary_hash_size;
						hash = sec_hash;
						goto found;
					}

				SHM_UNLOCK(sec_hash);
			}
		}

		/* try in a bigger bucket */
	}

	/* out of memory... we have to shut down */
#if defined(DBG_MALLOC) || defined(STATISTICS)
	LM_ERR(oom_errorf, hpb->name, hpb->size - hpb->real_used, size,
			hpb->name[0] == 'p' ? "M" : "m");
#else
	LM_ERR(oom_nostats_errorf, hpb->name, size,
	       hpb->name[0] == 'p' ? "M" : "m");
#endif
	return NULL;

found:
	hp_frag_detach(hpb, frag);

	update_stats_shm_frag_detach(frag);

#if defined(DBG_MALLOC) || defined(STATISTICS)
	hpb->used += (frag)->size;
	hpb->real_used += (frag)->size + FRAG_OVERHEAD;
#endif

	#if !defined INLINE_ALLOC && defined DBG_MALLOC
	/* split the fragment if possible */
	shm_frag_split_dbg(hpb, frag, size, hash, file, "hp_malloc frag", line);
	#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
	shm_frag_split(hpb, frag, size, hash);
	#else
	shm_frag_split(hpb, frag, size, hash, file, "hp_malloc frag", line);
	#endif

	#ifdef DBG_MALLOC
	frag->file=file;
	frag->func=func;
	frag->line=line;
	#endif

	SHM_UNLOCK(hash);

#ifndef HP_MALLOC_FAST_STATS
	unsigned long real_used;

	real_used = get_stat_val(shm_rused);
	if (real_used > hpb->max_real_used)
		hpb->max_real_used = real_used;
#endif

	/* ignore concurrency issues, simply obtaining an estimate is enough */
	shm_hash_usage[init_hash]++;

	return (char *)frag + sizeof *frag;
}

#if !defined INLINE_ALLOC && defined DBG_MALLOC
void hp_pkg_free_dbg(struct hp_block *hpb, void *p,
					const char *file, const char *func, unsigned int line)
#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
void hp_pkg_free(struct hp_block *hpb, void *p)
#else
void hp_pkg_free(struct hp_block *hpb, void *p,
					const char *file, const char *func, unsigned int line)
#endif
{
	struct hp_frag *f, *next;

	if (!p) {
		LM_WARN("free(0) called\n");
		return;
	}

	f = HP_FRAG(p);
	check_double_free(p, f, hpb);

	/*
	 * for private memory, coalesce as many consecutive fragments as possible
	 * The same operation is not performed for shared memory, because:
	 *		- performance penalties introduced by additional locking logic
	 *		- the allocator itself actually favours fragmentation and reusage
	 */
	for (;;) {
		next = FRAG_NEXT(f);
		if (next >= hpb->last_frag || !next->prev)
			break;

		hp_frag_detach(hpb, next);
		update_stats_pkg_frag_detach(hpb, next);

#ifdef DBG_MALLOC
#ifndef STATISTICS
		hpb->used += next->size;
		hpb->real_used += next->size + FRAG_OVERHEAD;
#endif
		hpb->used += FRAG_OVERHEAD;
#endif

		f->size += next->size + FRAG_OVERHEAD;
		update_stats_pkg_frag_merge(hpb);

#if defined(DBG_MALLOC) && !defined(STATISTICS)
		hpb->real_used -= FRAG_OVERHEAD;
		hpb->total_fragments--;
#endif
	}

	hp_frag_attach(hpb, f);
	update_stats_pkg_frag_attach(hpb, f);

#if defined(DBG_MALLOC) && !defined(STATISTICS)
	hpb->used -= f->size;
	hpb->real_used -= f->size + FRAG_OVERHEAD;
#endif
}

#if !defined INLINE_ALLOC && defined DBG_MALLOC
void hp_shm_free_unsafe_dbg(struct hp_block *hpb, void *p,
							const char *file, const char *func, unsigned int line)
#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
void hp_shm_free_unsafe(struct hp_block *hpb, void *p)
#else
void hp_shm_free_unsafe(struct hp_block *hpb, void *p,
							const char *file, const char *func, unsigned int line)
#endif
{
	struct hp_frag *f;

	if (!p) {
		LM_WARN("free(0) called\n");
		return;
	}

	f = HP_FRAG(p);
	check_double_free(p, f, hpb);

	hp_frag_attach(hpb, f);
	update_stats_shm_frag_attach(f);

#if defined(DBG_MALLOC) || defined(STATISTICS)
	hpb->used -= f->size;
	hpb->real_used -= f->size + FRAG_OVERHEAD;
#endif
}

#if !defined INLINE_ALLOC && defined DBG_MALLOC
void hp_shm_free_dbg(struct hp_block *hpb, void *p,
							const char *file, const char *func, unsigned int line)
#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
void hp_shm_free(struct hp_block *hpb, void *p)
#else
void hp_shm_free(struct hp_block *hpb, void *p,
							const char *file, const char *func, unsigned int line)
#endif
{
	struct hp_frag *f;
	unsigned int hash;

	if (!p) {
		LM_WARN("free(0) called\n");
		return;
	}

	f = HP_FRAG(p);
	check_double_free(p, f, hpb);

	hash = PEEK_HASH_RR(hpb, f->size);

	SHM_LOCK(hash);
	hp_frag_attach(hpb, f);
	SHM_UNLOCK(hash);

	update_stats_shm_frag_attach(f);

#if defined(DBG_MALLOC) || defined(STATISTICS)
	hpb->used -= f->size;
	hpb->real_used -= f->size + FRAG_OVERHEAD;
#endif
}

#if !defined INLINE_ALLOC && defined DBG_MALLOC
void *hp_pkg_realloc_dbg(struct hp_block *hpb, void *p, unsigned long size,
						const char *file, const char *func, unsigned int line)
#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
void *hp_pkg_realloc(struct hp_block *hpb, void *p, unsigned long size)
#else
void *hp_pkg_realloc(struct hp_block *hpb, void *p, unsigned long size,
						const char *file, const char *func, unsigned int line)
#endif
{
	struct hp_frag *f;
	unsigned long diff;
	unsigned long orig_size;
	struct hp_frag *next;
	void *ptr;
	
	if (size == 0) {
		if (p)
			#if !defined INLINE_ALLOC && defined DBG_MALLOC
			hp_pkg_free_dbg(hpb, p, file, func, line);
			#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
			hp_pkg_free(hpb, p);
			#else
			hp_pkg_free(hpb, p, file, func, line);
			#endif

		return NULL;
	}

	if (!p)
		#if !defined INLINE_ALLOC && defined DBG_MALLOC
		return hp_pkg_malloc_dbg(hpb, size, file, func, line);
		#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
		return hp_pkg_malloc(hpb, size);
		#else
		return hp_pkg_malloc(hpb, size, file, func, line);
		#endif

	f = HP_FRAG(p);

	size = ROUNDUP(size);
	orig_size = f->size;

	/* shrink operation */
	if (orig_size > size) {
		#if !defined INLINE_ALLOC && defined DBG_MALLOC
		pkg_frag_split_dbg(hpb, f, size, file, "hp_realloc frag", line);
		#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
		pkg_frag_split(hpb, f, size);
		#else
		pkg_frag_split(hpb, f, size, file, "hp_realloc frag", line);
		#endif
	/* grow operation */
	} else if (orig_size < size) {
		
		diff = size - orig_size;
		next = FRAG_NEXT(f);

		/* try to join with a large enough adjacent free fragment */
		if (next < hpb->last_frag && next->prev &&
		    (next->size + FRAG_OVERHEAD) >= diff) {

			hp_frag_detach(hpb, next);
			update_stats_pkg_frag_detach(hpb, next);

			#ifdef DBG_MALLOC
			#ifndef STATISTICS
			hpb->used += next->size;
			hpb->real_used += next->size + FRAG_OVERHEAD;
			#endif
			hpb->used += FRAG_OVERHEAD;
			#endif

			f->size += next->size + FRAG_OVERHEAD;

			/* split the result if necessary */
			if (f->size > size)
				#if !defined INLINE_ALLOC && defined DBG_MALLOC
				pkg_frag_split_dbg(hpb, f, size, file, "hp_realloc frag",line);
				#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
				pkg_frag_split(hpb, f, size);
				#else
				pkg_frag_split(hpb, f, size, file, "hp_realloc frag", line);
				#endif

		} else {
			/* could not join => realloc */
			#if !defined INLINE_ALLOC && defined DBG_MALLOC
			ptr = hp_pkg_malloc_dbg(hpb, size, file, func, line);
			#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
			ptr = hp_pkg_malloc(hpb, size);
			#else
			ptr = hp_pkg_malloc(hpb, size, file, func, line);
			#endif

			if (ptr) {
				/* copy, need by libssl */
				memcpy(ptr, p, orig_size);

				#if !defined INLINE_ALLOC && defined DBG_MALLOC
				hp_pkg_free_dbg(hpb, p, file, func, line);
				#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
				hp_pkg_free(hpb, p);
				#else
				hp_pkg_free(hpb, p, file, func, line);
				#endif
			}
			p = ptr;
		}

		if (hpb->real_used > hpb->max_real_used)
			hpb->max_real_used = hpb->real_used;
	}

	pkg_threshold_check();
	return p;
}

#if !defined INLINE_ALLOC && defined DBG_MALLOC
void *hp_shm_realloc_unsafe_dbg(struct hp_block *hpb, void *p,
                                unsigned long size, const char *file,
                                const char *func, unsigned int line)
#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
void *hp_shm_realloc_unsafe(struct hp_block *hpb, void *p, unsigned long size)
#else
void *hp_shm_realloc_unsafe(struct hp_block *hpb, void *p,
                            unsigned long size, const char *file,
                            const char *func, unsigned int line)
#endif
{
	struct hp_frag *f;
	unsigned long orig_size;
	void *ptr;

	if (size == 0) {
		if (p)
			#if !defined INLINE_ALLOC && defined DBG_MALLOC
			hp_shm_free_unsafe_dbg(hpb, p, file, func, line);
			#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
			hp_shm_free_unsafe(hpb, p);
			#else
			hp_shm_free_unsafe(hpb, p, file, func, line);
			#endif

		return NULL;
	}

	if (!p)
		#if !defined INLINE_ALLOC && defined DBG_MALLOC
		return hp_shm_malloc_unsafe_dbg(hpb, size, file, func, line);
		#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
		return hp_shm_malloc_unsafe(hpb, size);
		#else
		return hp_shm_malloc_unsafe(hpb, size, file, func, line);
		#endif

	f = HP_FRAG(p);
	size = ROUNDUP(size);

	orig_size = f->size;

	/* shrink operation? */
	if (orig_size > size)
		#if !defined INLINE_ALLOC && defined DBG_MALLOC
		shm_frag_split_unsafe_dbg(hpb, f, size, file, "hp_realloc frag", line);
		#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
		shm_frag_split_unsafe(hpb, f, size);
		#else
		shm_frag_split_unsafe(hpb, f, size, file, "hp_realloc frag", line);
		#endif
	else if (orig_size < size) {
		#if !defined INLINE_ALLOC && defined DBG_MALLOC
		ptr = hp_shm_malloc_unsafe_dbg(hpb, size, file, func, line);
		#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
		ptr = hp_shm_malloc_unsafe(hpb, size);
		#else
		ptr = hp_shm_malloc_unsafe(hpb, size, file, func, line);
		#endif
		if (ptr) {
			/* copy, need by libssl */
			memcpy(ptr, p, orig_size);

			#if !defined INLINE_ALLOC && defined DBG_MALLOC
			hp_shm_free_unsafe_dbg(hpb, p, file, func, line);
			#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
			hp_shm_free_unsafe(hpb, p);
			#else
			hp_shm_free_unsafe(hpb, p, file, func, line);
			#endif
		}

		p = ptr;
	}

	return p;
}

#if !defined INLINE_ALLOC && defined DBG_MALLOC
void *hp_shm_realloc_dbg(struct hp_block *hpb, void *p, unsigned long size,
						const char *file, const char *func, unsigned int line)
#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
void *hp_shm_realloc(struct hp_block *hpb, void *p, unsigned long size)
#else
void *hp_shm_realloc(struct hp_block *hpb, void *p, unsigned long size,
						const char *file, const char *func, unsigned int line)
#endif
{
	struct hp_frag *f;
	unsigned long orig_size;
	unsigned int hash;
	void *ptr;

	if (size == 0) {
		if (p)
			#if !defined INLINE_ALLOC && defined DBG_MALLOC
			hp_shm_free_dbg(hpb, p, file, func, line);
			#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
			hp_shm_free(hpb, p);
			#else
			hp_shm_free(hpb, p, file, func, line);
			#endif

		return NULL;
	}

	if (!p)
		#if !defined INLINE_ALLOC && defined DBG_MALLOC
		return hp_shm_malloc_dbg(hpb, size, file, func, line);
		#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
		return hp_shm_malloc(hpb, size);
		#else
		return hp_shm_malloc(hpb, size, file, func, line);
		#endif

	f = HP_FRAG(p);
	size = ROUNDUP(size);

	hash = PEEK_HASH_RR(hpb, f->size);

	SHM_LOCK(hash);
	orig_size = f->size;

	if (orig_size > size) {
		/* shrink */
		#if !defined INLINE_ALLOC && defined DBG_MALLOC
		shm_frag_split_unsafe_dbg(hpb, f, size, file, "hp_realloc frag", line);
		#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
		shm_frag_split_unsafe(hpb, f, size);
		#else
		shm_frag_split_unsafe(hpb, f, size, file, "hp_realloc frag", line);
		#endif

	} else if (orig_size < size) {
		SHM_UNLOCK(hash);

		#if !defined INLINE_ALLOC && defined DBG_MALLOC
		ptr = hp_shm_malloc_dbg(hpb, size, file, func, line);
		#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
		ptr = hp_shm_malloc(hpb, size);
		#else
		ptr = hp_shm_malloc(hpb, size, file, func, line);
		#endif
		if (ptr) {
			/* copy, need by libssl */
			memcpy(ptr, p, orig_size);
			#if !defined INLINE_ALLOC && defined DBG_MALLOC
			hp_shm_free_dbg(hpb, p, file, func, line);
			#elif !defined HP_MALLOC_DYN && !defined DBG_MALLOC
			hp_shm_free(hpb, p);
			#else
			hp_shm_free(hpb, p, file, func, line);
			#endif
		}

		return ptr;
	}

	SHM_UNLOCK(hash);
	return p;
}

#if !defined INLINE_ALLOC && defined DBG_MALLOC
void hp_status_dbg(struct hp_block *hpb)
#else
void hp_status(struct hp_block *hpb)
#endif
{
	struct hp_frag *f;
	int i, j, si, t = 0;
	int h;

#ifdef DBG_MALLOC
	mem_dbg_htable_t allocd;
	struct mem_dbg_entry *it;
#endif

#if HP_MALLOC_FAST_STATS && (defined(DBG_MALLOC) || defined(STATISTICS))
	if (hpb == shm_block)
		update_shm_stats(hpb);
#endif

	LM_GEN1(memdump, "hp_status (%p, ROUNDTO=%ld):\n", hpb, ROUNDTO);
	if (!hpb)
		return;

	LM_GEN1(memdump, "%20s : %ld\n", "HP_HASH_SIZE", HP_HASH_SIZE);
	LM_GEN1(memdump, "%20s : %ld\n", "HP_EXTRA_HASH_SIZE", HP_HASH_SIZE);
	LM_GEN1(memdump, "%20s : %ld\n", "HP_TOTAL_SIZE", HP_HASH_SIZE);

	LM_GEN1(memdump, "%20s : %ld\n", "total_size", hpb->size);

#if defined(STATISTICS) || defined(DBG_MALLOC)
	LM_GEN1(memdump, "%20s : %lu\n%20s : %lu\n%20s : %lu\n",
			"used", hpb->used,
			"used+overhead", hpb->real_used,
			"free", hpb->size - hpb->used);

	LM_GEN1(memdump, "%20s : %lu\n\n", "max_used (+overhead)", hpb->max_real_used);
#endif

#ifdef DBG_MALLOC
	dbg_ht_init(allocd);

	for (f=hpb->first_frag; (char*)f<(char*)hpb->last_frag; f=FRAG_NEXT(f))
		if (!frag_is_free(f) && f->file)
			if (dbg_ht_update(allocd, f->file, f->func, f->line, f->size) < 0) {
				LM_ERR("Unable to update alloc'ed. memory summary\n");
				dbg_ht_free(allocd);
				return;
			}

	LM_GEN1(memdump, "dumping summary of all alloc'ed. fragments:\n");
	LM_GEN1(memdump, "------------+---------------------------------------\n");
	LM_GEN1(memdump, "total_bytes | num_allocations x [file: func, line]\n");
	LM_GEN1(memdump, "------------+---------------------------------------\n");
	for(i=0; i < DBG_HASH_SIZE; i++) {
		it = allocd[i];
		while (it) {
			LM_GEN1(memdump, " %10lu : %lu x [%s: %s, line %lu]\n",
				it->size, it->no_fragments, it->file, it->func, it->line);
			it = it->next;
		}
	}
	LM_GEN1(memdump, "----------------------------------------------------\n");

	dbg_ht_free(allocd);
#endif

	LM_GEN1(memdump, "Dumping free fragments:\n");

	for (h = 0; h < HP_HASH_SIZE; h++) {
		if (hpb->free_hash[h].is_optimized) {
			LM_GEN1(memdump, "[ %4d ][ %5d B ][ frags: ", h, h * (int)ROUNDTO);

			for (si = HP_HASH_SIZE + h * shm_secondary_hash_size, j = 0;
				 j < shm_secondary_hash_size; j++, si++, t++) {

				SHM_LOCK(si);
				for (i=0, f=hpb->free_hash[si].first; f; f=f->u.nxt_free, i++, t++)
					;
				SHM_UNLOCK(si);

				LM_GEN1(memdump, "%s%5d ", j == 0 ? "" : "| ", i);
			}

			LM_GEN1(memdump, "]\n");

		} else {
			SHM_LOCK(h);
				for (i=0, f=hpb->free_hash[h].first; f; f=f->u.nxt_free, i++, t++)
					;
			SHM_UNLOCK(h);

			if (i == 0)
				continue;

			if (h > HP_LINEAR_HASH_SIZE) {
				LM_GEN1(memdump, "[ %4d ][ %8d B -> %7d B ][ frags: %5d ]\n",
						h, (int)UN_HASH(h), (int)UN_HASH(h+1) - (int)ROUNDTO, i);
			} else
				LM_GEN1(memdump, "[ %4d ][ %5d B ][ frags: %5d ]\n",
						h, h * (int)ROUNDTO, i);
		}
	}

	LM_GEN1(memdump, "TOTAL: %6d free fragments\n", t);
	LM_GEN1(memdump, "TOTAL: %ld large bytes\n", hpb->large_space );
	LM_GEN1(memdump, "TOTAL: %u overhead\n", (unsigned int)FRAG_OVERHEAD );
	LM_GEN1(memdump, "-----------------------------\n");
}

#define HP_MALLOC_DYN