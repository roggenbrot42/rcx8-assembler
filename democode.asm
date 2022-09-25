#This is an example of a source code comment

.comment
"This is an example of a comment that will be inserted into the
compiled binary. The order in which segments (blocks) are inserted
into the binary is the order in which they appear in the source code.
Segment syntax is as follows: ^.{segment type}[address]$
The adress is optional, and can lead to conflicts.
Allowed segment types are: {.comment,.data,.input}.
The input segment acts as external .data segments copied from
an external file given by a second argument:
.input 0x0000 extern.asm
With this, the segment addresses of the external file are overwritten by the
input segment address (+ offset), when specified.

Address labels (variables) are defined by the regexp ^[a-Z][a-Z_-0-9]*:$
The 'main' label is special, as it marks the program start address and must exist

Instructions follow a segment label, one per line: ^([A-Z]{2,3}) ((R[0-7])|(0x[A-F0-9]{4})|([a-Z_-])){0,2}
The regexp for instructions is actually more complex, but you get the idea.
Strings and constants are defined in a segment as follows: 
ds ''string'' for 0 terminated string
dw 0xDEAD or dw 12345 or dw 0b1100110011011101
db 0xAB or db 127 or db ''string'' or db 0b11010010"

.data 0x0100
hello: ds "Hello World!"
db 0b11010010
db 0xAB
dw 0xDEAD
dw 65345

.data 0x0200
main:
    nop
    fim R0 0x00 #fill R0 and R1 with upper (^) and lower (?) bytes of hello address
    fim R1 0x01
print:
    fin R3
    ld R3
    jz end
    asc
    jun print
end:
    hlt