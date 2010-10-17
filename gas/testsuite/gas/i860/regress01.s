# Test fst.* with and without auto-increment.

	.text

        fst.l   %f16,-16(%sp)
        fst.l   %f20,-16(%sp)
        fst.l   %f24,-16(%sp)

        fst.l   %f16,-16(%sp)++
        fst.l   %f20,-16(%sp)++
        fst.l   %f24,-16(%sp)++

        fst.q   %f16,-16(%sp)
        fst.q   %f20,-16(%sp)
        fst.q   %f24,-16(%sp)

        fst.q   %f16,-16(%sp)++
        fst.q   %f20,-16(%sp)++
        fst.q   %f24,-16(%sp)++
