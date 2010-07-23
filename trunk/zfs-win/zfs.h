/* 
 *  This file contains the *_phys_t structs and misc macros gathered from the Solaris ZFS driver
 */

#pragma once

#pragma pack(push, 1)

#define	roundup(x, y) ((((x) + ((y) - 1)) / (y)) * (y))

#define	BSWAP_8(x) ((x) & 0xff)
#define	BSWAP_16(x) ((BSWAP_8(x) << 8) | BSWAP_8((x) >> 8))
#define	BSWAP_32(x) ((BSWAP_16(x) << 16) | BSWAP_16((x) >> 16))
#define	BSWAP_64(x) ((BSWAP_32(x) << 32) | BSWAP_32((x) >> 32))

#define	BMASK_8(x) ((x) & 0xff)
#define	BMASK_16(x) ((x) & 0xffff)
#define	BMASK_32(x) ((x) & 0xffffffff)
#define	BMASK_64(x) (x)

#define	SPA_MINBLOCKSHIFT 9
#define	SPA_MAXBLOCKSHIFT 17
#define	SPA_MINBLOCKSIZE (1ULL << SPA_MINBLOCKSHIFT)
#define	SPA_MAXBLOCKSIZE (1ULL << SPA_MAXBLOCKSHIFT)
#define	SPA_BLOCKSIZES (SPA_MAXBLOCKSHIFT - SPA_MINBLOCKSHIFT + 1)
#define	SPA_GANGBLOCKSIZE SPA_MINBLOCKSIZE
#define	SPA_GBH_NBLKPTRS ((SPA_GANGBLOCKSIZE - sizeof (zio_eck_t)) / sizeof (blkptr_t))
#define	SPA_GBH_FILLER ((SPA_GANGBLOCKSIZE - sizeof (zio_eck_t) - (SPA_GBH_NBLKPTRS * sizeof (blkptr_t))) / sizeof (uint64_t))
#define	SPA_BLKPTRSHIFT 7 /* blkptr_t is 128 bytes */
#define	SPA_DVAS_PER_BP 3 /* Number of DVAs in a bp */

union dva_t 
{
	struct
	{
		struct {uint32_t asize:24; uint32_t grid:8;};
		uint32_t vdev;
		struct {uint64_t offset:63; uint64_t gang:1;};
	};

	uint64_t word[2];
};

struct cksum_t
{
	uint64_t word[4];

	void set(uint64_t a, uint64_t b, uint64_t c, uint64_t d)
	{
		word[0] = a;
		word[1] = b;
		word[2] = c;
		word[3] = d;
	}

	bool operator == (cksum_t& c) const
	{
		return ((word[0] ^ c.word[0]) | (word[1] ^ c.word[1]) | (word[2] ^ c.word[2]) | (word[3] ^ c.word[3])) == 0;
	}
};

/*
 * vdev		virtual device ID
 * offset	offset into virtual device
 * LSIZE	logical size
 * PSIZE	physical size (after compression)
 * ASIZE	allocated size (including RAID-Z parity and gang block headers)
 * GRID		RAID-Z layout information (reserved for future use)
 * cksum	checksum function
 * comp		compression function
 * G		gang block indicator
 * B		byteorder (endianness)
 * D		dedup
 * X		unused
 * lvl		level of indirection
 * type		DMU object type
 * phys birth	txg of block allocation; zero if same as logical birth txg
 * log. birth	transaction group in which the block was logically born
 * fill count	number of non-zero blocks under this bp
 * checksum[4]	256-bit checksum of the data this bp describes
 */

struct blkptr_t
{
	dva_t blk_dva[SPA_DVAS_PER_BP]; /* Data Virtual Addresses */

	union /* size, compression, type, etc */
	{
		struct
		{
			uint16_t lsize;
			uint16_t psize;
			uint8_t comp_type;
			uint8_t cksum_type;
			uint8_t type;
			struct {uint8_t lvl:5; uint8_t x:1; uint8_t d:1; uint8_t b:1;};
		};

		uint64_t prop;
	}; 

	uint64_t pad[2]; /* Extra space for the future */
	uint64_t phys_birth; /* txg when block was allocated */
	uint64_t birth; /* transaction group at birth */
	uint64_t fill; /* fill count */
	cksum_t cksum; /* 256-bit checksum */
};

#define	ZEC_MAGIC 0x0210da7ab10c7a11ULL

struct zio_eck_t
{
	uint64_t magic; /* for validation, endianness (ZEC_MAGIC) */
	cksum_t cksum; /* 256-bit checksum */
};

struct zio_gbh_phys_t
{
	blkptr_t blkptr[SPA_GBH_NBLKPTRS];
	uint64_t filler[SPA_GBH_FILLER];
	zio_eck_t tail;
};

enum zio_checksum 
{
	ZIO_CHECKSUM_INHERIT = 0,
	ZIO_CHECKSUM_ON,
	ZIO_CHECKSUM_OFF,
	ZIO_CHECKSUM_LABEL,
	ZIO_CHECKSUM_GANG_HEADER,
	ZIO_CHECKSUM_ZILOG,
	ZIO_CHECKSUM_FLETCHER_2,
	ZIO_CHECKSUM_FLETCHER_4,
	ZIO_CHECKSUM_SHA256,
	ZIO_CHECKSUM_ZILOG2,
	ZIO_CHECKSUM_FUNCTIONS
};

#define	ZIO_CHECKSUM_ON_VALUE ZIO_CHECKSUM_FLETCHER_4
#define	ZIO_CHECKSUM_DEFAULT ZIO_CHECKSUM_ON

#define	ZIO_CHECKSUM_MASK 0xffULL
#define	ZIO_CHECKSUM_VERIFY (1 << 8)

#define	ZIO_DEDUPCHECKSUM ZIO_CHECKSUM_SHA256
#define	ZIO_DEDUPDITTO_MIN 100

enum zio_compress
{
	ZIO_COMPRESS_INHERIT = 0,
	ZIO_COMPRESS_ON,
	ZIO_COMPRESS_OFF,
	ZIO_COMPRESS_LZJB,
	ZIO_COMPRESS_EMPTY,
	ZIO_COMPRESS_GZIP_1,
	ZIO_COMPRESS_GZIP_2,
	ZIO_COMPRESS_GZIP_3,
	ZIO_COMPRESS_GZIP_4,
	ZIO_COMPRESS_GZIP_5,
	ZIO_COMPRESS_GZIP_6,
	ZIO_COMPRESS_GZIP_7,
	ZIO_COMPRESS_GZIP_8,
	ZIO_COMPRESS_GZIP_9,
	ZIO_COMPRESS_ZLE,
	ZIO_COMPRESS_FUNCTIONS
};

#define	ZIO_COMPRESS_ON_VALUE ZIO_COMPRESS_LZJB
#define	ZIO_COMPRESS_DEFAULT ZIO_COMPRESS_OFF

#define	BOOTFS_COMPRESS_VALID(c) ((c) == ZIO_COMPRESS_LZJB || ((c) == ZIO_COMPRESS_ON && ZIO_COMPRESS_ON_VALUE == ZIO_COMPRESS_LZJB) || (c) == ZIO_COMPRESS_OFF)

//

#define	VDEV_RAIDZ_MAXPARITY 3

#define	VDEV_PAD_SIZE (8 << 10)
/* 2 padding areas (vl_pad1 and vl_pad2) to skip */
#define	VDEV_SKIP_SIZE VDEV_PAD_SIZE * 2
#define	VDEV_PHYS_SIZE (112 << 10)
#define	VDEV_UBERBLOCK_RING (128 << 10)

/*
 * Size and offset of embedded boot loader region on each label.
 * The total size of the first two labels plus the boot area is 4MB.
 */

#define	VDEV_BOOT_OFFSET (2 * sizeof(vdev_label_t))
#define	VDEV_BOOT_SIZE (7ULL << 19) /* 3.5M	*/

/*
 * Size of label regions at the start and end of each leaf device.
 */

#define	VDEV_LABEL_START_SIZE (2 * sizeof(vdev_label_t) + VDEV_BOOT_SIZE)
#define	VDEV_LABEL_END_SIZE (2 * sizeof(vdev_label_t))
#define	VDEV_LABELS 4


struct vdev_phys_t
{
	uint8_t nvlist[VDEV_PHYS_SIZE - sizeof(zio_eck_t)];
	zio_eck_t zbt;
};

struct vdev_label_t /* 256K total */
{
	uint8_t pad1[VDEV_PAD_SIZE]; /* 8K */
	uint8_t pad2[VDEV_PAD_SIZE]; /* 8K */
	vdev_phys_t vdev_phys; /* 112K */
	uint8_t uberblock[VDEV_UBERBLOCK_RING]; /* 128K */
};

#define	UBERBLOCK_MAGIC 0x00bab10cULL /* oo-ba-bloc!	 */
#define	UBERBLOCK_SHIFT 10 /* up to 1K */

struct uberblock_t
{
	uint64_t magic; /* UBERBLOCK_MAGIC */
	uint64_t version; /* SPA_VERSION */
	uint64_t txg; /* txg of last sync */
	uint64_t guid_sum; /* sum of all vdev guids */
	uint64_t timestamp; /* UTC time of last sync */
	blkptr_t rootbp; /* MOS objset_phys_t */
};

//

#define	DNODE_SHIFT 9 /* 512 bytes */
#define	DN_MIN_INDBLKSHIFT 10 /* 1k */
#define	DN_MAX_INDBLKSHIFT 14 /* 16k */
#define	DNODE_BLOCK_SHIFT 14 /* 16k */
#define	DNODE_CORE_SIZE 64 /* 64 bytes for dnode sans blkptrs */
#define	DN_MAX_OBJECT_SHIFT	48 /* 256 trillion (zfs_fid_t limit) */
#define	DN_MAX_OFFSET_SHIFT 64 /* 2^64 bytes in a dnode */
#define	DNODE_SIZE (1 << DNODE_SHIFT)
#define	DN_MAX_NBLKPTR ((DNODE_SIZE - DNODE_CORE_SIZE) >> SPA_BLKPTRSHIFT)
#define	DN_MAX_BONUSLEN (DNODE_SIZE - DNODE_CORE_SIZE - (1 << SPA_BLKPTRSHIFT))
#define	DN_MAX_OBJECT (1ULL << DN_MAX_OBJECT_SHIFT)
#define	DN_ZERO_BONUSLEN (DN_MAX_BONUSLEN + 1)

enum dmu_object_type
{
	DMU_OT_NONE = 0,
	/* general: */
	DMU_OT_OBJECT_DIRECTORY, /* ZAP */
	DMU_OT_OBJECT_ARRAY, /* UINT64 */
	DMU_OT_PACKED_NVLIST, /* UINT8 (XDR by nvlist_pack/unpack) */
	DMU_OT_PACKED_NVLIST_SIZE, /* UINT64 */ /* bonus = uint64_t */
	DMU_OT_BPLIST, /* UINT64 */
	DMU_OT_BPLIST_HDR, /* UINT64 */
	/* spa: */
	DMU_OT_SPACE_MAP_HEADER, /* UINT64 */ /* bonus = space_map_obj_t */
	DMU_OT_SPACE_MAP = 8, /* UINT64 */
	/* zil: */
	DMU_OT_INTENT_LOG, /* UINT64 */
	/* dmu: */
	DMU_OT_DNODE, /* DNODE */
	DMU_OT_OBJSET, /* OBJSET */
	/* dsl: */
	DMU_OT_DSL_DIR, /* UINT64 */ /* bonus = dsl_dir_phys_t */
	DMU_OT_DSL_DIR_CHILD_MAP, /* ZAP */
	DMU_OT_DSL_DS_SNAP_MAP, /* ZAP */
	DMU_OT_DSL_PROPS, /* ZAP */
	DMU_OT_DSL_DATASET = 16, /* UINT64 */ /* bonus = dsl_dataset_phys_t */
	/* zpl: */
	DMU_OT_ZNODE, /* ZNODE */ /* bonus = znode_phys_t */
	DMU_OT_OLDACL, /* Old ACL */
	DMU_OT_PLAIN_FILE_CONTENTS, /* UINT8 */
	DMU_OT_DIRECTORY_CONTENTS, /* ZAP */ /* bonus = znode_phys_t */
	DMU_OT_MASTER_NODE, /* ZAP */
	DMU_OT_UNLINKED_SET, /* ZAP */
	/* zvol: */
	DMU_OT_ZVOL, /* UINT8 */
	DMU_OT_ZVOL_PROP = 24, /* ZAP */
	/* other; for testing only! */
	DMU_OT_PLAIN_OTHER, /* UINT8 */
	DMU_OT_UINT64_OTHER, /* UINT64 */
	DMU_OT_ZAP_OTHER, /* ZAP */
	/* new object types: */
	DMU_OT_ERROR_LOG, /* ZAP */
	DMU_OT_SPA_HISTORY, /* UINT8 */
	DMU_OT_SPA_HISTORY_OFFSETS, /* spa_history_phys_t */
	DMU_OT_POOL_PROPS, /* ZAP */
	DMU_OT_DSL_PERMS = 32, /* ZAP */
	DMU_OT_ACL, /* ACL */
	DMU_OT_SYSACL, /* SYSACL */
	DMU_OT_FUID, /* FUID table (Packed NVLIST UINT8) */
	DMU_OT_FUID_SIZE, /* FUID table size UINT64 */
	DMU_OT_NEXT_CLONES, /* ZAP */
	DMU_OT_SCRUB_QUEUE, /* ZAP */
	DMU_OT_USERGROUP_USED, /* ZAP */
	DMU_OT_USERGROUP_QUOTA = 40, /* ZAP */
	DMU_OT_USERREFS, /* ZAP */
	DMU_OT_DDT_ZAP, /* ZAP */
	DMU_OT_DDT_STATS, /* ZAP */
	DMU_OT_NUMTYPES
};

enum dmu_objset_type
{
	DMU_OST_NONE,
	DMU_OST_META,
	DMU_OST_ZFS,
	DMU_OST_ZVOL,
	DMU_OST_OTHER, /* For testing only! */
	DMU_OST_ANY, /* Be careful! */
	DMU_OST_NUMTYPES
};

#define	ZIL_REPLAY_NEEDED 0x1 /* replay needed - internal only */
#define	ZIL_CLAIM_LR_SEQ_VALID 0x2 /* zh_claim_lr_seq field is valid */

struct zil_header_t
{
	uint64_t claim_txg; /* txg in which log blocks were claimed */
	uint64_t replay_seq; /* highest replayed sequence number */
	blkptr_t log; /* log chain */
	uint64_t claim_blk_seq; /* highest claimed block sequence number */
	uint64_t flags; /* header flags */
	uint64_t claim_lr_seq; /* highest claimed lr sequence number */
	uint64_t pad[3];
};

struct zil_chain_t
{
	uint64_t pad;
	blkptr_t next_blk; /* next block in chain */
	uint64_t nused; /* bytes in log block used */
	zio_eck_t eck; /* block trailer */
};

// TODO: rest of the zil structs

struct dnode_phys_t
{
	uint8_t type; /* dmu_object_type_t */
	uint8_t indblkshift; /* ln2(indirect block size) */
	uint8_t nlevels; /* 1=dn_blkptr->data blocks */
	uint8_t nblkptr; /* length of dn_blkptr */
	uint8_t bonustype; /* type of data in bonus buffer */
	uint8_t	checksum; /* ZIO_CHECKSUM type */
	uint8_t	compress; /* ZIO_COMPRESS type */
	uint8_t flags; /* DNODE_FLAG_* */
	uint16_t datablkszsec; /* data block size in 512b sectors */
	uint16_t bonuslen; /* length of dn_bonus */
	uint8_t pad2[4];
	uint64_t maxblkid; /* largest allocated block ID */
	uint64_t used; /* bytes (or sectors) of disk space */
	uint64_t pad3[4];
	blkptr_t blkptr[1]; // 1-3 elements
	uint8_t pad4[DN_MAX_BONUSLEN];

	uint8_t* bonus() {return (uint8_t*)&blkptr[nblkptr];}
};

#define	OBJSET_PHYS_SIZE 2048
#define	OBJSET_OLD_PHYS_SIZE 1024
#define	OBJSET_FLAG_USERACCOUNTING_COMPLETE (1ULL<<0)

struct objset_phys_t
{
	union
	{
		struct
		{
			dnode_phys_t meta_dnode;
			zil_header_t zil_header;
			uint64_t type;
			uint64_t flags;
		};

		uint8_t pad[OBJSET_PHYS_SIZE];
	};

	dnode_phys_t userused_dnode;
	dnode_phys_t groupused_dnode;
};

#define	DS_FLAG_INCONSISTENT (1ULL<<0)

/*
 * NB: nopromote can not yet be set, but we want support for it in this
 * on-disk version, so that we don't need to upgrade for it later.  It
 * will be needed when we implement 'zfs split' (where the split off
 * clone should not be promoted).
 */

#define	DS_FLAG_NOPROMOTE (1ULL<<1)

/*
 * DS_FLAG_UNIQUE_ACCURATE is set if ds_unique_bytes has been correctly
 * calculated for head datasets (starting with SPA_VERSION_UNIQUE_ACCURATE,
 * refquota/refreservations).
 */

#define	DS_FLAG_UNIQUE_ACCURATE (1ULL<<2)

/*
 * DS_FLAG_DEFER_DESTROY is set after 'zfs destroy -d' has been called
 * on a dataset. This allows the dataset to be destroyed using 'zfs release'.
 */

#define	DS_FLAG_DEFER_DESTROY (1ULL<<3)

/*
 * DS_FLAG_CI_DATASET is set if the dataset contains a file system whose
 * name lookups should be performed case-insensitively.
 */

#define	DS_FLAG_CI_DATASET	(1ULL<<16)

#define	DS_IS_INCONSISTENT(ds) ((ds)->phys->flags & DS_FLAG_INCONSISTENT)
#define	DS_IS_DEFER_DESTROY(ds) ((ds)->phys->flags & DS_FLAG_DEFER_DESTROY)

struct dsl_dataset_phys_t
{
	uint64_t dir_obj; /* DMU_OT_DSL_DIR */
	uint64_t prev_snap_obj; /* DMU_OT_DSL_DATASET */
	uint64_t prev_snap_txg;
	uint64_t next_snap_obj; /* DMU_OT_DSL_DATASET */
	uint64_t snapnames_zapobj; /* DMU_OT_DSL_DS_SNAP_MAP 0 for snaps */
	uint64_t num_children; /* clone/snap children; ==0 for head */
	uint64_t creation_time; /* seconds since 1970 */
	uint64_t creation_txg;
	uint64_t deadlist_obj; /* DMU_OT_BPLIST */
	uint64_t used_bytes;
	uint64_t compressed_bytes;
	uint64_t uncompressed_bytes;
	uint64_t unique_bytes; /* only relevant to snapshots */

	/*
	 * The ds_fsid_guid is a 56-bit ID that can change to avoid
	 * collisions.  The ds_guid is a 64-bit ID that will never
	 * change, so there is a small probability that it will collide.
	 */

	uint64_t fsid_guid;
	uint64_t guid;
	uint64_t flags; /* DS_FLAG_* */
	blkptr_t bp;
	uint64_t next_clones_obj; /* DMU_OT_DSL_CLONES */
	uint64_t props_obj; /* DMU_OT_DSL_PROPS for snaps */
	uint64_t userrefs_obj; /* DMU_OT_USERREFS */
	uint64_t pad[5]; /* pad out to 320 bytes for good measure */
};

enum dd_used
{
	DD_USED_HEAD,
	DD_USED_SNAP,
	DD_USED_CHILD,
	DD_USED_CHILD_RSRV,
	DD_USED_REFRSRV,
	DD_USED_NUM
};

#define	DD_FLAG_USED_BREAKDOWN (1<<0)

struct dsl_dir_phys_t
{
	uint64_t creation_time; /* not actually used */
	uint64_t head_dataset_obj;
	uint64_t parent_obj;
	uint64_t origin_obj;
	uint64_t child_dir_zapobj;

	/*
	 * how much space our children are accounting for; for leaf
	 * datasets, == physical space used by fs + snaps
	 */

	uint64_t used_bytes;
	uint64_t compressed_bytes;
	uint64_t uncompressed_bytes;

	/* Administrative quota setting */

	uint64_t quota;

	/* Administrative reservation setting */

	uint64_t reserved;
	uint64_t props_zapobj;
	uint64_t deleg_zapobj; /* dataset delegation permissions */
	uint64_t flags; // DD_FLAG_USED_BREAKDOWN
	uint64_t used_breakdown[DD_USED_NUM];
	uint64_t pad[14]; /* pad out to 256 bytes for good measure */
};

#define	ZAP_MAGIC 0x2F52AB2ABULL

#define	ZBT_LEAF ((1ULL << 63) + 0)
#define	ZBT_HEADER ((1ULL << 63) + 1)
#define	ZBT_MICRO ((1ULL << 63) + 3)

#define	MZAP_ENT_LEN 64
#define	MZAP_NAME_LEN (MZAP_ENT_LEN - 8 - 4 - 2)
#define	MZAP_MAX_BLKSHIFT SPA_MAXBLOCKSHIFT
#define	MZAP_MAX_BLKSZ (1 << MZAP_MAX_BLKSHIFT)

struct mzap_ent_phys_t
{
	uint64_t value;
	uint32_t cd;
	uint16_t pad; /* in case we want to chain them someday */
	char name[MZAP_NAME_LEN];
};

struct mzap_phys_t
{
	uint64_t block_type; /* ZBT_MICRO */
	uint64_t salt;
	uint64_t normflags;
	uint64_t pad[5];
	mzap_ent_phys_t chunk[1];

	/* actually variable size depending on block size */
};

/*
 * The (fat) zap is stored in one object. It is an array of
 * 1<<FZAP_BLOCK_SHIFT byte blocks. The layout looks like one of:
 *
 * ptrtbl fits in first block:
 * 	[zap_phys_t zap_ptrtbl_shift < 6] [zap_leaf_t] ...
 *
 * ptrtbl too big for first block:
 * 	[zap_phys_t zap_ptrtbl_shift >= 6] [zap_leaf_t] [ptrtbl] ...
 *
 */

/* any other values are ptrtbl blocks */

#define	FZAP_BLOCK_SHIFT(zap) ((zap)->f.block_shift)

/*
 * the embedded pointer table takes up half a block:
 * block size / entry size (2^3) / 2
 */

#define	ZAP_EMBEDDED_PTRTBL_SHIFT(zap) (FZAP_BLOCK_SHIFT(zap) - 3 - 1)

/*
 * The embedded pointer table starts half-way through the block.  Since
 * the pointer table itself is half the block, it starts at (64-bit)
 * word number (1<<ZAP_EMBEDDED_PTRTBL_SHIFT(zap)).
 */

#define	ZAP_EMBEDDED_PTRTBL_ENT(zap, idx)  ((uint64_t *)(zap)->f.phys)[(idx) + (1 << ZAP_EMBEDDED_PTRTBL_SHIFT(zap))]

/*
 * TAKE NOTE:
 * If zap_phys_t is modified, zap_byteswap() must be modified.
 */

struct zap_table_phys_t
{
	uint64_t blk; /* starting block number */
	uint64_t numblks; /* number of blocks */
	uint64_t shift; /* bits to index it */
	uint64_t nextblk; /* next (larger) copy start block */
	uint64_t blks_copied; /* number source blocks copied */
};

struct zap_phys_t 
{
	uint64_t block_type; /* ZBT_HEADER */
	uint64_t magic; /* ZAP_MAGIC */
	zap_table_phys_t ptrtbl;
	uint64_t freeblk; /* the next free block */
	uint64_t num_leafs; /* number of leafs */
	uint64_t num_entries; /* number of entries */
	uint64_t salt; /* salt to stir into hash function */
	uint64_t normflags; /* flags for u8_textprep_str() */
	uint64_t flags; /* zap_flags_t */

	/*
	 * This structure is followed by padding, and then the embedded
	 * pointer table.  The embedded pointer table takes up second
	 * half of the block.  It is accessed using the
	 * ZAP_EMBEDDED_PTRTBL_ENT() macro.
	 */
};

#define	ZAP_LEAF_MAGIC 0x2AB1EAF

/* chunk size = 24 bytes */

#define	ZAP_LEAF_CHUNKSIZE 24

/*
 * The amount of space available for chunks is:
 * block size (1<<l->l_bs) - hash entry size (2) * number of hash
 * entries - header space (2*chunksize)
 */

#define	ZAP_LEAF_NUMCHUNKS(l) (((1 << (l)->bs) - 2 * ZAP_LEAF_HASH_NUMENTRIES(l)) / ZAP_LEAF_CHUNKSIZE - 2)

/*
 * The amount of space within the chunk available for the array is:
 * chunk size - space for type (1) - space for next pointer (2)
 */
#define	ZAP_LEAF_ARRAY_BYTES (ZAP_LEAF_CHUNKSIZE - 3)

#define	ZAP_LEAF_ARRAY_NCHUNKS(bytes) (((bytes) + ZAP_LEAF_ARRAY_BYTES - 1) / ZAP_LEAF_ARRAY_BYTES)

/*
 * Low water mark:  when there are only this many chunks free, start
 * growing the ptrtbl.  Ideally, this should be larger than a
 * "reasonably-sized" entry.  20 chunks is more than enough for the
 * largest directory entry (MAXNAMELEN (256) byte name, 8-byte value),
 * while still being only around 3% for 16k blocks.
 */

#define	ZAP_LEAF_LOW_WATER (20)

/*
 * The leaf hash table has block size / 2^5 (32) number of entries,
 * which should be more than enough for the maximum number of entries,
 * which is less than block size / CHUNKSIZE (24) / minimum number of
 * chunks per entry (3).
 */

#define	ZAP_LEAF_HASH_SHIFT(l) ((l)->bs - 5)
#define	ZAP_LEAF_HASH_NUMENTRIES(l) (1 << ZAP_LEAF_HASH_SHIFT(l))

/*
 * The chunks start immediately after the hash table.  The end of the
 * hash table is at l_hash + HASH_NUMENTRIES, which we simply cast to a
 * chunk_t.
 */

#define	ZAP_LEAF_CHUNK(l, idx) ((zap_leaf_chunk_t *) ((l)->phys->hash + ZAP_LEAF_HASH_NUMENTRIES(l)))[idx]
#define	ZAP_LEAF_ENTRY(l, idx) (&ZAP_LEAF_CHUNK(l, idx).entry)

enum zap_chunk_type
{
	ZAP_CHUNK_FREE = 253,
	ZAP_CHUNK_ENTRY = 252,
	ZAP_CHUNK_ARRAY = 251,
	ZAP_CHUNK_TYPE_MAX = 250
};

#define	ZLF_ENTRIES_CDSORTED (1<<0)

/*
 * TAKE NOTE:
 * If zap_leaf_phys_t is modified, zap_leaf_byteswap() must be modified.
 */

struct zap_leaf_phys_t
{
	uint64_t block_type; /* ZBT_LEAF */
	uint64_t pad1;
	uint64_t prefix; /* hash prefix of this leaf */
	uint32_t magic; /* ZAP_LEAF_MAGIC */
	uint16_t nfree; /* number free chunks */
	uint16_t nentries; /* number of entries */
	uint16_t prefix_len; /* num bits used to id this */

	/* above is accessable to zap, below is zap_leaf private */

	uint16_t freelist; /* chunk head of free list */
	uint8_t flags; /* ZLF_* flags */
	uint8_t pad2[11];

	/* 2 24-byte chunks */

	/*
	 * The header is followed by a hash table with
	 * ZAP_LEAF_HASH_NUMENTRIES(zap) entries.  The hash table is
	 * followed by an array of ZAP_LEAF_NUMCHUNKS(zap)
	 * zap_leaf_chunk structures.  These structures are accessed
	 * with the ZAP_LEAF_CHUNK() macro.
	 */

	uint16_t hash[1];
};

struct zap_leaf_entry_t
{
	uint8_t type; 		/* always ZAP_CHUNK_ENTRY */
	uint8_t value_intlen;	/* size of value's ints */
	uint16_t next;		/* next entry in hash chain */
	uint16_t name_chunk;		/* first chunk of the name */
	uint16_t name_numints;	/* ints in name (incl null) */
	uint16_t value_chunk;	/* first chunk of the value */
	uint16_t value_numints;	/* value length in ints */
	uint32_t cd;			/* collision differentiator */
	uint64_t hash;		/* hash value of the name */
};

struct zap_leaf_array_t
{
	uint8_t type;		/* always ZAP_CHUNK_ARRAY */
	uint8_t buff[ZAP_LEAF_ARRAY_BYTES];
	uint16_t next;		/* next blk or CHAIN_END */
};

struct zap_leaf_free_t
{
	uint8_t type;		/* always ZAP_CHUNK_FREE */
	uint8_t pad[ZAP_LEAF_ARRAY_BYTES];
	uint16_t next;	/* next in free list, or CHAIN_END */
};

struct bplist_phys_t
{
	/*
	 * This is the bonus buffer for the dead lists.  The object's
	 * contents is an array of bpl_entries blkptr_t's, representing
	 * a total of bpl_bytes physical space.
	 */

	uint64_t entries;
	uint64_t bytes;
	uint64_t comp;
	uint64_t uncomp;
};

/*
 * On-disk DDT formats, in the desired search order (newest version first).
 */

enum ddt_type
{
	DDT_TYPE_ZAP = 0,
	DDT_TYPES
};

/*
 * DDT classes, in the desired search order (highest replication level first).
 */

enum ddt_class
{
	DDT_CLASS_DITTO = 0,
	DDT_CLASS_DUPLICATE,
	DDT_CLASS_UNIQUE,
	DDT_CLASSES
};

#define	DDT_TYPE_CURRENT 0

#define	DDT_COMPRESS_BYTEORDER_MASK 0x80
#define	DDT_COMPRESS_FUNCTION_MASK 0x7f

/*
 * On-disk ddt entry:  key (name) and physical storage (value).
 */

struct ddt_key_t
{
	cksum_t	cksum; /* 256-bit block checksum */
	uint64_t prop; /* LSIZE, PSIZE, compression */
};

/*
 * ddk_prop layout:
 *
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 *	|   0	|   0	|   0	| comp	|     PSIZE	|     LSIZE	|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 */

#define	DDT_KEY_WORDS (sizeof(ddt_key_t) / sizeof (uint64_t))

struct ddt_phys_t
{
	dva_t dva[SPA_DVAS_PER_BP];
	uint64_t refcnt;
	uint64_t phys_birth;
};

enum ddt_phys_type
{
	DDT_PHYS_DITTO,
	DDT_PHYS_SINGLE,
	DDT_PHYS_DOUBLE,
	DDT_PHYS_TRIPLE,
	DDT_PHYS_TYPES
};

struct spa_history_phys_t
{
	uint64_t pool_create_len; /* ending offset of zpool create */
	uint64_t phys_max_off; /* physical EOF */
	uint64_t bof; /* logical BOF */
	uint64_t eof; /* logical EOF */
	uint64_t records_lost; /* num of records overwritten */
};

#define	ACE_SLOT_CNT 6
#define	ZFS_ACL_VERSION_INITIAL 0ULL
#define	ZFS_ACL_VERSION_FUID 1ULL
#define	ZFS_ACL_VERSION ZFS_ACL_VERSION_FUID

/*
 * ZFS ACLs are store in various forms.
 * Files created with ACL version ZFS_ACL_VERSION_INITIAL
 * will all be created with fixed length ACEs of type
 * zfs_oldace_t.
 *
 * Files with ACL version ZFS_ACL_VERSION_FUID will be created
 * with various sized ACEs.  The abstraction entries will utilize
 * zfs_ace_hdr_t, normal user/group entries will use zfs_ace_t
 * and some specialized CIFS ACEs will use zfs_object_ace_t.
 */

/*
 * All ACEs have a common hdr.  For
 * owner@, group@, and everyone@ this is all
 * thats needed.
 */

struct zfs_ace_hdr_t
{
	uint16_t type;
	uint16_t flags;
	uint32_t access_mask;
};

typedef zfs_ace_hdr_t zfs_ace_abstract_t;

/*
 * Standard ACE
 */

struct zfs_ace_t
{
	zfs_ace_hdr_t hdr;
	uint64_t fuid;
};

/*
 * The following type only applies to ACE_ACCESS_ALLOWED|DENIED_OBJECT_ACE_TYPE
 * and will only be set/retrieved in a CIFS context.
 */

struct zfs_object_ace_t
{
	zfs_ace_t ace;
	uint8_t object_type[16]; /* object type */
	uint8_t inherit_type[16]; /* inherited object type */
};

struct zfs_oldace_t
{
	uint32_t fuid; /* "who" */
	uint32_t access_mask; /* access mask */
	uint16_t flags; /* flags, i.e inheritance */
	uint16_t type; /* type of entry allow/deny */
};

struct zfs_acl_phys_v0_t
{
	uint64_t acl_extern_obj; /* ext acl pieces */
	uint32_t acl_count; /* Number of ACEs */
	uint16_t acl_version; /* acl version */
	uint16_t acl_pad; /* pad */
	zfs_oldace_t ace_data[ACE_SLOT_CNT]; /* 6 standard ACEs */
};

#define	ZFS_ACE_SPACE (sizeof (zfs_oldace_t) * ACE_SLOT_CNT)

struct zfs_acl_phys_t
{
	uint64_t acl_extern_obj;	  /* ext acl pieces */
	uint32_t acl_size;		  /* Number of bytes in ACL */
	uint16_t acl_version;		  /* acl version */
	uint16_t acl_count;		  /* ace count */
	uint8_t ace_data[ZFS_ACE_SPACE]; /* space for embedded ACEs */
};

/*
 * Additional file level attributes, that are stored
 * in the upper half of zp_flags
 */

#define	ZFS_READONLY 0x0000000100000000ULL
#define	ZFS_HIDDEN 0x0000000200000000ULL
#define	ZFS_SYSTEM 0x0000000400000000ULL
#define	ZFS_ARCHIVE 0x0000000800000000ULL
#define	ZFS_IMMUTABLE 0x0000001000000000ULL
#define	ZFS_NOUNLINK 0x0000002000000000ULL
#define	ZFS_APPENDONLY 0x0000004000000000ULL
#define	ZFS_NODUMP 0x0000008000000000ULL
#define	ZFS_OPAQUE 0x0000010000000000ULL
#define	ZFS_AV_QUARANTINED 0x0000020000000000ULL
#define	ZFS_AV_MODIFIED 0x0000040000000000ULL
#define	ZFS_REPARSE 0x0000080000000000ULL

#define	ZFS_ATTR_SET(zp, attr, value) { if(value) zp->phys->flags |= attr; else zp->phys->flags &= ~attr; }

/*
 * Define special zfs pflags
 */

#define	ZFS_XATTR 0x1 /* is an extended attribute */
#define	ZFS_INHERIT_ACE 0x2 /* ace has inheritable ACEs */
#define	ZFS_ACL_TRIVIAL 0x4 /* files ACL is trivial */
#define	ZFS_ACL_OBJ_ACE 0x8 /* ACL has CMPLX Object ACE */
#define	ZFS_ACL_PROTECTED 0x10 /* ACL protected */
#define	ZFS_ACL_DEFAULTED 0x20 /* ACL should be defaulted */
#define	ZFS_ACL_AUTO_INHERIT 0x40 /* ACL should be inherited */
#define	ZFS_BONUS_SCANSTAMP 0x80 /* Scanstamp in bonus area */
#define	ZFS_NO_EXECS_DENIED 0x100 /* exec was given to everyone */

/*
 * Is ID ephemeral?
 */

#define	IS_EPHEMERAL(x) (x > MAXUID)

/*
 * Should we use FUIDs?
 */

#define	USE_FUIDS(version, os) (version >= ZPL_VERSION_FUID && spa_version(dmu_objset_spa(os)) >= SPA_VERSION_FUID)

#define	MASTER_NODE_OBJ	1

/*
 * Special attributes for master node.
 * "userquota@" and "groupquota@" are also valid (from
 * zfs_userquota_prop_prefixes[]).
 */

#define	ZFS_FSID "FSID"
#define	ZFS_UNLINKED_SET "DELETE_QUEUE"
#define	ZFS_ROOT_OBJ "ROOT"
#define	ZPL_VERSION_STR "VERSION"
#define	ZFS_FUID_TABLES "FUID"
#define	ZFS_SHARES_DIR "SHARES"

#define	ZFS_MAX_BLOCKSIZE SPA_MAXBLOCKSIZE

/* Path component length */

/*
 * The generic fs code uses MAXNAMELEN to represent
 * what the largest component length is.  Unfortunately,
 * this length includes the terminating NULL.  ZFS needs
 * to tell the users via pathconf() and statvfs() what the
 * true maximum length of a component is, excluding the NULL.
 */

#define	ZFS_MAXNAMELEN (MAXNAMELEN - 1)

/*
 * Convert mode bits (zp_mode) to BSD-style DT_* values for storing in
 * the directory entries.
 */

#ifndef IFTODT
#define	IFTODT(mode) (((mode) & S_IFMT) >> 12)
#endif

/*
 * The directory entry has the type (currently unused on Solaris) in the
 * top 4 bits, and the object number in the low 48 bits.  The "middle"
 * 12 bits are unused.
 */

#define	ZFS_DIRENT_TYPE(de) BF64_GET(de, 60, 4)
#define	ZFS_DIRENT_OBJ(de) BF64_GET(de, 0, 48)

/*
 * This is the persistent portion of the znode.  It is stored
 * in the "bonus buffer" of the file.  Short symbolic links
 * are also stored in the bonus buffer.
 */

struct znode_phys_t
{
	uint64_t atime[2]; /*  0 - last file access time */
	uint64_t mtime[2]; /* 16 - last file modification time */
	uint64_t ctime[2]; /* 32 - last file change time */
	uint64_t crtime[2]; /* 48 - creation time */
	uint64_t gen; /* 64 - generation (txg of creation) */
	uint64_t mode; /* 72 - file mode bits */
	uint64_t size; /* 80 - size of file */
	uint64_t parent; /* 88 - directory parent (`..') */
	uint64_t links; /* 96 - number of links to file */
	uint64_t xattr; /* 104 - DMU object for xattrs */
	uint64_t rdev; /* 112 - dev_t for VBLK & VCHR files */
	uint64_t flags; /* 120 - persistent flags */
	uint64_t uid; /* 128 - file owner */
	uint64_t gid; /* 136 - owning group */
	uint64_t zap; /* 144 - extra attributes */
	uint64_t pad[3]; /* 152 - future */
	zfs_acl_phys_t acl; /* 176 - 263 ACL */

	/*
	 * Data may pad out any remaining bytes in the znode buffer, eg:
	 *
	 * |<---------------------- dnode_phys (512) ------------------------>|
	 * |<-- dnode (192) --->|<----------- "bonus" buffer (320) ---------->|
	 *			|<---- znode (264) ---->|<---- data (56) ---->|
	 *
	 * At present, we use this space for the following:
	 *  - symbolic links
	 *  - 32-byte anti-virus scanstamp (regular files only)
	 */
};

struct raidz_col_t
{
	uint64_t devidx; /* child device index for I/O */
	uint64_t offset; /* device offset */
	uint64_t size; /* I/O size */
};

class raidz_map_t
{
public:
	uint32_t m_cols; /* Regular column count */
	uint32_t m_scols; /* Count including skipped columns */
	uint32_t m_bigcols; /* Number of oversized columns */
	uint32_t m_firstdatacol; /* First data column/parity count */
	uint64_t m_nskip; /* Skipped sectors for padding */
	uint32_t m_skipstart; /* Column index of padding start */
	uint64_t m_asize; /* Actual total I/O size */
	std::vector<raidz_col_t> m_col; /* Flexible array of I/O columns */
	
public:
	raidz_map_t(uint64_t offset, uint64_t psize, uint32_t ashift, uint32_t dcols, uint32_t nparity)
		: m_cols(dcols)
		, m_scols(dcols)
	{
		uint64_t b = offset >> ashift;
		uint64_t s = psize >> ashift;
		uint32_t f = (uint32_t)(b % dcols);
		uint64_t o = (b / dcols) << ashift;
		uint64_t q = s / (dcols - nparity);
		uint32_t r = (uint32_t)(s - q * (dcols - nparity));
		uint32_t bc = (r == 0 ? 0 : r + nparity);
		uint64_t tot = s + nparity * (q + (r == 0 ? 0 : 1));

		if(q == 0)
		{
			m_cols = bc;
			m_scols = std::min<uint32_t>(dcols, roundup(bc, nparity + 1));
		} 

		// ASSERT3U(m_cols, <=, m_scols);

		m_bigcols = bc;
		m_skipstart = bc;
		m_firstdatacol = nparity;

		m_col.resize(m_scols);

		uint64_t asize = 0;

		for(uint32_t c = 0; c < m_scols; c++)
		{
			uint32_t col = f + c;
			uint64_t coff = o;
		
			if(col >= dcols)
			{
				col -= dcols;
				coff += 1ULL << ashift;
			}

			m_col[c].devidx = col;
			m_col[c].offset = coff;

			if(c >= m_cols)
			{
				m_col[c].size = 0;
			}
			else if(c < bc)
			{
				m_col[c].size = (q + 1) << ashift;
			}
			else
			{
				m_col[c].size = q << ashift;
			}

			asize += m_col[c].size;
		}

		m_asize = roundup(asize, (nparity + 1) << ashift);
		m_nskip = roundup(tot, nparity + 1) - tot;
		
		/*
		 * If all data stored spans all columns, there's a danger that parity
		 * will always be on the same device and, since parity isn't read
		 * during normal operation, that that device's I/O bandwidth won't be
		 * used effectively. We therefore switch the parity every 1MB.
		 *
		 * ... at least that was, ostensibly, the theory. As a practical
		 * matter unless we juggle the parity between all devices evenly, we
		 * won't see any benefit. Further, occasional writes that aren't a
		 * multiple of the LCM of the number of children and the minimum
		 * stripe width are sufficient to avoid pessimal behavior.
		 * Unfortunately, this decision created an implicit on-disk format
		 * requirement that we need to support for all eternity, but only
		 * for single-parity RAID-Z.
		 *
		 * If we intend to skip a sector in the zeroth column for padding
		 * we must make sure to note this swap. We will never intend to
		 * skip the first column since at least one data and one parity
		 * column must appear in each row.
		 */

		if(m_firstdatacol == 1 && (offset & (1ULL << 20)))
		{
			uint64_t devidx = m_col[0].devidx;
			uint64_t offset = m_col[0].offset;
			m_col[0].devidx = m_col[1].devidx;
			m_col[0].offset = m_col[1].offset;
			m_col[1].devidx = devidx;
			m_col[1].offset = offset;

			if(m_skipstart == 0)
			{
				m_skipstart = 1;
			}
		}
	}
};

#pragma pack(pop)
