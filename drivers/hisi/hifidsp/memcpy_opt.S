    .global  memcpy128
    .global  memcpy64

memcpy128:
    add	x2, x0, x2
memcpy_align_128:
    ldp	x3, x4, [x1], #16
    stp	x3, x4, [x0], #16
    cmp	x0, x2
    b.ne	memcpy_align_128
    ret

memcpy64:
    add	x2, x0, x2
memcpy_align_64:
    ldr	x3, [x1], #8
    str	x3, [x0], #8
    cmp	x0, x2
    b.ne	memcpy_align_64
    ret

