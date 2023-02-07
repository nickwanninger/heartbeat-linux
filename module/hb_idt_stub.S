.code64
.extern hb_irq_handler
.extern hb_error_entry
.extern hb_error_return

.section .text
.global _hb_idt_entry


_hb_idt_entry:
		nopl (%rax)
		pushq $0xffffffffffffffff
		call *hb_error_entry
		mov %rsp, %rdi
		call hb_irq_handler
    jmp *hb_error_return 
    nop
    nop
    nop
    nop
    nop
