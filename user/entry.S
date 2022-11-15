.code64
.section .text
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