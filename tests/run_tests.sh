#!/bin/sh
# Usage: tests/run_tests.sh path/to/rcx8asm
# Assembles small programs and compares the output file (as hex) with the expected bytes.
ASM=${1:?usage: run_tests.sh path/to/rcx8asm}
DIR=$(cd "$(dirname "$0")" && pwd)
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
fail=0

# check NAME "source" EXPECTED_HEX
check() {
	printf '%b' "$2" > "$TMP/t.asm"
	if "$ASM" "$TMP/t.asm" "$TMP/t.prg" > "$TMP/log" 2>&1; then
		got=$(od -An -v -tx1 "$TMP/t.prg" | tr -d ' \n')
	else
		got=FAIL
	fi
	if [ "$got" = "$3" ]; then echo "ok   $1"; else echo "FAIL $1: expected $3, got $got"; fail=1; fi
}

# check_error NAME "source" MESSAGE
check_error() {
	printf '%b' "$2" > "$TMP/t.asm"
	rm -f "$TMP/t.prg"
	if "$ASM" "$TMP/t.asm" "$TMP/t.prg" > "$TMP/log" 2>&1; then
		echo "FAIL $1: assembling should have failed"; fail=1
	elif grep -q "$3" "$TMP/log" && [ ! -e "$TMP/t.prg" ]; then
		echo "ok   $1"
	else
		echo "FAIL $1: expected message '$3'"; cat "$TMP/log"; fail=1
	fi
}

# image PC BLOCKS...: expected file for start address PC (little endian hex), blocks, eof
pre='0070'

check basic '.data 0x0100\nmain:\n nop\n hlt\n' "${pre}0001""0020""00010200""00ff""0000"
check lo_hi '.data 0x0200\nmain:\n fim R0 lo(tab)\n fim R1 hi(tab)\n hlt\ntab: db 1\n' "${pre}0002""0020""00020600""20052202ff01""0000"
check label_offset '.data 0x0100\nmain:\n jun main+3\n' "${pre}0001""0020""00010300""400301""0000"
check label_minus '.data 0x0100\nmain: nop\n jun main-1+2\n' "${pre}0001""0020""00010400""00400101""0000"
check ports '.data 0x0100\nmain:\n wmp 5\n rdr P7\n' "${pre}0001""0020""00010400""e105ea07""0000"
check db_dw_lists '.data 0x0100\nmain:\n hlt\n db "AB", 0\n dw 0x1234, main\n ds "x"\n' "${pre}0001""0020""00010a00""ff414200""34120001""7800""0000"
check negative_byte '.data 0x0100\nmain:\n db -1, 255\n' "${pre}0001""0020""00010200""ffff""0000"
check immediates '.data 0x0100\nmain:\n ldm 5\n bbl 3\n' "${pre}0001""0020""00010200""d5c3""0000"
check lowercase_hex '.data 0x0100\nmain:\n fim R0 0xab\n' "${pre}0001""0020""00010200""20ab""0000"
check label_named_lo '.data 0x0100\nlo:\nmain: jun lo+1\n' "${pre}0001""0020""00010300""400101""0000"

mkdir -p "$TMP/sub"
printf '.data 0x0200\nfunc:\n hlt\n' > "$TMP/sub/lib.asm"
check include '.include "sub/lib.asm"\n.data 0x0100\nmain:\n jun func\n' "${pre}0001""0020""000201""00ff""0020""00010300""400002""0000"

check_error undefined_label '.data 0x0100\nmain:\n jun nowhere\n' 'Label not found'
check_error value_range '.data 0x0100\nmain:\n fim R0 256\n' 'out of range'
check_error nibble_range '.data 0x0100\nmain:\n ldm 16\n' 'out of range'
check_error hi_range_ok_but_addr '.data 0x0100\nmain:\n jun 70000\n' 'out of range'
check_error unknown_instruction '.data 0x0100\nmain:\n foo R1\n' 'unknown instruction'
check_error bad_register '.data 0x0100\nmain:\n inc R8\n' 'register'
check_error missing_include '.include "nope.asm"\n' 'could not open'
check_error duplicate_label '.data 0x0100\nmain:\n nop\n.data 0x0200\nmain:\n nop\n' 'already defined'
check_error two_labels '.data 0x0100\nmain:\n jun main+main\n' 'only one'
check_error unclosed_paren '.data 0x0100\nmain:\n fim R0 hi(main\n' "expected ')'"
check_error no_main '.data 0x0100\nfoo:\n nop\n' "no 'main'"

exit $fail
