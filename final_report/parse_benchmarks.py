import sys
import re


def squash(list, a, b):
    list[a:b] = [sum(list[a:b])]
    return list


pattern = r'bw=([\d.]+)(M|K)iB/s'
speeds = []

mapper = {
    0: 'random reads',
    1: 'random writes',
    2: 'random both',
    3: 'sequential reads',
    4: 'sequential writes',
    5: 'sequential both',
    6: 'big file',
    7: 'metadata',

}

for path in sys.argv[1:]:
    with open(path) as file:
        res = re.findall(pattern, file.read())
        speeds.append(res)

result = []
for row in zip(*speeds):
    subsum = 0
    for item in row:
        subsum += float(item[0]) * (1024 if item[1] == 'M' else 1)

    result.append(subsum / len(row))

result = squash(result, 8, 10)
result = squash(result, 6, 8)
result = squash(result, 2, 4)

print(*list(zip(map(mapper.get, range(len(result))), map(round, result))), sep=' ')

# print(mapper.values())
