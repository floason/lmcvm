# LMC VM
I got bored, so I wrote a Little Man Computer interpreter. It's a model of a computer that is supposed to teach students. It's not really intended to be accurate, for instance its instruction set is extremely limited, and data values are three-digit in base10 (i.e. decimal).

I believe the original Little Man Computer specification does not address negative numbers by means of ten's complement, although this is a way of which you can implement LMC arithmetic. Some online simulators have odd undefined behaviour. Peter Higginson's has the accumulator range from -999 to 999 instead.