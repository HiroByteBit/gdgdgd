.data
a: .asciiz "HELLO"

.code
daddiu r4, r0, a
syscall 4
daddiu r4, r0, #10
syscall 11
syscall 10
