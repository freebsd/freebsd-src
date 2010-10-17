	.set noat
	.set noreorder
	.set nomacro
	.arch ev6
	.section	.init.data,"aw",@progbits
	.align 2
	.type	mount_initrd, @object
	.size	mount_initrd, 4
mount_initrd:
	.long	0
	.globl root_mountflags
	.section	.sdata,"aw",@progbits
	.align 2
	.type	root_mountflags, @object
	.size	root_mountflags, 4
root_mountflags:
	.long	32769
	.section	.sbss,"aw"
	.type	do_devfs, @object
	.size	do_devfs, 4
	.align 2
do_devfs:
	.zero	4
	.section	.init.text,"ax",@progbits
	.align 2
	.align 4
	.ent load_ramdisk
load_ramdisk:
	.frame $30,16,$26,0
	.mask 0x4000000,-16
	ldah $29,0($27)		!gpdisp!1
	lda $29,0($29)		!gpdisp!1
$load_ramdisk..ng:
	ldq $27,simple_strtol($29)		!literal!2
	lda $30,-16($30)
	mov $31,$17
	mov $31,$18
	stq $26,0($30)
	.prologue 1
	jsr $26,($27),simple_strtol		!lituse_jsr!2
	ldah $29,0($26)		!gpdisp!3
	lda $29,0($29)		!gpdisp!3
	ldq $26,0($30)
	and $0,3,$0
	ldah $1,rd_doload($29)		!gprelhigh
	stl $0,rd_doload($1)		!gprellow
	lda $0,1($31)
	lda $30,16($30)
	ret $31,($26),1
	.end load_ramdisk
	.section	.init.data
	.type	__setup_str_load_ramdisk, @object
	.size	__setup_str_load_ramdisk, 14
__setup_str_load_ramdisk:
	.ascii "load_ramdisk=\0"
	.section	.init.setup,"aw",@progbits
	.align 3
	.type	__setup_load_ramdisk, @object
	.size	__setup_load_ramdisk, 16
__setup_load_ramdisk:
	.quad	__setup_str_load_ramdisk
	.quad	load_ramdisk
	.section	.init.text
	.align 2
	.align 4
	.ent readonly
readonly:
	.frame $30,0,$26,0
	ldah $29,0($27)		!gpdisp!4
	lda $29,0($29)		!gpdisp!4
$readonly..ng:
	.prologue 1
	ldbu $1,0($16)
	mov $31,$0
	bne $1,$L167
	ldl $1,root_mountflags($29)		!gprel
	lda $0,1($31)
	bis $1,1,$1
	stl $1,root_mountflags($29)		!gprel
$L167:
	ret $31,($26),1
	.end readonly
	.align 2
	.align 4
	.ent readwrite
readwrite:
	.frame $30,0,$26,0
	ldah $29,0($27)		!gpdisp!5
	lda $29,0($29)		!gpdisp!5
$readwrite..ng:
	.prologue 1
	ldbu $1,0($16)
	mov $31,$0
	bne $1,$L169
	ldl $1,root_mountflags($29)		!gprel
	lda $0,1($31)
	bic $1,1,$1
	stl $1,root_mountflags($29)		!gprel
$L169:
	ret $31,($26),1
	.end readwrite
	.section	.init.data
	.type	__setup_str_readonly, @object
	.size	__setup_str_readonly, 3
__setup_str_readonly:
	.ascii "ro\0"
	.section	.init.setup
	.align 3
	.type	__setup_readonly, @object
	.size	__setup_readonly, 16
__setup_readonly:
	.quad	__setup_str_readonly
	.quad	readonly
	.section	.init.data
	.type	__setup_str_readwrite, @object
	.size	__setup_str_readwrite, 3
__setup_str_readwrite:
	.ascii "rw\0"
	.section	.init.setup
	.align 3
	.type	__setup_readwrite, @object
	.size	__setup_readwrite, 16
__setup_readwrite:
	.quad	__setup_str_readwrite
	.quad	readwrite
	.section	.rodata.str1.1,"aMS",@progbits,1
$LC1:
	.ascii "/sys/block/%s/dev\0"
$LC2:
	.ascii "/sys/block/%s/range\0"
	.section	.init.text
	.align 2
	.align 4
	.ent try_name
try_name:
	.frame $30,160,$26,0
	.mask 0x4003e00,-160
	ldah $29,0($27)		!gpdisp!6
	lda $29,0($29)		!gpdisp!6
$try_name..ng:
	lda $30,-160($30)
	ldq $27,sprintf($29)		!literal!25
	stq $10,16($30)
	stq $12,32($30)
	mov $16,$10
	mov $17,$12
	ldah $17,$LC1($29)		!gprelhigh
	stq $26,0($30)
	stq $9,8($30)
	lda $16,48($30)
	stq $11,24($30)
	stq $13,40($30)
	.prologue 1
	mov $10,$18
	lda $17,$LC1($17)		!gprellow
	jsr $26,($27),sprintf		!lituse_jsr!25
	ldah $29,0($26)		!gpdisp!26
	lda $29,0($29)		!gpdisp!26
	lda $16,48($30)
	mov $31,$18
	mov $31,$17
	ldq $27,sys_open($29)		!literal!23
	jsr $26,($27),sys_open		!lituse_jsr!23
	ldah $29,0($26)		!gpdisp!24
	addl $31,$0,$9
	lda $29,0($29)		!gpdisp!24
	blt $9,$L174
	ldq $27,sys_read($29)		!literal!21
	lda $11,112($30)
	mov $9,$16
	lda $18,32($31)
	mov $11,$17
	jsr $26,($27),sys_read		!lituse_jsr!21
	ldah $29,0($26)		!gpdisp!22
	lda $29,0($29)		!gpdisp!22
	addl $31,$9,$16
	addl $31,$0,$9
	ldq $27,sys_close($29)		!literal!19
	jsr $26,($27),sys_close		!lituse_jsr!19
	ldah $29,0($26)		!gpdisp!20
	cmpeq $9,32,$2
	cmple $9,0,$1
	lda $29,0($29)		!gpdisp!20
	bis $1,$2,$1
	bne $1,$L174
	subl $9,1,$2
	addq $11,$2,$0
	ldbu $1,0($0)
	cmpeq $1,10,$1
	bne $1,$L189
$L174:
	mov $31,$0
$L171:
	ldq $26,0($30)
	ldq $9,8($30)
	ldq $10,16($30)
	ldq $11,24($30)
	ldq $12,32($30)
	ldq $13,40($30)
	lda $30,160($30)
	ret $31,($26),1
$L189:
	ldq $27,simple_strtoul($29)		!literal!17
	mov $11,$16
	lda $17,144($30)
	lda $18,16($31)
	stb $31,0($0)
	jsr $26,($27),simple_strtoul		!lituse_jsr!17
	ldah $29,0($26)		!gpdisp!18
	ldq $1,144($30)
	lda $29,0($29)		!gpdisp!18
	addl $31,$0,$13
	ldbu $2,0($1)
	bne $2,$L174
	mov $13,$0
	beq $12,$L171
	ldq $27,sprintf($29)		!literal!15
	ldah $17,$LC2($29)		!gprelhigh
	mov $10,$18
	lda $16,48($30)
	lda $17,$LC2($17)		!gprellow
	jsr $26,($27),sprintf		!lituse_jsr!15
	ldah $29,0($26)		!gpdisp!16
	lda $29,0($29)		!gpdisp!16
	lda $16,48($30)
	mov $31,$18
	mov $31,$17
	ldq $27,sys_open($29)		!literal!13
	jsr $26,($27),sys_open		!lituse_jsr!13
	ldah $29,0($26)		!gpdisp!14
	addl $31,$0,$9
	lda $29,0($29)		!gpdisp!14
	blt $9,$L174
	ldq $27,sys_read($29)		!literal!11
	mov $9,$16
	mov $11,$17
	lda $18,32($31)
	jsr $26,($27),sys_read		!lituse_jsr!11
	ldah $29,0($26)		!gpdisp!12
	lda $29,0($29)		!gpdisp!12
	addl $31,$9,$16
	addl $31,$0,$9
	ldq $27,sys_close($29)		!literal!9
	jsr $26,($27),sys_close		!lituse_jsr!9
	ldah $29,0($26)		!gpdisp!10
	cmpeq $9,32,$2
	cmple $9,0,$1
	lda $29,0($29)		!gpdisp!10
	bis $1,$2,$1
	bne $1,$L174
	subl $9,1,$2
	addq $11,$2,$0
	ldbu $1,0($0)
	cmpeq $1,10,$1
	beq $1,$L174
	ldq $27,simple_strtoul($29)		!literal!7
	mov $11,$16
	lda $17,144($30)
	lda $18,10($31)
	stb $31,0($0)
	jsr $26,($27),simple_strtoul		!lituse_jsr!7
	ldah $29,0($26)		!gpdisp!8
	ldq $1,144($30)
	lda $29,0($29)		!gpdisp!8
	addl $31,$0,$0
	ldbu $2,0($1)
	bne $2,$L174
	cmplt $12,$0,$1
	addl $13,$12,$0
	bne $1,$L171
	br $31,$L174
	.end try_name
	.section	.rodata.str1.1
$LC3:
	.ascii "/sys\0"
$LC4:
	.ascii "sysfs\0"
$LC5:
	.ascii "/dev/\0"
$LC6:
	.ascii "nfs\0"
	.section	.init.text
	.align 2
	.align 4
	.globl name_to_dev_t
	.ent name_to_dev_t
name_to_dev_t:
	.frame $30,96,$26,0
	.mask 0x4001e00,-96
	ldah $29,0($27)		!gpdisp!27
	lda $29,0($29)		!gpdisp!27
$name_to_dev_t..ng:
	lda $30,-96($30)
	ldq $27,sys_mkdir($29)		!literal!46
	lda $17,448($31)
	stq $12,32($30)
	stq $9,8($30)
	ldah $12,$LC3($29)		!gprelhigh
	lda $9,$LC3($12)		!gprellow
	stq $10,16($30)
	stq $11,24($30)
	mov $16,$10
	stq $26,0($30)
	.prologue 1
	mov $31,$11
	mov $9,$16
	jsr $26,($27),sys_mkdir		!lituse_jsr!46
	ldah $29,0($26)		!gpdisp!47
	lda $29,0($29)		!gpdisp!47
	mov $9,$17
	mov $31,$19
	mov $31,$20
	ldah $16,$LC4($29)		!gprelhigh
	ldq $27,sys_mount($29)		!literal!44
	lda $16,$LC4($16)		!gprellow
	mov $16,$18
	jsr $26,($27),sys_mount		!lituse_jsr!44
	ldah $29,0($26)		!gpdisp!45
	lda $29,0($29)		!gpdisp!45
	blt $0,$L192
	ldq $27,memcmp($29)		!literal!42
	ldah $17,$LC5($29)		!gprelhigh
	mov $10,$16
	lda $18,5($31)
	lda $17,$LC5($17)		!gprellow
	jsr $26,($27),memcmp		!lituse_jsr!42
	ldah $29,0($26)		!gpdisp!43
	lda $29,0($29)		!gpdisp!43
	bne $0,$L219
	ldq $27,memcmp($29)		!literal!38
	lda $10,5($10)
	ldah $17,$LC6($29)		!gprelhigh
	lda $18,4($31)
	lda $11,255($31)
	mov $10,$16
	lda $17,$LC6($17)		!gprellow
	jsr $26,($27),memcmp		!lituse_jsr!38
	ldah $29,0($26)		!gpdisp!39
	lda $29,0($29)		!gpdisp!39
	beq $0,$L196
	ldq $27,strlen($29)		!literal!36
	mov $10,$16
	jsr $26,($27),strlen		!lituse_jsr!36
	ldah $29,0($26)		!gpdisp!37
	cmpule $0,31,$0
	lda $29,0($29)		!gpdisp!37
	beq $0,$L195
	ldq $27,strcpy($29)		!literal!34
	mov $10,$17
	lda $16,48($30)
	jsr $26,($27),strcpy		!lituse_jsr!34
	ldah $29,0($26)		!gpdisp!35
	ldbu $1,48($30)
	lda $16,48($30)
	lda $29,0($29)		!gpdisp!35
	mov $16,$2
	stq $16,80($30)
	beq $1,$L217
	lda $3,46($31)
	.align 4
$L204:
	ldbu $1,0($2)
	cmpeq $1,47,$1
	bne $1,$L220
$L201:
	lda $16,1($16)
	stq $16,80($30)
	mov $16,$2
	ldbu $1,0($16)
	bne $1,$L204
$L217:
	lda $16,48($30)
	mov $31,$17
	bsr $26,try_name		!samegp
	addl $31,$0,$11
	bne $11,$L196
	ldq $16,80($30)
	lda $2,48($30)
	cmpule $16,$2,$1
	mov $16,$3
	bne $1,$L207
	ldq $4,_ctype($29)		!literal
	ldbu $1,-1($16)
	addq $1,$4,$1
	ldbu $2,0($1)
	and $2,4,$2
	beq $2,$L207
	.align 4
$L210:
	lda $16,-1($3)
	lda $2,48($30)
	cmpule $16,$2,$1
	stq $16,80($30)
	mov $16,$3
	bne $1,$L207
	ldbu $1,-1($16)
	addq $1,$4,$1
	ldbu $2,0($1)
	and $2,4,$2
	bne $2,$L210
	.align 4
$L207:
	lda $2,48($30)
	cmpeq $16,$2,$1
	bne $1,$L195
	ldbu $1,0($16)
	sextb $1,$1
	beq $1,$L195
	cmpeq $1,48,$1
	bne $1,$L195
	ldq $27,simple_strtoul($29)		!literal!32
	mov $31,$17
	lda $18,10($31)
	jsr $26,($27),simple_strtoul		!lituse_jsr!32
	ldah $29,0($26)		!gpdisp!33
	ldq $1,80($30)
	addl $31,$0,$9
	lda $29,0($29)		!gpdisp!33
	lda $16,48($30)
	mov $9,$17
	stb $31,0($1)
	bsr $26,try_name		!samegp
	addl $31,$0,$11
	bne $11,$L196
	ldq $4,80($30)
	lda $1,50($30)
	cmpult $4,$1,$1
	bne $1,$L195
	ldbu $1,-2($4)
	ldq $3,_ctype($29)		!literal
	addq $1,$3,$1
	ldbu $2,0($1)
	and $2,4,$2
	beq $2,$L195
	ldbu $1,-1($4)
	cmpeq $1,112,$1
	bne $1,$L221
	.align 4
$L195:
	mov $31,$11
$L196:
	ldq $27,sys_umount($29)		!literal!30
	lda $16,$LC3($12)		!gprellow
	mov $31,$17
	jsr $26,($27),sys_umount		!lituse_jsr!30
	ldah $29,0($26)		!gpdisp!31
	lda $29,0($29)		!gpdisp!31
$L192:
	ldq $27,sys_rmdir($29)		!literal!28
	lda $16,$LC3($12)		!gprellow
	jsr $26,($27),sys_rmdir		!lituse_jsr!28
	ldah $29,0($26)		!gpdisp!29
	mov $11,$0
	ldq $26,0($30)
	ldq $9,8($30)
	lda $29,0($29)		!gpdisp!29
	ldq $10,16($30)
	ldq $11,24($30)
	ldq $12,32($30)
	lda $30,96($30)
	ret $31,($26),1
$L221:
	stb $31,-1($4)
	mov $9,$17
	lda $16,48($30)
	bsr $26,try_name		!samegp
	addl $31,$0,$11
	br $31,$L196
	.align 4
$L220:
	stb $3,0($2)
	ldq $16,80($30)
	br $31,$L201
	.align 4
$L219:
	ldq $27,simple_strtoul($29)		!literal!40
	mov $10,$16
	lda $17,80($30)
	lda $18,16($31)
	jsr $26,($27),simple_strtoul		!lituse_jsr!40
	ldah $29,0($26)		!gpdisp!41
	ldq $1,80($30)
	lda $29,0($29)		!gpdisp!41
	addl $31,$0,$11
	ldbu $2,0($1)
	beq $2,$L196
	br $31,$L195
	.end name_to_dev_t
	.align 2
	.align 4
	.ent root_dev_setup
root_dev_setup:
	.frame $30,16,$26,0
	.mask 0x4000200,-16
	ldah $29,0($27)		!gpdisp!48
	lda $29,0($29)		!gpdisp!48
$root_dev_setup..ng:
	lda $30,-16($30)
	ldq $27,strncpy($29)		!literal!49
	mov $16,$17
	lda $18,64($31)
	stq $9,8($30)
	stq $26,0($30)
	.prologue 1
	ldah $9,saved_root_name($29)		!gprelhigh
	lda $9,saved_root_name($9)		!gprellow
	mov $9,$16
	jsr $26,($27),strncpy		!lituse_jsr!49
	ldah $29,0($26)		!gpdisp!50
	stb $31,63($9)
	lda $0,1($31)
	lda $29,0($29)		!gpdisp!50
	ldq $26,0($30)
	ldq $9,8($30)
	lda $30,16($30)
	ret $31,($26),1
	.end root_dev_setup
	.section	.init.data
	.type	__setup_str_root_dev_setup, @object
	.size	__setup_str_root_dev_setup, 6
__setup_str_root_dev_setup:
	.ascii "root=\0"
	.section	.init.setup
	.align 3
	.type	__setup_root_dev_setup, @object
	.size	__setup_root_dev_setup, 16
__setup_root_dev_setup:
	.quad	__setup_str_root_dev_setup
	.quad	root_dev_setup
	.section	.init.text
	.align 2
	.align 4
	.ent root_data_setup
root_data_setup:
	.frame $30,0,$26,0
	ldah $29,0($27)		!gpdisp!51
	lda $29,0($29)		!gpdisp!51
$root_data_setup..ng:
	.prologue 1
	ldah $1,root_mount_data($29)		!gprelhigh
	lda $0,1($31)
	stq $16,root_mount_data($1)		!gprellow
	ret $31,($26),1
	.end root_data_setup
	.align 2
	.align 4
	.ent fs_names_setup
fs_names_setup:
	.frame $30,0,$26,0
	ldah $29,0($27)		!gpdisp!52
	lda $29,0($29)		!gpdisp!52
$fs_names_setup..ng:
	.prologue 1
	ldah $1,root_fs_names($29)		!gprelhigh
	lda $0,1($31)
	stq $16,root_fs_names($1)		!gprellow
	ret $31,($26),1
	.end fs_names_setup
	.section	.init.data
	.type	__setup_str_root_data_setup, @object
	.size	__setup_str_root_data_setup, 11
__setup_str_root_data_setup:
	.ascii "rootflags=\0"
	.section	.init.setup
	.align 3
	.type	__setup_root_data_setup, @object
	.size	__setup_root_data_setup, 16
__setup_root_data_setup:
	.quad	__setup_str_root_data_setup
	.quad	root_data_setup
	.section	.init.data
	.type	__setup_str_fs_names_setup, @object
	.size	__setup_str_fs_names_setup, 12
__setup_str_fs_names_setup:
	.ascii "rootfstype=\0"
	.section	.init.setup
	.align 3
	.type	__setup_fs_names_setup, @object
	.size	__setup_fs_names_setup, 16
__setup_fs_names_setup:
	.quad	__setup_str_fs_names_setup
	.quad	fs_names_setup
	.section	.init.text
	.align 2
	.align 4
	.ent get_fs_names
get_fs_names:
	.frame $30,32,$26,0
	.mask 0x4000600,-32
	ldah $29,0($27)		!gpdisp!53
	lda $29,0($29)		!gpdisp!53
$get_fs_names..ng:
	ldah $1,root_fs_names($29)		!gprelhigh
	lda $30,-32($30)
	ldq $17,root_fs_names($1)		!gprellow
	stq $10,16($30)
	mov $16,$10
	stq $26,0($30)
	stq $9,8($30)
	.prologue 1
	beq $17,$L226
	ldq $27,strcpy($29)		!literal!58
	jsr $26,($27),strcpy		!lituse_jsr!58
	ldah $29,0($26)		!gpdisp!59
	ldbu $1,0($10)
	lda $29,0($29)		!gpdisp!59
	lda $10,1($10)
	beq $1,$L232
	.align 4
$L231:
	ldbu $1,-1($10)
	cmpeq $1,44,$1
	bne $1,$L245
$L227:
	ldbu $1,0($10)
	lda $10,1($10)
	bne $1,$L231
	.align 4
$L232:
	stb $31,0($10)
	ldq $26,0($30)
	ldq $9,8($30)
	ldq $10,16($30)
	lda $30,32($30)
	ret $31,($26),1
	.align 4
$L245:
	stb $31,-1($10)
	br $31,$L227
$L226:
	ldq $27,get_filesystem_list($29)		!literal!56
	jsr $26,($27),get_filesystem_list		!lituse_jsr!56
	ldah $29,0($26)		!gpdisp!57
	addq $10,$0,$0
	lda $9,-1($10)
	lda $29,0($29)		!gpdisp!57
	stb $31,0($0)
	beq $9,$L232
	.align 4
$L241:
	ldq $27,strchr($29)		!literal!54
	lda $9,1($9)
	lda $17,10($31)
	mov $9,$16
	jsr $26,($27),strchr		!lituse_jsr!54
	ldah $29,0($26)		!gpdisp!55
	ldbu $1,0($9)
	lda $29,0($29)		!gpdisp!55
	lda $9,1($9)
	cmpeq $1,9,$1
	bne $1,$L238
$L235:
	mov $0,$9
	bne $0,$L241
	br $31,$L232
	.align 4
$L238:
	ldbu $1,0($9)
	lda $9,1($9)
	cmpeq $1,10,$2
	stb $1,0($10)
	lda $10,1($10)
	beq $2,$L238
	stb $31,-1($10)
	br $31,$L235
	.end get_fs_names
	.section	.rodata.str1.1
$LC7:
	.ascii "/root\0"
$LC8:
	.ascii "VFS: Cannot open root device \"%s\" or %s\12\0"
$LC9:
	.ascii "Please append a correct \"root=\" boot option\12\0"
$LC10:
	.ascii "VFS: Unable to mount root fs on %s\0"
$LC12:
	.ascii " readonly\0"
$LC13:
	.ascii "\0"
$LC11:
	.ascii "VFS: Mounted root (%s filesystem)%s.\12\0"
	.section	.init.text
	.align 2
	.align 4
	.ent mount_block_root
mount_block_root:
	.frame $30,64,$26,0
	.mask 0x400fe00,-64
	ldah $29,0($27)		!gpdisp!60
	lda $29,0($29)		!gpdisp!60
$mount_block_root..ng:
	ldq $1,names_cachep($29)		!literal
	lda $30,-64($30)
	ldq $27,kmem_cache_alloc($29)		!literal!82
	stq $12,32($30)
	stq $11,24($30)
	mov $16,$12
	mov $17,$11
	stq $26,0($30)
	stq $9,8($30)
	lda $17,464($31)
	ldq $16,0($1)
	stq $10,16($30)
	stq $13,40($30)
	stq $14,48($30)
	stq $15,56($30)
	.prologue 1
	jsr $26,($27),kmem_cache_alloc		!lituse_jsr!82
	ldah $29,0($26)		!gpdisp!83
	lda $29,0($29)		!gpdisp!83
	mov $0,$16
	mov $0,$10
	bsr $26,get_fs_names		!samegp
$L247:
	ldbu $1,0($10)
	mov $10,$9
	beq $1,$L267
	ldah $1,$LC7($29)		!gprelhigh
	ldah $13,root_mount_data($29)		!gprelhigh
	ldq $15,ROOT_DEV($29)		!literal
	lda $14,$LC7($1)		!gprellow
$L262:
	ldq $20,root_mount_data($13)		!gprellow
	ldq $27,sys_mount($29)		!literal!80
	mov $9,$18
	mov $12,$16
	mov $14,$17
	mov $11,$19
	jsr $26,($27),sys_mount		!lituse_jsr!80
	ldah $29,0($26)		!gpdisp!81
	addl $31,$0,$0
	lda $29,0($29)		!gpdisp!81
	mov $9,$16
	lda $1,13($0)
	lda $2,22($0)
	beq $1,$L255
	bgt $1,$L259
	beq $2,$L250
$L252:
	ldl $1,0($15)
	ldq $27,kdevname($29)		!literal!78
	bis $31,$1,$16
	jsr $26,($27),kdevname		!lituse_jsr!78
	ldah $29,0($26)		!gpdisp!79
	lda $29,0($29)		!gpdisp!79
	mov $0,$18
	ldq $27,printk($29)		!literal!76
	ldah $17,root_device_name($29)		!gprelhigh
	ldah $16,$LC8($29)		!gprelhigh
	lda $17,root_device_name($17)		!gprellow
	lda $16,$LC8($16)		!gprellow
	jsr $26,($27),printk		!lituse_jsr!76
	ldah $29,0($26)		!gpdisp!77
	lda $29,0($29)		!gpdisp!77
	ldq $27,printk($29)		!literal!74
	ldah $16,$LC9($29)		!gprelhigh
	lda $16,$LC9($16)		!gprellow
	jsr $26,($27),printk		!lituse_jsr!74
	ldah $29,0($26)		!gpdisp!75
	lda $29,0($29)		!gpdisp!75
	ldl $1,0($15)
	ldq $27,kdevname($29)		!literal!72
	bis $31,$1,$16
	jsr $26,($27),kdevname		!lituse_jsr!72
	ldah $29,0($26)		!gpdisp!73
	lda $29,0($29)		!gpdisp!73
$L269:
	mov $0,$17
	ldah $16,$LC10($29)		!gprelhigh
	lda $16,$LC10($16)		!gprellow
	ldq $27,panic($29)		!literal!67
	jsr $26,($27),panic		!lituse_jsr!67
	.align 4
$L250:
	ldq $27,strlen($29)		!literal!70
	jsr $26,($27),strlen		!lituse_jsr!70
	ldah $29,0($26)		!gpdisp!71
	addq $9,$0,$0
	lda $29,0($29)		!gpdisp!71
	ldbu $1,1($0)
	lda $9,1($0)
	bne $1,$L262
$L267:
	ldq $1,ROOT_DEV($29)		!literal
	ldq $27,kdevname($29)		!literal!68
	ldl $2,0($1)
	bis $31,$2,$16
	jsr $26,($27),kdevname		!lituse_jsr!68
	ldah $29,0($26)		!gpdisp!69
	lda $29,0($29)		!gpdisp!69
	br $31,$L269
$L259:
	bne $0,$L252
$L254:
	ldq $1,names_cachep($29)		!literal
	ldq $27,kmem_cache_free($29)		!literal!65
	mov $10,$17
	ldq $16,0($1)
	jsr $26,($27),kmem_cache_free		!lituse_jsr!65
	ldah $29,0($26)		!gpdisp!66
	lda $29,0($29)		!gpdisp!66
	mov $14,$16
	ldq $27,sys_chdir($29)		!literal!63
	jsr $26,($27),sys_chdir		!lituse_jsr!63
	ldah $29,0($26)		!gpdisp!64
	ldq $4,64($8)
	lda $29,0($29)		!gpdisp!64
	ldah $1,$LC12($29)		!gprelhigh
	lda $18,$LC12($1)		!gprellow
	ldq $2,1264($4)
	ldq $3,40($2)
	ldq $2,ROOT_DEV($29)		!literal
	ldq $1,40($3)
	ldl $3,16($1)
	ldq $4,56($1)
	ldq $5,96($1)
	stl $3,0($2)
	ldq $17,0($4)
	blbs $5,$L265
	ldah $1,$LC13($29)		!gprelhigh
	lda $18,$LC13($1)		!gprellow
$L265:
	ldq $27,printk($29)		!literal!61
	ldah $16,$LC11($29)		!gprelhigh
	lda $16,$LC11($16)		!gprellow
	jsr $26,($27),printk		!lituse_jsr!61
	ldah $29,0($26)		!gpdisp!62
	ldq $26,0($30)
	ldq $9,8($30)
	lda $29,0($29)		!gpdisp!62
	ldq $10,16($30)
	ldq $11,24($30)
	ldq $12,32($30)
	ldq $13,40($30)
	ldq $14,48($30)
	ldq $15,56($30)
	lda $30,64($30)
	ret $31,($26),1
$L255:
	bis $11,1,$11
	br $31,$L247
	.end mount_block_root
	.align 2
	.align 4
	.ent create_dev
create_dev:
	.frame $30,96,$26,0
	.mask 0x4000600,-96
	ldah $29,0($27)		!gpdisp!84
	lda $29,0($29)		!gpdisp!84
$create_dev..ng:
	ldq $27,sys_unlink($29)		!literal!87
	lda $30,-96($30)
	stq $9,8($30)
	stq $10,16($30)
	mov $16,$9
	mov $17,$10
	stq $26,0($30)
	.prologue 1
	jsr $26,($27),sys_unlink		!lituse_jsr!87
	ldah $29,0($26)		!gpdisp!88
	lda $29,0($29)		!gpdisp!88
	lda $0,-1($31)
	mov $9,$16
	mov $10,$18
	ldl $1,do_devfs($29)		!gprel
	lda $17,24960($31)
	beq $1,$L280
$L270:
	ldq $26,0($30)
	ldq $9,8($30)
	ldq $10,16($30)
	lda $30,96($30)
	ret $31,($26),1
	.align 4
$L280:
	ldq $27,sys_mknod($29)		!literal!85
	jsr $26,($27),sys_mknod		!lituse_jsr!85
	ldah $29,0($26)		!gpdisp!86
	lda $29,0($29)		!gpdisp!86
	addl $31,$0,$0
	br $31,$L270
	.end create_dev
	.align 2
	.align 4
	.ent rd_load_image
$rd_load_image..ng:
rd_load_image:
	.frame $30,0,$26,0
	.prologue 0
	mov $31,$0
	ret $31,($26),1
	.end rd_load_image
	.section	.rodata.str1.1
$LC14:
	.ascii "/dev/root\0"
	.section	.init.text
	.align 2
	.align 4
	.ent rd_load_disk
rd_load_disk:
	.frame $30,0,$26,0
	ldah $29,0($27)		!gpdisp!89
	lda $29,0($29)		!gpdisp!89
$rd_load_disk..ng:
	.prologue 1
	ldah $16,$LC14($29)		!gprelhigh
	lda $16,$LC14($16)		!gprellow
	br $31,rd_load_image		!samegp
	.end rd_load_disk
	.align 2
	.align 4
	.ent mount_root
mount_root:
	.frame $30,16,$26,0
	.mask 0x4000200,-16
	ldah $29,0($27)		!gpdisp!90
	lda $29,0($29)		!gpdisp!90
$mount_root..ng:
	ldq $1,ROOT_DEV($29)		!literal
	lda $30,-16($30)
	ldah $18,root_device_name($29)		!gprelhigh
	stq $9,8($30)
	lda $18,root_device_name($18)		!gprellow
	stq $26,0($30)
	.prologue 1
	ldah $9,$LC14($29)		!gprelhigh
	lda $9,$LC14($9)		!gprellow
	ldl $17,0($1)
	mov $9,$16
	bsr $26,create_dev		!samegp
	ldq $26,0($30)
	mov $9,$16
	ldl $17,root_mountflags($29)		!gprel
	ldq $9,8($30)
	lda $30,16($30)
	br $31,mount_block_root		!samegp
	.end mount_root
	.align 2
	.align 4
	.ent handle_initrd
$handle_initrd..ng:
handle_initrd:
	.frame $30,0,$26,0
	.prologue 0
	ret $31,($26),1
	.end handle_initrd
	.section	.rodata.str1.1
$LC15:
	.ascii "/dev/initrd\0"
	.section	.init.text
	.align 2
	.align 4
	.ent initrd_load
initrd_load:
	.frame $30,0,$26,0
	ldah $29,0($27)		!gpdisp!91
	lda $29,0($29)		!gpdisp!91
$initrd_load..ng:
	.prologue 1
	ldah $16,$LC15($29)		!gprelhigh
	lda $16,$LC15($16)		!gprellow
	br $31,rd_load_image		!samegp
	.end initrd_load
	.section	.rodata.str1.1
$LC16:
	.ascii "/dev\0"
$LC17:
	.ascii ".\0"
$LC18:
	.ascii "/\0"
	.text
	.align 2
	.align 4
	.globl prepare_namespace
	.ent prepare_namespace
prepare_namespace:
	.frame $30,32,$26,0
	.mask 0x4000e00,-32
	ldah $29,0($27)		!gpdisp!92
	lda $29,0($29)		!gpdisp!92
$prepare_namespace..ng:
	lda $30,-32($30)
	stq $10,16($30)
	stq $9,8($30)
	ldah $9,saved_root_name($29)		!gprelhigh
	ldq $10,ROOT_DEV($29)		!literal
	stq $11,24($30)
	stq $26,0($30)
	.prologue 1
	ldbu $2,saved_root_name($9)		!gprellow
	ldl $1,0($10)
	zapnot $1,15,$1
	srl $1,8,$1
	cmpeq $1,2,$11
	bne $2,$L296
$L287:
	ldl $17,0($10)
	ldah $16,$LC14($29)		!gprelhigh
	mov $31,$18
	lda $16,$LC14($16)		!gprellow
	bsr $26,create_dev		!samegp
	ldah $1,mount_initrd($29)		!gprelhigh
	ldl $2,mount_initrd($1)		!gprellow
	beq $2,$L290
	bsr $26,initrd_load		!samegp
	beq $0,$L293
	ldl $1,0($10)
	lda $1,-256($1)
	bne $1,$L297
	.align 4
$L293:
	bsr $26,mount_root		!samegp
$L292:
	ldq $27,sys_umount($29)		!literal!98
	ldah $16,$LC16($29)		!gprelhigh
	mov $31,$17
	lda $16,$LC16($16)		!gprellow
	jsr $26,($27),sys_umount		!lituse_jsr!98
	ldah $29,0($26)		!gpdisp!99
	lda $29,0($29)		!gpdisp!99
	mov $31,$18
	lda $19,8192($31)
	mov $31,$20
	ldah $9,$LC17($29)		!gprelhigh
	ldq $27,sys_mount($29)		!literal!96
	ldah $17,$LC18($29)		!gprelhigh
	lda $9,$LC17($9)		!gprellow
	lda $17,$LC18($17)		!gprellow
	mov $9,$16
	jsr $26,($27),sys_mount		!lituse_jsr!96
	ldah $29,0($26)		!gpdisp!97
	lda $29,0($29)		!gpdisp!97
	mov $9,$16
	ldq $27,sys_chroot($29)		!literal!94
	jsr $26,($27),sys_chroot		!lituse_jsr!94
	ldah $29,0($26)		!gpdisp!95
	lda $29,0($29)		!gpdisp!95
	ldq $1,security_ops($29)		!literal
	ldq $2,0($1)
	ldq $27,184($2)
	jsr $26,($27),0
	ldah $29,0($26)		!gpdisp!93
	ldq $26,0($30)
	ldq $9,8($30)
	lda $29,0($29)		!gpdisp!93
	ldq $10,16($30)
	ldq $11,24($30)
	lda $30,32($30)
	ret $31,($26),1
$L297:
	bsr $26,handle_initrd		!samegp
	br $31,$L292
	.align 4
$L290:
	beq $11,$L293
	ldah $1,rd_doload($29)		!gprelhigh
	ldl $2,rd_doload($1)		!gprellow
	beq $2,$L293
	mov $31,$16
	bsr $26,rd_load_disk		!samegp
	beq $0,$L293
	lda $1,256($31)
	stl $1,0($10)
	br $31,$L293
	.align 4
$L296:
	lda $9,saved_root_name($9)		!gprellow
	mov $9,$16
	bsr $26,name_to_dev_t		!samegp
	mov $9,$16
	ldq $27,memcmp($29)		!literal!102
	ldah $17,$LC5($29)		!gprelhigh
	stl $0,0($10)
	lda $18,5($31)
	lda $17,$LC5($17)		!gprellow
	jsr $26,($27),memcmp		!lituse_jsr!102
	ldah $29,0($26)		!gpdisp!103
	lda $29,0($29)		!gpdisp!103
	lda $1,5($9)
	cmoveq $0,$1,$9
	ldq $27,strcpy($29)		!literal!100
	ldah $16,root_device_name($29)		!gprelhigh
	lda $16,root_device_name($16)		!gprellow
	mov $9,$17
	jsr $26,($27),strcpy		!lituse_jsr!100
	ldah $29,0($26)		!gpdisp!101
	lda $29,0($29)		!gpdisp!101
	br $31,$L287
	.end prepare_namespace
	.comm	ROOT_DEV,4,4
	.globl rd_doload
	.section	.init.data
	.align 2
	.type	rd_doload, @object
	.size	rd_doload, 4
rd_doload:
	.zero	4
	.section	.bss
	.type	root_device_name, @object
	.size	root_device_name, 64
root_device_name:
	.zero	64
	.type	saved_root_name, @object
	.size	saved_root_name, 64
saved_root_name:
	.zero	64
	.section	.init.data
	.align 3
	.type	root_mount_data, @object
	.size	root_mount_data, 8
root_mount_data:
	.zero	8
	.align 3
	.type	root_fs_names, @object
	.size	root_fs_names, 8
root_fs_names:
	.zero	8
	.ident	"GCC: (GNU) 3.3 20021103 (experimental)"
