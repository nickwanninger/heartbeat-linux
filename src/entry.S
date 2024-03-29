.code64
.section .text
.global _hb_entry
_hb_entry:

	pushf
  pushq %r15
  pushq %r14
  pushq %r13
  pushq %r12
  pushq %r11
  pushq %rbp
  pushq %rcx
  pushq %rbx
  pushq %r9
  pushq %r8
  pushq %r10
  pushq %rdx
  pushq %rsi
  pushq %rdi
  pushq %rax

	movq %rsp, %rdi

	# call into the high half after we save the state
	call hb_entry_high

  popq %rax
  popq %rdi
  popq %rsi
  popq %rdx
  popq %r10
  popq %r8
  popq %r9
  popq %rbx
  popq %rcx
  popq %rbp
  popq %r11
  popq %r12
  popq %r13
  popq %r14
  popq %r15
	popf

	popq %rsp

	# %rsp is now the interrupted context's rsp, so we cannot use
	# `ret` to go back to the old contex
	jmp *-128(%rsp)

.global hb_test
.global hb_test_target
.global hb_test_target_rf
hb_test:
	movq $0, %rax
.loop:
	inc %rax
hb_test_target:
	jmp .loop

hb_test_target_rf:
	ret
