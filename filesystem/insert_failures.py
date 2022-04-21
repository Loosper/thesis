import sys
import random

if len(sys.argv) < 2:
    print('Please specify device to insert errors into')
    sys.exit()


random.seed()

with open(sys.argv[1], 'r+b') as file:
    file.seek(0, 2)
    size = file.tell()
    nr_blocks = size // 4096

    for blk_nr in range(nr_blocks):
        bits_to_flip = random.randrange(0, 101)
        blk_off = blk_nr * 4096

        file.seek(blk_off)
        blk = bytearray(file.read(4096))

        for _ in range(bits_to_flip):
            bit = random.randrange(0, 4096)
            byte_nr = bit - (bit % 8)

            blk[byte_nr] = blk[byte_nr] ^ (bit % 8).to_bytes(1, 'big')[0]

        file.seek(blk_off)
        file.write(blk)
        file.flush()
