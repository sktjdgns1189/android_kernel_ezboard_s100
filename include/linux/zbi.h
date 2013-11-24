/*-----------------------------------------------------------------------------
  파 일 : zbi.h
  설 명 : 
  작 성 : frog@falinux.com
  날 짜 : 2011-07-29
  주 의 :

-------------------------------------------------------------------------------*/

#ifndef _ZBI_H_
#define _ZBI_H_

#define ZBI_MAGIC										0xFA0000B2
#define ZBI_VERSION										100

#define ZBI_CPU_SAVE_SIZE								512
#define ZBI_CO_SAVE_SIZE								512

#define ZBI_NAND_START_PAGE								(256*1024*1024/2048)	// FIXME 
#define ZBI_NAND_SIZE_MAX								(256*1024*1024)			// FIXME

#define ZBI_L2_FAULT_MARK(paddr)						(paddr|0x2E); 			// FIXME
																				// nG  S APX  TEX AP C B 1 XN
		                                   										//  0  0   0   00 10 1 1 1  0 = 0 000 0010 1110 = 0x2E
#ifndef PAGE_SIZE
#define PAGE_SIZE										4096
#endif

#define ZBI_PADDR										0x50008000				// FIXME PHYS_OFFSET + 0x8000
#define ZBI_VADDR										0xC0008000				// FIXME 

#define ZBI_DRAM_MMU_L1_INDEX							0xC00
	
#define	KERNEL_VECTOR_TABLE_VADDR						0xFFFF0000
#define KERNEL_SWAPPER_PG_DIR_VADDR						0xC0004000

#define ZBI_ATTR_NONE                      				0 
#define ZBI_ATTR_MMU_PTE                   				(1<<0)

typedef struct
{
	u32 		dest_paddr;
	u16 		nand_page_offset;
	u16 		attr;
} zbi_copy_info_t;

#define ZBI_UNIT_SIZE									(4*PAGE_SIZE)			// FIXME
#define ZBI_HEADER_MAX									(ZBI_CPU_SAVE_SIZE+ZBI_CO_SAVE_SIZE+1024)
#define ZBI_COPY_MAX									((ZBI_UNIT_SIZE-ZBI_HEADER_MAX)/sizeof(zbi_copy_info_t))	

typedef struct
{
	union	{
		struct {
			u32 		magic;		// ZBI_MAGIC
			u32  		version;    // ZBI_VERSION
			u8  		cpu_save	[ZBI_CPU_SAVE_SIZE];
			u8   		co_save		[ZBI_CO_SAVE_SIZE];
			
			u32			nand_base_page;
			
			u32			co_save_size;
			u32			cpu_save_size;
			
			u32			copy_count;
			u32       	copy_checksum;
			
			u32			jump_vaddr;

			u32			fault_nand_io_vaddr;

			// 이전의 항목들은 변경하지 말것
			// 부트로더와 연동이 되어 있어 변경시 부트로더를 함께 변경해야 함
			u32			dram_size;
			u32			page_size;
		};
		
		u8	align_2048[2048];
	};
	
	zbi_copy_info_t		copy_data	[ZBI_COPY_MAX];
} zbi_t;

extern void 		zbi_init							( void );
extern void 		zbi_free							( void );
                                                		
extern int 			zbi_append_bootcopy_data			( u32 dest_paddr, u32 attr );

extern void     	zbi_build_nand_page 				( void );
extern void     	zbi_write_info_of_bootcopy_to_zbi	( void );
extern void			zbi_build_nand_page_for_m2n_data	( void *m2n_data, u32 used_count );
                                                		
extern int 			zbi_save_coprocessor				( u8 *coprocessor_data , u32 size );
extern int 			zbi_save_cpu						( u8 *cpu_data , u32 size );
extern int 			zbi_jump_vaddr						( u32 vaddr );
extern void 		zbi_set_nand_io						( u32 vaddr );

extern void 		zbi_save_data_for_zbi_data			( void );
extern void 		zbi_save_for_m2n_data				( void *m2n_data, u32 used_count );
                                                		
extern void 		zbi_save_header						( void );
extern void 		zbi_display							( void );

extern u32 (*nalcode_func)( u32 function_offset, u32 arg1, u32 arg2, u32 arg3 );

#endif  // _ZBI_H_

