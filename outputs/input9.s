.data
print_fmt:.string "%ld\n" 
.text
.globl main
evaluar_alertas:
pushq %rbp
movq %rsp, %rbp
subq $32, %rsp
movq %rdi, -8(%rbp)
movq %rsi, -16(%rbp)
movq %rdx, -24(%rbp)
pushq $0
movq $75, %rax
pushq %rax
movq -8(%rbp), %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
setle %al
movzbq %al, %rax
addq %rax, (%rsp)
movq $70, %rax
pushq %rax
movq -16(%rbp), %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
setle %al
movzbq %al, %rax
addq %rax, (%rsp)
movq -24(%rbp), %rax
pushq %rax
movq $1000, %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
setle %al
movzbq %al, %rax
addq %rax, (%rsp)
popq %rax
jmp .Lfin_evaluar_alertas
movq $0, %rax
.Lfin_evaluar_alertas:
leave
ret
calcular_prioridad:
pushq %rbp
movq %rsp, %rbp
subq $32, %rsp
movq %rdi, -8(%rbp)
movq %rsi, -16(%rbp)
movq %rdx, -24(%rbp)
movq $2, %rax
pushq %rax
movq -8(%rbp), %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
setle %al
movzbq %al, %rax
cmpq $0, %rax
je else_0
movq -16(%rbp), %rax
pushq %rax
movq $70, %rax
movq %rax, %rcx
popq %rax
subq %rcx, %rax
pushq %rax
movq -24(%rbp), %rax
pushq %rax
movq $50, %rax
movq %rax, %rcx
popq %rax
subq %rcx, %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
cmovl %rcx, %rax
pushq %rax
movq $10, %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
cmovl %rcx, %rax
jmp .Lfin_calcular_prioridad
jmp endif_0
else_0:
movq -16(%rbp), %rax
pushq %rax
movq -24(%rbp), %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
cmovg %rcx, %rax
pushq %rax
movq $5, %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
cmovg %rcx, %rax
jmp .Lfin_calcular_prioridad
endif_0:
movq $0, %rax
.Lfin_calcular_prioridad:
leave
ret
main:
pushq %rbp
movq %rsp, %rbp
subq $16, %rsp
movq $0, -8(%rbp)
movq $0, -16(%rbp)
movq $82, %rax
pushq %rax
movq $75, %rax
pushq %rax
movq $1015, %rax
pushq %rax
popq %rdx
popq %rsi
popq %rdi
call evaluar_alertas
movq %rax, -8(%rbp)
movq -8(%rbp), %rax
movq %rax, %rsi
leaq print_fmt(%rip), %rdi
movl $0, %eax
call printf@PLT
movq -8(%rbp), %rax
pushq %rax
movq $82, %rax
pushq %rax
movq $75, %rax
pushq %rax
popq %rdx
popq %rsi
popq %rdi
call calcular_prioridad
movq %rax, -16(%rbp)
movq -16(%rbp), %rax
movq %rax, %rsi
leaq print_fmt(%rip), %rdi
movl $0, %eax
call printf@PLT
movq -16(%rbp), %rax
pushq %rax
movq $2, %rax
movq %rax, %rcx
popq %rax
imulq %rcx, %rax
pushq %rax
movq -8(%rbp), %rax
movq %rax, %rcx
popq %rax
addq %rcx, %rax
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
