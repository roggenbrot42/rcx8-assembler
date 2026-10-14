# Interactive demo: pick a text to read.
#   1 = The Hitchhiker's Guide to the Galaxy
#   2 = Goethe's Faust
#   3 = a joke
#   q = quit
# Any other key (including Enter) is ignored.
#
# Notes on the RCX8 quirks this relies on:
#  - INC does not carry into the next register, so a 16 bit pointer is
#    advanced with "inc R0 / ino R1" (INO increments R1 only on carry).
#  - XOR stores its result in the register, so "ld R2, xor R4, ld R4, jz"
#    tests R2 == R4.
#  - FIN reads the byte at address R1.R0 (R1 = MSB).
#  - KBI is line buffered in the emulator, Enter arrives as a key (0x0A).

.comment
"Menu demo: Hitchhiker's Guide, Faust or a joke."

.data 0x0100

menu_txt:
    db 10, "What would you like?", 10
    db "  1) The Hitchhiker's Guide to the Galaxy", 10
    db "  2) Goethe's Faust", 10
    db "  3) A joke", 10
    db "  q) Quit", 10
    ds "> "

hitch_txt:
    db 10, "From The Hitchhiker's Guide to the Galaxy:", 10
    db "  Space is big. You just won't believe how vastly, hugely,", 10
    db "  mind-bogglingly big it is. I mean, you may think it's a", 10
    db "  long way down the road to the chemist's, but that's just", 10
    db "  peanuts to space.", 10
    db "  -- and don't forget your towel.", 10
    ds ""

faust_txt:
    db 10, "From Faust (Johann Wolfgang von Goethe):", 10
    db "  Habe nun, ach! Philosophie,", 10
    db "  Juristerei und Medizin,", 10
    db "  Und leider auch Theologie!", 10
    db "  Durchaus studiert, mit heissem Bemuehn.", 10
    db "  Da steh ich nun, ich armer Tor!", 10
    db "  Und bin so klug als wie zuvor;", 10
    ds ""

joke_txt:
    db 10, "A joke:", 10
    db "  There are 10 kinds of people in the world:", 10
    db "  those who understand binary and those who don't.", 10
    ds ""

bye_txt:
    ds "Goodbye!"

.data 0x0400

# print: write the zero terminated string at R1.R0 (clobbers R0, R1, R3)
print:
    fin R3
    ld R3
    jz print_done
    asc
    inc R0
    ino R1
    jun print
print_done:
    bbl 0

main:
menu:
    fim R0 lo(menu_txt)
    fim R1 hi(menu_txt)
    jms print

readkey:
    kbi
    xch R2              # R2 = key

    fim R4 0x31         # '1'
    ld R2
    xor R4
    ld R4
    jz show_hitch

    fim R4 0x32         # '2'
    ld R2
    xor R4
    ld R4
    jz show_faust

    fim R4 0x33         # '3'
    ld R2
    xor R4
    ld R4
    jz show_joke

    fim R4 0x71         # 'q'
    ld R2
    xor R4
    ld R4
    jz quit

    fim R4 0xFF         # end of input
    ld R2
    xor R4
    ld R4
    jz quit

    jun readkey         # unknown key, ask again

show_hitch:
    fim R0 lo(hitch_txt)
    fim R1 hi(hitch_txt)
    jms print
    jun menu

show_faust:
    fim R0 lo(faust_txt)
    fim R1 hi(faust_txt)
    jms print
    jun menu

show_joke:
    fim R0 lo(joke_txt)
    fim R1 hi(joke_txt)
    jms print
    jun menu

quit:
    fim R0 lo(bye_txt)
    fim R1 hi(bye_txt)
    jms print
    ldm 10
    asc
    hlt
