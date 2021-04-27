
def atof(str):
    sum = 0.0
    isrem = 0
    first = True
    for i in str:
        if i == ' ' and first:
            continue
        first = False
        add = ord(i) - ord('0')
        if add>=0 and add<=9:
            if isrem==0:
                sum = sum*10 + add
            else:
                sum += add/isrem
                isrem *= 10
        elif i=='.' and isrem==0:
            isrem = 10.0
        else:
            break
    return sum
str = '[  3] 0.00-0.18 sec  4.38 MBytes   204 Mbits/sec  35/0          0      395K/10881 us'
s1 = atof(str.split('Bytes  ')[1])
print(s1)
