data $ch = { b 0 }
data $nl = { b "\n\0" }

export function $dputs(l %s, w %fd) {
@start
@loop
    %ch =w loadub %s
    jnz %ch, @pbyte, @done
@pbyte
    storew %ch, $ch
    call $write(w %fd, l $ch, w 1)
    %s =l add %s, 1
    jmp @loop
@done
    call $write(w %fd, l $nl, w 1)
    ret
}
