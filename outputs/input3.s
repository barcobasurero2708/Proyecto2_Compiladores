.data
print_fmt:.string "%ld\n" 
.text
.globl main
evaluar_notas:
pushq %rbp
movq %rsp, %rbp
subq $48, %rsp
movq %rdi, -8(%rbp)
movq %rsi, -16(%rbp)
movq %rdx, -24(%rbp)
movq %rcx, -32(%rbp)
movq $0, -40(%rbp)
pushq $0
movq $60, %rax
pushq %rax
movq -8(%rbp), %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
setle %al
movzbq %al, %rax
addq %rax, (%rsp)
movq $60, %rax
pushq %rax
movq -16(%rbp), %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
setle %al
movzbq %al, %rax
addq %rax, (%rsp)
movq $60, %rax
pushq %rax
movq -24(%rbp), %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
setle %al
movzbq %al, %rax
addq %rax, (%rsp)
movq $60, %rax
pushq %rax
movq -32(%rbp), %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
setle %al
movzbq %al, %rax
addq %rax, (%rsp)
popq %rax
movq %rax, -40(%rbp)
movq -40(%rbp), %rax
jmp .Lfin_evaluar_notas
movq $0, %rax
.Lfin_evaluar_notas:
leave
ret
es_sobresaliente:
pushq %rbp
movq %rsp, %rbp
subq $48, %rsp
movq %rdi, -8(%rbp)
movq %rsi, -16(%rbp)
movq %rdx, -24(%rbp)
movq %rcx, -32(%rbp)
movq $0, -40(%rbp)
pushq $0
movq $85, %rax
pushq %rax
movq -8(%rbp), %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
setle %al
movzbq %al, %rax
addq %rax, (%rsp)
movq $85, %rax
pushq %rax
movq -16(%rbp), %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
setle %al
movzbq %al, %rax
addq %rax, (%rsp)
movq $85, %rax
pushq %rax
movq -24(%rbp), %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
setle %al
movzbq %al, %rax
addq %rax, (%rsp)
movq $85, %rax
pushq %rax
movq -32(%rbp), %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
setle %al
movzbq %al, %rax
addq %rax, (%rsp)
popq %rax
movq %rax, -40(%rbp)
movq $2, %rax
pushq %rax
movq -40(%rbp), %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
setle %al
movzbq %al, %rax
jmp .Lfin_es_sobresaliente
movq $0, %rax
.Lfin_es_sobresaliente:
leave
ret
main:
pushq %rbp
movq %rsp, %rbp
subq $32, %rsp
movq $0, -8(%rbp)
movq $0, -16(%rbp)
movq $0, -24(%rbp)
movq $0, -32(%rbp)
movq $85, %rax
pushq %rax
movq $42, %rax
pushq %rax
movq $90, %rax
pushq %rax
movq $58, %rax
pushq %rax
popq %rcx
popq %rdx
popq %rsi
popq %rdi
call evaluar_notas
movq %rax, -8(%rbp)
movq -8(%rbp), %rax
movq %rax, %rsi
leaq print_fmt(%rip), %rdi
movl $0, %eax
call printf@PLT
movq $85, %rax
pushq %rax
movq $42, %rax
pushq %rax
movq $90, %rax
pushq %rax
movq $58, %rax
pushq %rax
popq %rcx
popq %rdx
popq %rsi
popq %rdi
call es_sobresaliente
movq %rax, -32(%rbp)
movq -32(%rbp), %rax
cmpq $0, %rax
je ifexp_else_0
movq $100, %rax
jmp ifexp_end_0
ifexp_else_0:
movq $0, %rax
ifexp_end_0:
movq %rax, %rsi
leaq print_fmt(%rip), %rdi
movl $0, %eax
call printf@PLT
movq $85, %rax
pushq %rax
movq $42, %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
cmovg %rcx, %rax
pushq %rax
movq $90, %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
cmovg %rcx, %rax
pushq %rax
movq $58, %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
cmovg %rcx, %rax
movq %rax, -16(%rbp)
movq $85, %rax
pushq %rax
movq $42, %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
cmovl %rcx, %rax
pushq %rax
movq $90, %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
cmovl %rcx, %rax
pushq %rax
movq $58, %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
cmovl %rcx, %rax
movq %rax, -24(%rbp)
movq -16(%rbp), %rax
movq %rax, %rsi
leaq print_fmt(%rip), %rdi
movl $0, %eax
call printf@PLT
movq -24(%rbp), %rax
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
