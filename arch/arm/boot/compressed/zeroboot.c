//------------------------------------------------------------------------------
// 화일명 : snapshot_boot.c
// 설  명 : ezBoot의 스냅샷 부팅을 시작한다.
// 
// 작성자 : 유영창 에프에이리눅스(주) frog@falinux.com
// 작성일 : 2001년 10월 8일
// 저작권 : 에프에이리눅스(주)
// 주  의 : 
//------------------------------------------------------------------------------

//******************************************************************************
//
// 헤더 정의
//
//******************************************************************************
// #include <typedef.h>
//#include <arch.h>

// unsigned int __machine_arch_type;

#define _LINUX_STRING_H_

#include <linux/compiler.h>	/* for inline */
#include <linux/types.h>	/* for size_t */
#include <linux/stddef.h>	/* for NULL */
#include <linux/linkage.h>
#include <asm/string.h>

#include <mach/uncompress.h>   

//******************************************************************************
// 참조 하는 것들
// ZBI_CPU_SAVE_SIZE	- zbi_t
// ZBI_CO_SAVE_SIZE		- zbi_t
// zbi_copy_info_t 		- zbi_t
// zbi_t				- zbi_t
// ZBI_NAND_START_PAGE	- zbi 헤더의 첫 페이지를 어디에서 읽을 것인가
// ZBI_PADDR			- zbi 헤더를 물리번지 어디에 위치시킬 것인가
// ZBI_VADDR			- 복귀시 인자
// PAGE_SIZE			- page size
#include "zeroboot.h"
//******************************************************************************

#define printf(args...)	do { } while(0)

// extern void zeroboot_read_4k_page( int nand_page, unsigned char *mem_page );
void zeroboot_read_4k_page( int nand_page, unsigned char *mem_page )
{
}

static zbi_t	*zbi = (zbi_t *) ZBI_PADDR;

//******************************************************************************
//
// 함수 정의
//
//******************************************************************************

void put_raw_char( char ch )
{
	putc(ch);
}

void put_raw_hex( u32 data )
{
	data = data & 0xF;
	if     ( data < 0xA ) data = data + '0';
	else				  data = data - 0xA + 'A';	
	
	put_raw_char( data );	
}

void put_raw_hex32( u32 data )
{
	int lp;
	
	for( lp = 7; lp >= 0; lp-- )
		put_raw_hex( data >> (4*lp) );
}

void put_raw_str( char *str )
{
	while(*str) 
	{
		if( *str == '\n' ) put_raw_char( '\r' );	
		put_raw_char( *str++ );	
	}	
}

void zb_read_zbi_header( void )
{
	int		lp;
	u32   	nand;
	char  	*data;
	u32		zbi_used;
	
	nand = ZBI_NAND_START_PAGE;
	data = (char *) zbi;

	// Read zbi first page
	zeroboot_read_4k_page( nand, data );
	nand++;nand++;
	data += PAGE_SIZE;

	// calculate how page used
	zbi_used = (u32)(&zbi->copy_data[zbi->copy_count+1]) - (u32)zbi;
	zbi_used = (zbi_used+(PAGE_SIZE-1))/PAGE_SIZE;
	if(zbi_used>0) zbi_used--;

	//for( lp = 0; lp < ZBI_UNIT_SIZE/PAGE_SIZE; lp++ )
	for( lp = 0; lp < zbi_used; lp++ )
	{
		zeroboot_read_4k_page( nand, data );
		nand++;nand++;
		data += PAGE_SIZE;
	}	
	
//	put_raw_str( "ZBI nand start page = "); put_raw_hex32( ZBI_NAND_START_PAGE  ); put_raw_str( "\n");
//	put_raw_str( "> magic             = "); put_raw_hex32( zbi->magic           ); put_raw_str( "\n");
//	put_raw_str( "> version           = "); put_raw_hex32( zbi->version         ); put_raw_str( "\n");
//	put_raw_str( "> fault_page_info   = "); put_raw_hex32( zbi->fault_page_info ); put_raw_str( "\n");
//	put_raw_str( "> jump              = "); put_raw_hex32( zbi->jump_vaddr      ); put_raw_str( "\n");

}

void zb_copy_data( void )
{
	zbi_copy_info_t	*copy_data;
	u32 			index;
	u32 			nand_page;
	volatile u8 	*page_buf;

//	u32				checksum_lp;
//	u32 			check_sum;
	
    put_raw_str( "> copy_count    = "); put_raw_hex32( zbi->copy_count ); put_raw_str( "\n");
    put_raw_str( "> copy_checksum = "); put_raw_hex32( zbi->copy_checksum ); put_raw_str( "\n");

//	check_sum = 0;

    copy_data = zbi->copy_data;
    
    for( index = 0; index < zbi->copy_count; index++ )
    {
//    	put_raw_str("> Bootcopy " );
//    	put_raw_str( "I = "); put_raw_hex32( index );
//    	put_raw_str( "P = "); put_raw_hex32( copy_data->dest_paddr );
//    	put_raw_str( ", N = "); put_raw_hex32( copy_data->nand_page_offset );
//    	put_raw_str( "\n" );

    	nand_page = ZBI_NAND_START_PAGE + copy_data->nand_page_offset;
		page_buf  = (u8 *) copy_data->dest_paddr;
		zeroboot_read_4k_page( nand_page, (unsigned char * ) page_buf );

//		for( checksum_lp = 0; checksum_lp < PAGE_SIZE; checksum_lp++ )
//		{
//			check_sum += page_buf[checksum_lp];
//		}
		
    	copy_data++;
    }
    
//    put_raw_str( "> CHECKSUM : "); put_raw_hex32( check_sum ); put_raw_str( "\n");
    
}

/*------------------------------------------------------------------------------
  @brief   코프로세서 상태를 복구한다.
  @remark  
*///----------------------------------------------------------------------------
u32 coprocessor_restore( u8 *co_load_buffer, u32 jump_vaddr, u32 dummy2, u32 *cpu_load_buffer );
//                           r0                  r1              r2          r3
asm("   	       					                \n\
.align  5						                    \n\
.text                                               \n\
.global coprocessor_restore                         \n\
coprocessor_restore:                                \n\
                                                    \n\
    ldr r2, [r0], #4                                \n\
    str r2, save_cr                                 \n\
 @	mcr p15, 0, r2, c1,  c0,  0		@ CR            \n\
                                                    \n\
    ldr r2, [r0], #4                                \n\
	mcr p15, 0, r2, c1,  c0,  1		@ Auxiliary     \n\
                                                    \n\
    ldr r2, [r0], #4                                \n\
	mcr p15, 0, r2, c1,  c0,  2		@ CACR          \n\
    ldr r2, [r0], #4                                \n\
	mcr p15, 0, r2, c2,  c0,  0		@ TTB_0R        \n\
    ldr r2, [r0], #4                                \n\
	mcr p15, 0, r2, c2,  c0,  1		@ TTB_1R        \n\
    ldr r2, [r0], #4                                \n\
	mcr p15, 0, r2, c2,  c0,  2		@ TTBCR         \n\
    ldr r2, [r0], #4                                \n\
	mcr p15, 0, r2, c3,  c0,  0		@ DACR          \n\
    ldr r2, [r0], #4                                \n\
	mcr p15, 0, r2, c5,  c0,  0		@ D_FSR         \n\
    ldr r2, [r0], #4                                \n\
	mcr p15, 0, r2, c5,  c0,  1		@ I_FSR         \n\
    ldr r2, [r0], #4                                \n\
	mcr p15, 0, r2, c6,  c0,  0		@ D_FAR         \n\
    ldr r2, [r0], #4                                \n\
	mcr p15, 0, r2, c6,  c0,  2		@ I_FAR         \n\
    ldr r2, [r0], #4                                \n\
	mcr p15, 0, r2, c7,  c4,  0		@ PAR           \n\
    ldr r2, [r0], #4                                \n\
	mcr p15, 0, r2, c10, c0,  0		@ D_TLBLR       \n\
    ldr r2, [r0], #4                                \n\
	mcr p15, 0, r2, c10, c2,  0		@ PRRR          \n\
    ldr r2, [r0], #4                                \n\
	mcr p15, 0, r2, c10, c2,  1		@ NRRR          \n\
    ldr r2, [r0], #4                                \n\
	mcr p15, 0, r2, c11, c1,  0		@ PLEUAR        \n\
    ldr r2, [r0], #4                                \n\
	mcr p15, 0, r2, c11, c2,  0		@ PLECNR        \n\
    ldr r2, [r0], #4                                \n\
	mcr p15, 0, r2, c11, c4,  0		@ PLECR         \n\
    ldr r2, [r0], #4                                \n\
	mcr p15, 0, r2, c11, c5,  0		@ PLEISAR       \n\
    ldr r2, [r0], #4                                \n\
	mcr p15, 0, r2, c11, c7,  0		@ PLEIEAR       \n\
    ldr r2, [r0], #4                                \n\
	mcr p15, 0, r2, c11, c15, 0		@ PLECIDR       \n\
    ldr r2, [r0], #4                                \n\
	mcr p15, 0, r2, c12, c0,  0		@ SNSVBAR       \n\
    ldr r2, [r0], #4                                \n\
	mcr p15, 0, r2, c13, c0,  0		@ FCSE          \n\
    ldr r2, [r0], #4                                \n\
	mcr p15, 0, r2, c13, c0,  1		@ CID           \n\
    ldr r2, [r0], #4                                \n\
	mcr p15, 0, r2, c13, c0,  2		@ URWTPID       \n\
    ldr r2, [r0], #4                                \n\
	mcr p15, 0, r2, c13, c0,  3		@ UROTPID       \n\
    ldr r2, [r0], #4                                \n\
	mcr p15, 0, r2, c13, c0,  4		@ POTPID        \n\
   	ldr r2, [r0], #4                                \n\
                                                    \n\
.align	5                                           \n\
                                                    \n\
	mov	r0, r3                                      \n\
    ldr r2, save_cr                                 \n\
    mcr p15, 0, r2, c1 , c0 , 0     @  CR           \n\
	mrc	p15, 0, r2, c0, c0, 0		@ read id reg   \n\
	mov	r2, r2                                      \n\
	mov	pc, r1                                      \n\
                                                    \n\
                                                    \n\
save_cr: .word	0x0		@ CR                        \n\
                                                    \n\
                                                    \n\
preboot: .word	0x50090004							\n\
");

void zeroboot( void )
{
	volatile u32 *gph3dat;
	volatile u32 *keyifrow;
	
	gph3dat 	= (volatile u32 *) 0xE0200C64;
	
	// CHECK ZEROBOOT MODE
	if( ( *gph3dat & (1<<1) ) ) return;

	put_raw_str( "\nZeroBoot Start\n" );

	while(1);	

 	zbi = (zbi_t *) ZBI_PADDR;
 	zb_read_zbi_header();
 	zb_copy_data();
 
 	put_raw_str( "ZeroBoot Copy End\n" );
 	
 	coprocessor_restore( zbi->co_save, 
 	                     zbi->jump_vaddr, 
 	                     0, 
 	                     (zbi->cpu_save - ZBI_PADDR)+ZBI_VADDR );
 
 	put_raw_str( "ZeroBoot Oops Bug\n" );
 	while(1);
	
}

