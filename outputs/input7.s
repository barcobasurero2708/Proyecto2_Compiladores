.data
print_fmt:.string "%ld\n" 
.text
.globl main
menor3:
pushq %rbp
movq %rsp, %rbp
subq $32, %rsp
movq %rdi, -8(%rbp)
movq %rsi, -16(%rbp)
movq %rdx, -24(%rbp)
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
jmp .Lfin_menor3
movq $0, %rax
.Lfin_menor3:
leave
ret
mayor3:
pushq %rbp
movq %rsp, %rbp
subq $32, %rsp
movq %rdi, -8(%rbp)
movq %rsi, -16(%rbp)
movq %rdx, -24(%rbp)
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
jmp .Lfin_mayor3
movq $0, %rax
.Lfin_mayor3:
leave
ret
medio3:
pushq %rbp
movq %rsp, %rbp
subq $32, %rsp
movq %rdi, -8(%rbp)
movq %rsi, -16(%rbp)
movq %rdx, -24(%rbp)
movq -8(%rbp), %rax
pushq %rax
movq -16(%rbp), %rax
movq %rax, %rcx
popq %rax
addq %rcx, %rax
pushq %rax
movq -24(%rbp), %rax
movq %rax, %rcx
popq %rax
addq %rcx, %rax
pushq %rax
movq -8(%rbp), %rax
pushq %rax
movq -16(%rbp), %rax
pushq %rax
movq -24(%rbp), %rax
pushq %rax
popq %rdx
popq %rsi
popq %rdi
subq $8, %rsp
call menor3
addq $8, %rsp
movq %rax, %rcx
popq %rax
subq %rcx, %rax
pushq %rax
movq -8(%rbp), %rax
pushq %rax
movq -16(%rbp), %rax
pushq %rax
movq -24(%rbp), %rax
pushq %rax
popq %rdx
popq %rsi
popq %rdi
subq $8, %rsp
call mayor3
addq $8, %rsp
movq %rax, %rcx
popq %rax
subq %rcx, %rax
jmp .Lfin_medio3
movq $0, %rax
.Lfin_medio3:
leave
ret
verificar_orden:
pushq %rbp
movq %rsp, %rbp
subq $32, %rsp
movq %rdi, -8(%rbp)
movq %rsi, -16(%rbp)
movq %rdx, -24(%rbp)
pushq $0
movq -8(%rbp), %rax
pushq %rax
movq -16(%rbp), %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
setle %al
movzbq %al, %rax
addq %rax, (%rsp)
movq -16(%rbp), %rax
pushq %rax
movq -24(%rbp), %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
setle %al
movzbq %al, %rax
addq %rax, (%rsp)
movq -8(%rbp), %rax
pushq %rax
movq -24(%rbp), %rax
movq %rax, %rcx
popq %rax
cmpq %rcx, %rax
setle %al
movzbq %al, %rax
addq %rax, (%rsp)
popq %rax
jmp .Lfin_verificar_orden
movq $0, %rax
.Lfin_verificar_orden:
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
movq $48, %rax
pushq %rax
movq $17, %rax
pushq %rax
movq $33, %rax
pushq %rax
popq %rdx
popq %rsi
popq %rdi
call menor3
movq %rax, -8(%rbp)
movq $48, %rax
pushq %rax
movq $17, %rax
pushq %rax
movq $33, %rax
pushq %rax
popq %rdx
popq %rsi
popq %rdi
call medio3
movq %rax, -16(%rbp)
movq $48, %rax
pushq %rax
movq $17, %rax
pushq %rax
movq $33, %rax
pushq %rax
popq %rdx
popq %rsi
popq %rdi
call mayor3
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
call verificar_orden
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
