#include <linux/linkage.h>
#include <mach/hardware.h>
#include <asm/ptrace.h>
ENTRY(move2esram)

// address of  sleep_esram () (r0)
		mov r1, r30
// jump to power-off code
		ldw r2, =0x40f05000

//store F64
                lfm.w (f0,f1,f2,f3,f4,f5,f6,f7), [r2+]
                lfm.w (f8,f9,f10,f11,f12,f13,f14,f15), [r2+]
                lfm.w (f16,f17,f18,f19,f20,f21,f22,f23), [r2+]
                lfm.w (f24,f25,f26,f27,f28,f29,f30,f31), [r2+]

//store r4-r28:
      		stm.w (r4-r15), [r2+]
      		stm.w (r16-r28), [r2+]
		
//Save MODE registers
//              mov r3, #0xd3
//              mov.a asr, r3           @priv(SVC)
                /***************/
                mov r0, bsr
                add r2, r2, #4
                stw.w r0, [r2]
                /***************/
                stm.w (r29-r30), [r2+]

                mov r3, #0xd1
                mov.a asr, r3           @real(FIQ)
                /***************/
                mov r0, bsr
                add r2, r2, #4
                stw.w r0, [r2]
                /***************/
                stm.w (r24-r30), [r2+]

                mov r3, #0xd7
                mov.a asr, r3           @trap(ABT)
                /***************/
                mov r0, bsr
                add r2, r2, #4
                stw.w r0, [r2]
                /***************/
                stm.w (r29-r30), [r2+]

                mov r3, #0xd2
                mov.a asr, r3           @int(IRQ)
                /***************/
                mov r0, bsr
                add r2, r2, #4
                stw.w r0, [r2]
                /***************/
                stm.w (r29-r30), [r2+]

                mov r3, #0xdb
                mov.a asr, r3           @ext(UND)
                /***************/
                mov r0, bsr
                add r2, r2, #4
                stw.w r0, [r2]
                /***************/
                stm.w (r29-r30), [r2+]

                mov r3, #0xdf
                mov.a asr, r3           @sys(SYS)
                /***************/
                mov r0, bsr
                add r2, r2, #4
                stw.w r0, [r2]
                /***************/
                stm.w (r29-r30), [r2+]


// back-to priv(SVC) mode
                andn r3, r3, #0xf
                or r3, r3, #0x3
                mov.a asr, r3
                /***************/
                mov r0, bsr
                add r2, r2, #4
                stw.w r0, [r2]
                /***************/
                stm.w (r29-r30), [r2+]

//##########################################################        
                                                                   
//3. flush cache                                                   
                mov ip , #0                                        
                movc p0.c5,ip,#14                                  
                nop                                                
                nop                                                
                nop                                                
                nop                                                
                nop                                                
                nop                                                
                nop                                                
                nop  



//store  p0.c2 
      		add r2, r2, #4		
      		movc r0, p0.c2, #0
      		stw r0, [r2+]

// store  p0.c1 
      		add r2, r2, #4		
      		movc r0, p0.c1, #0
      		stw r0, [r2+]
		
		nop
		nop
		nop
		nop
		nop
// Move to esram

		mov	pc, r1

