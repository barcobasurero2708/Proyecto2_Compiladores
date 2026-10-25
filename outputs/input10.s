.data
print_fmt:.string "%ld\n" 
.text
.globl main
fib_spread:
pushq %rbp
movq %rsp, %rbp
subq $48, %rsp
movq %rdi, -8(%rbp)
movq $0, -16(%rbp)
movq $0, -24(%rbp)
movq $0, -32(%rbp)
movq $0, -40(%rbp)
movq $0, -48(%rbp)
movq $1, %rax
movq %rax, -16(%rbp)
movq $1, %rax
movq %rax, -24(%rbp)
movq $0, %rax
movq %rax, -40(%rbp)
movq $0, %rax
movq %rax, -48(%rbp)
while_0:
movq -40(%rbp), %rax
pushq %rax
movq -8(%rbp), %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
setle %al
movzbq %al, %rax
cmpq $0, %rax
je endwhile_0
movq -16(%rbp), %rax
pushq %rax
movq -24(%rbp), %rax
movq %rax, %rcx
popq %rax
addq %rcx, %rax
movq %rax, -32(%rbp)
movq -24(%rbp), %rax
movq %rax, -16(%rbp)
movq -32(%rbp), %rax
movq %rax, -24(%rbp)
movq -48(%rbp), %rax
pushq %rax
movq -16(%rbp), %rax
pushq %rax
movq -24(%rbp), %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
cmovl %rcx, %rax
movq %rax, %rcx
popq %rax
addq %rcx, %rax
pushq %rax
movq -16(%rbp), %rax
pushq %rax
movq -24(%rbp), %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
cmovg %rcx, %rax
movq %rax, %rcx
popq %rax
subq %rcx, %rax
movq %rax, -48(%rbp)
movq -40(%rbp), %rax
pushq %rax
movq $1, %rax
movq %rax, %rcx
popq %rax
addq %rcx, %rax
movq %rax, -40(%rbp)
jmp while_0
endwhile_0:
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
movq -48(%rbp), %rax
movq %rax, %rsi
leaq print_fmt(%rip), %rdi
movl $0, %eax
call printf@PLT
pushq $0
movq $10, %rax
pushq %rax
movq -16(%rbp), %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
setle %al
movzbq %al, %rax
addq %rax, (%rsp)
movq $20, %rax
pushq %rax
movq -24(%rbp), %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
setle %al
movzbq %al, %rax
addq %rax, (%rsp)
movq $50, %rax
pushq %rax
movq -48(%rbp), %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
setle %al
movzbq %al, %rax
addq %rax, (%rsp)
movq -40(%rbp), %rax
pushq %rax
movq $10, %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
setle %al
movzbq %al, %rax
addq %rax, (%rsp)
popq %rax
jmp .Lfin_fib_spread
movq $0, %rax
.Lfin_fib_spread:
leave
ret
main:
pushq %rbp
movq %rsp, %rbp
subq $16, %rsp
movq $0, -8(%rbp)
movq $5, %rax
pushq %rax
popq %rdi
call fib_spread
movq %rax, -8(%rbp)
movq -8(%rbp), %rax
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
