title The subProcs.asm Subroutine    (subProcs.asm)
; Description: Calling ASM procedures in C with different calling conventions
; Date:        01/19/2017
; Written by:  Zuoliu Ding
; (C) Copyright 2017- 2027, Zuoliu Ding. All Rights Reserved.  

.386P
.model flat  ; No convention  
.stack 4096

.code
;-----------------------------------------------------------------------
ProcC_CallWithParameterList PROC C, x:DWORD, y:DWORD
; Explicitly declared, C calling convention with Parameter list
; Receives: x and y as unsigned integers 
; Returns:  EAX, the result x-y
;-----------------------------------------------------------------------
    mov    eax, x      ; first argument 
    sub    eax, y      ; second argument
    ret               
ProcC_CallWithParameterList endp

;-----------------------------------------------------------------------
_ProcC_CallWithStackFrame PROC near 
; For __cdecl, manually making C calling convention with Stack Frame
; Receives: x and y on the Stack Frame 
; Returns:  EAX, the result x-y
;-----------------------------------------------------------------------
    push   ebp
    mov    ebp,esp

    mov    eax,[ebp+8]      ; first argument x
    sub    eax,[ebp+12]     ; second argument y

    pop    ebp
    ret               
_ProcC_CallWithStackFrame endp

;-----------------------------------------------------------------------
ProcSTD_CallWithParameterList PROC stdcall, x:DWORD, y:DWORD
; Explicitly declared, C calling convention with Parameter list
; Receives: x and y as unsigned integers 
; Returns:  EAX, the result x-y
;-----------------------------------------------------------------------
    mov    eax, x      ; first argument 
    sub    eax, y      ; second argument
    ret               
ProcSTD_CallWithParameterList endp

;-----------------------------------------------------------------------
_ProcSTD_CallWithStackFrame@8 PROC near 
; For __stdcall, manually making STD calling convention with Stack Frame
; Receives: x and y on the Stack Frame 
; Returns:  EAX, the result x-y
;-----------------------------------------------------------------------
    push   ebp
    mov    ebp,esp

    mov    eax,[ebp+8]      ; first argument x
    sub    eax,[ebp+12]     ; second argument y

    pop    ebp
    ret 8               
_ProcSTD_CallWithStackFrame@8 endp


ReadFromConsole PROTO C, by:BYTE
DisplayToConsole PROTO C, s:PTR BYTE, n:DWORD
;-----------------------------------------------------------------------
DoSubtraction PROC C
; Call C++ ReadFromConsole to read X, Y and DisplayToConsole show X-Y
;-----------------------------------------------------------------------
.data 
   text2Disp BYTE 'X-Y =', 0
   diff DWORD ?
.code
   INVOKE ReadFromConsole, 'X'
   mov diff, eax
   INVOKE ReadFromConsole, 'Y'
   sub diff, eax
   INVOKE DisplayToConsole, OFFSET text2Disp, diff
   ret
DoSubtraction endp

end
