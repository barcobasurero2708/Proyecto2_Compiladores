.data
print_fmt:.string "%ld\n" 
.text
.globl main
contar_multiplos:
pushq %rbp
movq %rsp, %rbp
subq $32, %rsp
movq %rdi, -8(%rbp)
movq $0, -16(%rbp)
movq $0, -24(%rbp)
movq $1, %rax
movq %rax, -16(%rbp)
movq $0, %rax
movq %rax, -24(%rbp)
while_0:
movq $1, %rax
cmpq $0, %rax
je endwhile_0
movq -8(%rbp), %rax
pushq %rax
movq -16(%rbp), %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
setle %al
movzbq %al, %rax
cmpq $0, %rax
je else_1
jmp endwhile_0
jmp endif_1
else_1:
endif_1:
movq -24(%rbp), %rax
pushq %rax
pushq $0
movq -16(%rbp), %rax
pushq %rax
movq -16(%rbp), %rax
pushq %rax
movq $2, %rax
movq %rax, %rcx
popq %rax
cqto
idivq %rcx
pushq %rax
movq $2, %rax
movq %rax, %rcx
popq %rax
imulq %rcx, %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
setle %al
movzbq %al, %rax
addq %rax, (%rsp)
movq $1, %rax
addq %rax, (%rsp)
popq %rax
movq %rax, %rcx
popq %rax
addq %rcx, %rax
pushq %rax
movq $1, %rax
movq %rax, %rcx
popq %rax
subq %rcx, %rax
movq %rax, -24(%rbp)
movq -16(%rbp), %rax
pushq %rax
movq $1, %rax
movq %rax, %rcx
popq %rax
addq %rcx, %rax
movq %rax, -16(%rbp)
jmp while_0
endwhile_0:
movq -24(%rbp), %rax
jmp .Lfin_contar_multiplos
movq $0, %rax
.Lfin_contar_multiplos:
leave
ret
main:
pushq %rbp
movq %rsp, %rbp
subq $16, %rsp
movq $0, -8(%rbp)
movq $0, -16(%rbp)
movq $10, %rax
pushq %rax
popq %rdi
call contar_multiplos
movq %rax, -8(%rbp)
movq -8(%rbp), %rax
movq %rax, %rsi
leaq print_fmt(%rip), %rdi
movl $0, %eax
call printf@PLT
movq $20, %rax
pushq %rax
popq %rdi
call contar_multiplos
movq %rax, -16(%rbp)
movq -16(%rbp), %rax
movq %rax, %rsi
leaq print_fmt(%rip), %rdi
movl $0, %eax
call printf@PLT
movq $0, %rax
jmp .Lfin_main
movq $0, %rax
.Lfin_main:
movq $0, %rax
leave
ret
.section .note.GNU-stack,"",@progbits
