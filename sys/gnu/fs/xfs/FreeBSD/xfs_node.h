#ifndef __XFS_NODE_H__
#define __XFS_NODE_H__

/*
 * Save one allocation on FreeBSD and always allocate both inode and
 * xfs_vnode struct as a single memory block.
 */
struct xfs_node
{
	struct xfs_inode n_inode;
	struct xfs_vnode n_vnode;
};

#define XFS_CAST_IP2VP(ip)	(&((struct xfs_node *)(ip))->n_vnode)

#endif	/* __XFS_NODE_H__ */
