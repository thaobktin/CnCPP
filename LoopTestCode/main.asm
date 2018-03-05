TITLE MASM Template						(main.asm)

; Description:
; 
; Revision date:

INCLUDE Irvine32.inc
INCLUDE Macros.inc


;------------------------------------------------------
mCallSumProc MACRO SumProc:REQ
; Receives: SumProc, a summation procedure 
;           ECX as n, to calculate 1+2+...+n
;------------------------------------------------------
   push ecx
   call GetMseconds	; get start time
   mov	esi,eax
   call  SumProc
   mWrite "&SumProc: "
   call	WriteDec	
   call crlf

   call GetMseconds	; get start time
   sub	eax,esi
   call	WriteDec		; display elapsed time
   mWrite <' millisecond(s) used', 0Dh,0Ah, 0Dh,0Ah>
   pop ecx
ENDM


.code
main PROC
   call Clrscr
   mWrite "To calculate 1+2+...+n, please enter n (1 - 4294967295): "
   call ReadDec				      ; read value from user
   mov ecx, eax
   call  crlf

   mCallSumProc Using_LOOP
   mCallSumProc Using_DEC_JNE
   mCallSumProc Using_DEC_JECXZ_JMP
   call WaitMsg
   exit
main ENDP


;--------------------------------------------------------
Using_LOOP PROC
; Receives: ECX, as n, an integer to calculate 1+2+...+n
; Returns:  EAX, the sum of 1+2+...+n
;--------------------------------------------------------
   xor eax, eax
L1:
   add eax, ecx
   loop L1
   ret  
Using_LOOP ENDP

;--------------------------------------------------------
Using_DEC_JNE PROC
; Receives: ECX, as n, an integer to calculate 1+2+...+n
; Returns:  EAX, the sum of 1+2+...+n
;--------------------------------------------------------
   xor eax, eax
L1:
   add eax, ecx
   dec ecx        ; Two here equivalent to LOOP L1
   JNE L1
   ret  
Using_DEC_JNE ENDP

;--------------------------------------------------------
Using_DEC_JECXZ_JMP PROC
; Receives: ECX, as n, an integer to calculate 1+2+...+n
; Returns:  EAX, the sum of 1+2+...+n
;--------------------------------------------------------
   xor eax, eax
L1:
   add eax, ecx
   dec ecx        ; Three here quivalent to LOOP L1
   JECXZ L2
   jmp L1
L2:
   ret  
Using_DEC_JECXZ_JMP ENDP

END main