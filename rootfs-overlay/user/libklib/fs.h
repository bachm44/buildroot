#ifndef LIBKLIB_FS_H
#define LIBKLIB_FS_H

/* depending on tux3 */

#include <libklib/lockdebug.h>
#include <libklib/uidgid.h>

struct nameidata {
};

/* Generic inode */
struct inode {
	struct sb		*i_sb;

	struct mutex		i_mutex;
	unsigned long		i_state;
	atomic_t		i_count;

	umode_t			i_mode;
	kuid_t			i_uid;
	kgid_t			i_gid;
	unsigned int		i_nlink;
	dev_t			i_rdev;
	loff_t			i_size;
	struct timespec		i_atime;
	struct timespec		i_mtime;
	struct timespec		i_ctime;
	spinlock_t		i_lock;
	u64			i_version;

	map_t			*map;
	struct hlist_node	i_hash;
};

/*
 * Helper functions so that in most cases filesystems will
 * not need to deal directly with kuid_t and kgid_t and can
 * instead deal with the raw numeric values that are stored
 * in the filesystem.
 */
static inline uid_t i_uid_read(const struct inode *inode)
{
	return from_kuid(&init_user_ns, inode->i_uid);
}

static inline gid_t i_gid_read(const struct inode *inode)
{
	return from_kgid(&init_user_ns, inode->i_gid);
}

static inline void i_uid_write(struct inode *inode, uid_t uid)
{
	inode->i_uid = make_kuid(&init_user_ns, uid);
}

static inline void i_gid_write(struct inode *inode, gid_t gid)
{
	inode->i_gid = make_kgid(&init_user_ns, gid);
}

/*
 * dentry stuff
 */

struct qstr {
	/* unsigned int hash; */
	unsigned int len;
	const unsigned char *name;
};

struct dentry {
	struct qstr d_name;
	struct inode *d_inode;
};

void d_instantiate(struct dentry *dentry, struct inode *inode);
struct dentry *d_splice_alias(struct inode *inode, struct dentry *dentry);

/*
 * fs stuff
 */

#define REQ_WRITE	1
#define REQ_SYNC	0
#define REQ_META	0
#define REQ_PRIO	0
#define REQ_NOIDLE	0

#define REQ_RAHEAD	0

#define REQ_FUA		0
#define REQ_FLUSH	0

#define RW_MASK		REQ_WRITE
#define RWA_MASK	REQ_RAHEAD

#define READ		0
#define WRITE		RW_MASK
#define READA		RWA_MASK

#define READ_SYNC	(READ | REQ_SYNC)
#define WRITE_SYNC	(WRITE | REQ_SYNC | REQ_NOIDLE)
#define WRITE_ODIRECT	(WRITE | REQ_SYNC)
#define WRITE_FLUSH	(WRITE | REQ_SYNC | REQ_NOIDLE | REQ_FLUSH)
#define WRITE_FUA	(WRITE | REQ_SYNC | REQ_NOIDLE | REQ_FUA)
#define WRITE_FLUSH_FUA	(WRITE | REQ_SYNC | REQ_NOIDLE | REQ_FLUSH | REQ_FUA)

/* File handle */
struct file {
	struct inode	*f_inode;
	u64		f_version;
	loff_t		f_pos;
};

#define MAX_LFS_FILESIZE	((loff_t)LLONG_MAX)

static inline struct inode *file_inode(struct file *f)
{
	return f->f_inode;
}

/*
 * File types
 *
 * NOTE! These match bits 12..15 of stat.st_mode
 * (ie "(i_mode >> 12) & 15").
 */
#define DT_UNKNOWN	0
#define DT_FIFO		1
#define DT_CHR		2
#define DT_DIR		4
#define DT_BLK		6
#define DT_REG		8
#define DT_LNK		10
#define DT_SOCK		12
#define DT_WHT		14

typedef int (*filldir_t)(void *, const char *, int, loff_t, u64, unsigned);
struct dir_context {
	const filldir_t actor;
	loff_t pos;
};

static inline bool dir_emit(struct dir_context *ctx,
			    const char *name, int namelen,
			    u64 ino, unsigned type)
{
	return ctx->actor(ctx, name, namelen, ctx->pos, ino, type) == 0;
}
static inline bool dir_relax(struct inode *inode)
{
	return true;
}

void inc_nlink(struct inode *inode);
void drop_nlink(struct inode *inode);
void clear_nlink(struct inode *inode);
void set_nlink(struct inode *inode, unsigned int nlink);

void mark_inode_dirty(struct inode *inode);
static inline void inode_inc_link_count(struct inode *inode)
{
	inc_nlink(inode);
	mark_inode_dirty(inode);
}

static inline void inode_dec_link_count(struct inode *inode)
{
	drop_nlink(inode);
	mark_inode_dirty(inode);
}

#endif /* !LIBKLIB_FS_H */
