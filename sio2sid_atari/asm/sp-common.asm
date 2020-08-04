; ASM Routines

.export _siov

_siov:
	cli
	jsr $E459
	sei
	rts
