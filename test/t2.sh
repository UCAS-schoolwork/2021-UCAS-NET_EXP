#str1='h1 echo "nameserver 8.8.8.8" > /etc/resolv.conf
#h1 wget www.baidu.com'
#echo -e $str1 | ./te1

./te1 << EOF
h1 echo "nameserver 8.8.8.8" > /etc/resolv.conf
h1 wget www.baidu.com
EOF