export function l $itos(w %i) {
@start
    %ca =l alloc4 10
    %c =w copy 0
    %rem =w rem %i, 10
    %rem =w add %rem, 48
    storeb %rem, %ca
    %i =w div %i, 10
    %c =w add %c, 1
    %cmp =w ceqw %i, 0
    jnz %cmp, @end, @loop

@loop
    %ca =l add %ca, 1
    %rem =w rem %i, 10
    %rem =w add %rem, 48
    storeb %rem, %ca
    %i =w div %i, 10
    %p =w add %i, 48
    %cmp =w ceqw %i, 0
    %c =w add %c, 1
    jnz %cmp, @end, @loop
@end
    %s =l call $malloc(w %c)
    %sb =l copy %s
@revstring
    %v =w loadub %ca
    storeb %v, %s
    %s =l add %s, 1
    %ca =l sub %ca, 1
    %c =w sub %c, 1
    %iz =w ceqw %c, 0
    jnz %iz, @revend, @revstring
@revend
    storeb 0, %s
    ret %sb
}
