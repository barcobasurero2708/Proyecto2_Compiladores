.data
print_fmt:.string "%ld\n" 
.text
.globl main
clamp:
pushq %rbp
movq %rsp, %rbp
subq $32, %rsp
movq %rdi, -8(%rbp)
movq %rsi, -16(%rbp)
movq %rdx, -24(%rbp)
movq -16(%rbp), %rax
pushq %rax
movq -8(%rbp), %rax
pushq %rax
movq -24(%rbp), %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
cmovg %rcx, %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
cmovl %rcx, %rax
jmp .Lfin_clamp
movq $0, %rax
.Lfin_clamp:
leave
ret
span:
pushq %rbp
movq %rsp, %rbp
subq $48, %rsp
movq %rdi, -8(%rbp)
movq %rsi, -16(%rbp)
movq %rdx, -24(%rbp)
movq $0, -32(%rbp)
movq $0, -40(%rbp)
movq -8(%rbp), %rax
pushq %rax
movq -16(%rbp), %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
cmovg %rcx, %rax
pushq %rax
movq -24(%rbp), %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
cmovg %rcx, %rax
movq %rax, -32(%rbp)
movq -8(%rbp), %rax
pushq %rax
movq -16(%rbp), %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
cmovl %rcx, %rax
pushq %rax
movq -24(%rbp), %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
cmovl %rcx, %rax
movq %rax, -40(%rbp)
movq -40(%rbp), %rax
pushq %rax
movq -32(%rbp), %rax
movq %rax, %rcx
popq %rax
subq %rcx, %rax
jmp .Lfin_span
movq $0, %rax
.Lfin_span:
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
movq $15, %rax
pushq %rax
movq $10, %rax
pushq %rax
movq $20, %rax
pushq %rax
popq %rdx
popq %rsi
popq %rdi
call clamp
movq %rax, -8(%rbp)
movq $5, %rax
pushq %rax
movq $10, %rax
pushq %rax
movq $20, %rax
pushq %rax
popq %rdx
popq %rsi
popq %rdi
call clamp
movq %rax, -16(%rbp)
movq $25, %rax
pushq %rax
movq $10, %rax
pushq %rax
movq $20, %rax
pushq %rax
popq %rdx
popq %rsi
popq %rdi
call clamp
movq %rax, -24(%rbp)
movq -8(%rbp), %rax
movq %rax, %rsi
leaq print_fmt(%rip), %rdi
movl $0, %eax
call printf@PLT
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
movq -8(%rbp), %rax
pushq %rax
movq -16(%rbp), %rax
pushq %rax
movq -24(%rbp), %rax
pushq %rax
popq %rdx
popq %rsi
popq %rdi
call span
movq %rax, -32(%rbp)
movq -32(%rbp), %rax
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
