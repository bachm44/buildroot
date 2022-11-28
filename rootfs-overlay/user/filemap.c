#include "tux3user.h"

#ifndef trace
#define trace trace_on
#endif

/* Truncate partial block. If partial, we have to update last block. */
int tux3_truncate_partial_block(struct inode *inode, loff_t newsize)
{
	unsigned delta = tux3_get_current_delta();
	struct sb *sb = tux_sb(inode->i_sb);
	block_t index = newsize >> sb->blockbits;
	unsigned offset = newsize & sb->blockmask;
	struct buffer_head *buffer, *clone;

	if (!offset)
		return 0;

	buffer = blockread(mapping(inode), index);
	if (!buffer)
		return -EIO;

	clone = blockdirty(buffer, delta);
	if (IS_ERR(clone)) {
		blockput(buffer);
		return PTR_ERR(clone);
	}

	memset(bufdata(clone) + offset, 0, sb->blocksize - offset);
	mark_buffer_dirty_non(clone);
	blockput(clone);

	return 0;
}

void tux3_truncate_pagecache(struct inode *inode, loff_t newsize)
{
	truncate_pagecache(inode, newsize);
}

#include "../filemap.c"

static int filemap_bufvec_check(struct bufvec *bufvec, enum map_mode mode)
{
	struct sb *sb = tux_sb(bufvec_inode(bufvec)->i_sb);
	struct buffer_head *buffer;

	trace("%s inode 0x%Lx block 0x%Lx",
	      (mode == MAP_READ) ? "read" :
			(mode == MAP_WRITE) ? "write" : "redirect",
	      tux_inode(bufvec_inode(bufvec))->inum,
	      bufvec_contig_index(bufvec));

	if (bufvec_contig_last_index(bufvec) & (-1LL << MAX_BLOCKS_BITS))
		return -EIO;

	list_for_each_entry(buffer, &bufvec->contig, link) {
		if (mode != MAP_READ && buffer_empty(buffer))
			tux3_warn(sb, "egad, writing an invalid buffer");
		if (mode == MAP_READ && buffer_dirty(buffer))
			tux3_warn(sb, "egad, reading a dirty buffer");
	}

	return 0;
}

/*
 * Extrapolate from single buffer blockread to opportunistic extent IO
 *
 * Essentially readahead:
 *  - stop at first present buffer
 *  - stop at end of file
 *
 * Stop when extent is "big enough", whatever that means.
 */
static int guess_readahead(struct bufvec *bufvec, struct inode *inode,
			   block_t index)
{
	struct sb *sb = inode->i_sb;
	struct buffer_head *buffer;
	block_t limit;
	int ret;

	bufvec_init(bufvec, inode->map, NULL, NULL);

	limit = (inode->i_size + sb->blockmask) >> sb->blockbits;
	if (limit > index + READAHEAD_BLOCKS)
		limit = index + READAHEAD_BLOCKS;

	/*
	 * FIXME: pin buffers early may be inefficient. We can delay to
	 * prepare buffers until filemap() was done.
	 */
	buffer = blockget(mapping(inode), index++);
	if (!buffer)
		return -ENOMEM;		/* FIXME: error code */
	ret = bufvec_contig_add(bufvec, buffer);
	assert(ret);

	while (index < limit) {
		struct buffer_head *nextbuf = peekblk(buffer->map, index);
		if (nextbuf) {
			unsigned stop = !buffer_empty(nextbuf);
			if (stop) {
				blockput(nextbuf);
				break;
			}
		} else {
			nextbuf = blockget(buffer->map, index);
			if (!nextbuf)
				break;
		}
		ret = bufvec_contig_add(bufvec, nextbuf);
		assert(ret);

		index++;
	}

	return 0;
}

/* For read end I/O */
static void filemap_read_endio(struct buffer_head *buffer, int err)
{
	if (err) {
		/* FIXME: What to do? Hack: This re-link to state from bufvec */
		assert(0);
		__set_buffer_empty(buffer);
	} else {
		set_buffer_clean(buffer);
	}
	/* This drops refcount for bufvec of guess_readahead() */
	blockput(buffer);
}

/* For hole region */
static void filemap_hole_endio(struct buffer_head *buffer, int err)
{
	assert(err == 0);
	memset(bufdata(buffer), 0, bufsize(buffer));
	set_buffer_clean(buffer);
	/* This drops refcount for bufvec of guess_readahead() */
	blockput(buffer);
}

/* For readahead cleanup */
static void filemap_clean_endio(struct buffer_head *buffer, int err)
{
	assert(err == 0);
	__set_buffer_empty(buffer);
	/* This drops refcount for bufvec of guess_readahead() */
	blockput(buffer);
}

static int filemap_extent_io(enum map_mode mode, int rw, struct bufvec *bufvec)
{
	struct inode *inode = bufvec_inode(bufvec);
	block_t block, index = bufvec_contig_index(bufvec);
	int err;

	/* FIXME: now assuming buffer is only 1 for MAP_READ */
	assert(mode != MAP_READ || bufvec_contig_count(bufvec) == 1);
	err = filemap_bufvec_check(bufvec, mode);
	if (err)
		return err;

	struct bufvec *bufvec_io, bufvec_ahead;
	unsigned count;
	if (!(rw & WRITE)) {
		/* In the case of read, use new bufvec for readahead */
		err = guess_readahead(&bufvec_ahead, inode, index);
		if (err)
			return err;
		bufvec_io = &bufvec_ahead;
	} else {
		bufvec_io = bufvec;
	}
	count = bufvec_contig_count(bufvec_io);

	struct block_segment seg[10];

	int segs = filemap(inode, index, count, seg, ARRAY_SIZE(seg), mode);
	if (segs < 0)
		return segs;
	assert(segs);

	for (int i = 0; i < segs; i++) {
		block = seg[i].block;
		count = seg[i].count;

		trace("extent 0x%Lx/%x => %Lx", index, count, block);

		if (seg[i].state != BLOCK_SEG_HOLE) {
			if (!(rw & WRITE))
				bufvec_io->end_io = filemap_read_endio;
			else
				bufvec_io->end_io = clear_buffer_dirty_for_endio;

			err = blockio_vec(rw, bufvec_io, block, count);
			if (err)
				break;
		} else {
			assert(!(rw & WRITE));
			bufvec_io->end_io = filemap_hole_endio;
			bufvec_complete_without_io(bufvec_io, count);
		}

		index += count;
	}

	/*
	 * In the write case, bufvec owner is caller. And caller must
	 * be handle buffers was not mapped (and is not written out)
	 * this time.
	 */
	if (!(rw & WRITE)) {
		/* Clean buffers was not mapped in this time */
		count = bufvec_contig_count(bufvec_io);
		if (count) {
			bufvec_io->end_io = filemap_clean_endio;
			bufvec_complete_without_io(bufvec_io, count);
		}
		bufvec_free(bufvec_io);
	}

	return err;
}

static int tuxio(struct file *file, void *data, unsigned len, int write)
{
	unsigned delta = write ? tux3_get_current_delta() : 0;
	struct inode *inode = file->f_inode;
	struct sb *sb = tux_sb(inode->i_sb);
	loff_t pos = file->f_pos;
	int err = 0;

	trace("%s %u bytes at %Lu, isize = 0x%Lx",
	      write ? "write" : "read", len, (s64)pos, (s64)inode->i_size);

	if (write && pos + len > sb->s_maxbytes)
		return -EFBIG;
	if (!write && pos + len > inode->i_size) {
		if (pos >= inode->i_size)
			return 0;
		len = inode->i_size - pos;
	}

	if (write) {
		tux3_iattrdirty(inode);
		inode->i_mtime = inode->i_ctime = gettime();
	}

	unsigned bbits = sb->blockbits;
	unsigned bsize = sb->blocksize;
	unsigned bmask = sb->blockmask;

	loff_t tail = len;
	while (tail) {
		struct buffer_head *buffer, *clone;
		unsigned from = pos & bmask;
		unsigned some = from + tail > bsize ? bsize - from : tail;
		int full = write && some == bsize;

		if (full)
			buffer = blockget(mapping(inode), pos >> bbits);
		else
			buffer = blockread(mapping(inode), pos >> bbits);
		if (!buffer) {
			err = -EIO;
			break;
		}

		if (write) {
			clone = blockdirty(buffer, delta);
			if (IS_ERR(clone)) {
				blockput(buffer);
				err = PTR_ERR(clone);
				break;
			}

			memcpy(bufdata(clone) + from, data, some);
			mark_buffer_dirty_non(clone);
		} else {
			clone = buffer;
			memcpy(data, bufdata(clone) + from, some);
		}

		trace_off("transfer %u bytes, block 0x%Lx, buffer %p",
			  some, bufindex(clone), buffer);

		blockput(clone);

		tail -= some;
		data += some;
		pos += some;
	}
	file->f_pos = pos;

	if (write) {
		if (inode->i_size < pos)
			inode->i_size = pos;
		tux3_mark_inode_dirty(inode);
	}

	return err ? err : len - tail;
}

int tuxread(struct file *file, void *data, unsigned len)
{
	return tuxio(file, data, len, 0);
}

int tuxwrite(struct file *file, const void *data, unsigned len)
{
	struct sb *sb = file->f_inode->i_sb;
	int ret;
	change_begin(sb);
	ret = tuxio(file, (void *)data, len, 1);
	change_end(sb);
	return ret;
}

void tuxseek(struct file *file, loff_t pos)
{
	file->f_pos = pos;
}

int page_symlink(struct inode *inode, const char *symname, int len)
{
	struct file file = { .f_inode = inode, };
	int ret;

	assert(inode->i_size == 0);
	ret = tuxio(&file, (void *)symname, len, 1);
	if (ret < 0)
		return ret;
	if (len != ret)
		return -EIO;
	return 0;
}

int page_readlink(struct inode *inode, void *buf, unsigned size)
{
	struct file file = { .f_inode = inode, };
	unsigned len = min_t(loff_t, inode->i_size, size);
	int ret;

	ret = tuxread(&file, buf, len);
	if (ret < 0)
		return ret;
	if (ret != len)
		return -EIO;
	return 0;
}
