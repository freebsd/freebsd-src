/* HPPA linker stub instructions  */

/* These are the instructions which the linker may insert into the
   code stream when building final executables to handle out-of-range
   calls and argument relocations.  */

#define LDO_M4_R31_R31		0x37ff3ff9	/* ldo -4(%r31),%r31	  */
#define LDIL_R1			0x20200000	/* ldil XXX,%r1		  */
#define BE_SR4_R1		0xe0202000	/* be XXX(%sr4,%r1)	  */
#define COPY_R31_R2          	0x081f0242	/* copy %r31,%r2	  */
#define BLE_SR4_R0		0xe4002000	/* ble XXX(%sr4,%r0)	  */
#define BLE_SR4_R1		0xe4202000	/* ble XXX(%sr4,%r1)	  */
#define BV_N_0_R31		0xebe0c002	/* bv,n 0(%r31)		  */
#define STW_R31_M8R30		0x6bdf3ff1	/* stw %r31,-8(%r30)	  */
#define LDW_M8R30_R31		0x4bdf3ff1	/* ldw -8(%r30),%r31	  */
#define STW_ARG_M16R30		0x6bc03fe1	/* stw %argX,-16(%r30)	  */
#define LDW_M16R30_ARG		0x4bc03fe1	/* ldw -12(%r30),%argX	  */
#define STW_ARG_M12R30		0x6bc03fe9	/* stw %argX,-16(%r30)	  */
#define LDW_M12R30_ARG		0x4bc03fe9	/* ldw -12(%r30),%argX	  */
#define FSTW_FARG_M16R30	0x27c11200	/* fstws %fargX,-16(%r30) */
#define FLDW_M16R30_FARG	0x27c11000	/* fldws -16(%r30),%fargX */
#define FSTD_FARG_M16R30	0x2fc11200	/* fstds %fargX,-16(%r30) */
#define FLDD_M16R30_FARG	0x2fc11000	/* fldds -16(%r30),%fargX */
