
.code


myproc PROC C dst_color: QWORD 

mov rdi, dst_color
mov rsi, scr_color
movd mm0, dword ptr[rdi]
paddusb mm0, [rsi]
movd dword ptr[rdi], mm0

ret

myproc ENDP


END
